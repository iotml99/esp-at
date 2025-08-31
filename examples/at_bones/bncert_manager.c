/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncert_manager.h"
#include "bncert.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_tls.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "BNCERT_MGR";

// Global certificate registry
static bncert_registry_t s_cert_registry = {0};

// Certificate partition handle
static const esp_partition_t *s_cert_partition = NULL;

bool bncert_manager_init(void)
{
    if (s_cert_registry.initialized) {
        ESP_LOGW(TAG, "Certificate manager already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing certificate manager");

    // Find certificate partition
    s_cert_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, NULL);
    if (!s_cert_partition) {
        ESP_LOGE(TAG, "Certificate partition not found");
        return false;
    }

    // Initialize registry
    memset(&s_cert_registry, 0, sizeof(s_cert_registry));
    s_cert_registry.initialized = true;

    ESP_LOGI(TAG, "Certificate manager initialized with partition at 0x%08X (%u bytes)",
             (unsigned int)s_cert_partition->address, (unsigned int)s_cert_partition->size);

    return true;
}

void bncert_manager_deinit(void)
{
    if (!s_cert_registry.initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing certificate manager");
    
    // Clear registry
    memset(&s_cert_registry, 0, sizeof(s_cert_registry));
    s_cert_partition = NULL;
}

bool bncert_manager_register(uint32_t address, size_t size)
{
    if (!s_cert_registry.initialized) {
        ESP_LOGE(TAG, "Certificate manager not initialized");
        return false;
    }

    // Check if registry is full
    if (s_cert_registry.count >= BNCERT_MAX_CERTIFICATES) {
        ESP_LOGE(TAG, "Certificate registry full (max %d certificates)", BNCERT_MAX_CERTIFICATES);
        return false;
    }

    // Check if address already registered
    for (size_t i = 0; i < BNCERT_MAX_CERTIFICATES; i++) {
        if (s_cert_registry.certificates[i].in_use && 
            s_cert_registry.certificates[i].address == address) {
            ESP_LOGW(TAG, "Certificate at address 0x%08X already registered", 
                     (unsigned int)address);
            return false;
        }
    }

    // Find empty slot
    for (size_t i = 0; i < BNCERT_MAX_CERTIFICATES; i++) {
        if (!s_cert_registry.certificates[i].in_use) {
            bncert_metadata_t *cert = &s_cert_registry.certificates[i];
            
            cert->address = address;
            cert->size = size;
            cert->in_use = true;
            
            s_cert_registry.count++;
            
            ESP_LOGI(TAG, "Registered certificate at 0x%08X (%u bytes)",
                     (unsigned int)address, (unsigned int)size);
            
            return true;
        }
    }

    ESP_LOGE(TAG, "No free slots in certificate registry");
    return false;
}

bool bncert_manager_unregister(uint32_t address)
{
    if (!s_cert_registry.initialized) {
        ESP_LOGE(TAG, "Certificate manager not initialized");
        return false;
    }

    for (size_t i = 0; i < BNCERT_MAX_CERTIFICATES; i++) {
        if (s_cert_registry.certificates[i].in_use && 
            s_cert_registry.certificates[i].address == address) {
            
            ESP_LOGI(TAG, "Unregistering certificate at 0x%08X (%u bytes)",
                     (unsigned int)address, (unsigned int)s_cert_registry.certificates[i].size);
            
            memset(&s_cert_registry.certificates[i], 0, sizeof(bncert_metadata_t));
            s_cert_registry.count--;
            
            return true;
        }
    }

    ESP_LOGW(TAG, "Certificate at address 0x%08X not found in registry", 
             (unsigned int)address);
    return false;
}

bool bncert_manager_load_cert(uint32_t address, size_t size, uint8_t **data_out)
{
    if (!s_cert_partition || !data_out) {
        ESP_LOGE(TAG, "Invalid parameters for certificate loading");
        return false;
    }

    // Calculate offset within partition
    if (address < s_cert_partition->address || 
        address + size > s_cert_partition->address + s_cert_partition->size) {
        ESP_LOGE(TAG, "Certificate address 0x%08X outside partition bounds", 
                 (unsigned int)address);
        return false;
    }

    uint32_t offset = address - s_cert_partition->address;

    // Allocate buffer
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for certificate", (unsigned int)size);
        return false;
    }

    // Read certificate data
    esp_err_t err = esp_partition_read(s_cert_partition, offset, buffer, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read certificate from partition: %s", esp_err_to_name(err));
        free(buffer);
        return false;
    }

    *data_out = buffer;
    ESP_LOGI(TAG, "Loaded certificate from 0x%08X (%u bytes)", 
             (unsigned int)address, (unsigned int)size);
    
    return true;
}

int bncert_manager_detect_cert_type(const uint8_t *data, size_t size)
{
    if (!data || size == 0) {
        return 0; // Unknown
    }

    // Check for private key formats first (more specific)
    const char *key_markers[] = {
        "-----BEGIN PRIVATE KEY-----",
        "-----BEGIN RSA PRIVATE KEY-----",
        "-----BEGIN EC PRIVATE KEY-----"
    };
    
    for (int i = 0; i < 3; i++) {
        size_t marker_len = strlen(key_markers[i]);
        if (size > marker_len && memcmp(data, key_markers[i], marker_len) == 0) {
            ESP_LOGD(TAG, "Detected private key format");
            return 2; // Private key
        }
    }
    
    // Check for certificate formats
    if (size > 27 && memcmp(data, "-----BEGIN CERTIFICATE-----", 27) == 0) {
        ESP_LOGD(TAG, "Detected PEM certificate format");
        return 1; // Certificate
    }
    
    // Check for DER format (X.509 certificates start with 0x30)
    if (size > 4 && data[0] == 0x30 && data[1] == 0x82) {
        ESP_LOGD(TAG, "Detected DER certificate/key format");
        return 1; // Assume certificate for DER format
    }

    ESP_LOGW(TAG, "Certificate type detection failed - unrecognized format");
    return 0; // Unknown
}

bool bncert_manager_configure_tls(esp_tls_cfg_t *tls_cfg)
{
    if (!s_cert_registry.initialized || !tls_cfg) {
        ESP_LOGE(TAG, "Invalid parameters for TLS configuration");
        return false;
    }

    ESP_LOGI(TAG, "Configuring TLS with certificates from partition");

    bool configured = false;

    // Scan all registered certificates and classify them
    for (size_t i = 0; i < BNCERT_MAX_CERTIFICATES; i++) {
        if (!s_cert_registry.certificates[i].in_use) {
            continue;
        }

        bncert_metadata_t *cert = &s_cert_registry.certificates[i];
        uint8_t *cert_data = NULL;
        
        if (!bncert_manager_load_cert(cert->address, cert->size, &cert_data)) {
            ESP_LOGW(TAG, "Failed to load certificate at 0x%08X", (unsigned int)cert->address);
            continue;
        }

        // Validate certificate format
        if (!bncert_manager_validate_cert(cert_data, cert->size)) {
            ESP_LOGW(TAG, "Invalid certificate format at 0x%08X", (unsigned int)cert->address);
            free(cert_data);
            continue;
        }

        // Detect certificate type
        int cert_type = bncert_manager_detect_cert_type(cert_data, cert->size);
        
        if (cert_type == 1 && !tls_cfg->cacert_buf) {
            // Use first certificate found as CA certificate
            tls_cfg->cacert_buf = cert_data;
            tls_cfg->cacert_bytes = cert->size;
            
            ESP_LOGI(TAG, "Configured CA certificate from partition (%u bytes)", 
                     (unsigned int)cert->size);
            configured = true;
        } else if (cert_type == 1 && tls_cfg->cacert_buf && !tls_cfg->clientcert_buf) {
            // Use second certificate as client certificate
            tls_cfg->clientcert_buf = cert_data;
            tls_cfg->clientcert_bytes = cert->size;
            
            ESP_LOGI(TAG, "Configured client certificate from partition (%u bytes)", 
                     (unsigned int)cert->size);
            configured = true;
        } else if (cert_type == 2 && !tls_cfg->clientkey_buf) {
            // Use private key
            tls_cfg->clientkey_buf = cert_data;
            tls_cfg->clientkey_bytes = cert->size;
            
            ESP_LOGI(TAG, "Configured client key from partition (%u bytes)", 
                     (unsigned int)cert->size);
            configured = true;
        } else {
            // Not needed or duplicate
            free(cert_data);
        }
    }

    return configured;
}

void bncert_manager_cleanup_tls(esp_tls_cfg_t *tls_cfg)
{
    if (!tls_cfg) {
        return;
    }

    // Free CA certificate buffer if allocated by manager
    if (tls_cfg->cacert_buf) {
        free((void*)tls_cfg->cacert_buf);
        tls_cfg->cacert_buf = NULL;
        tls_cfg->cacert_bytes = 0;
    }

    // Free client certificate buffer if allocated by manager
    if (tls_cfg->clientcert_buf) {
        free((void*)tls_cfg->clientcert_buf);
        tls_cfg->clientcert_buf = NULL;
        tls_cfg->clientcert_bytes = 0;
    }

    // Free client key buffer if allocated by manager
    if (tls_cfg->clientkey_buf) {
        free((void*)tls_cfg->clientkey_buf);
        tls_cfg->clientkey_buf = NULL;
        tls_cfg->clientkey_bytes = 0;
    }
}

void bncert_manager_list_certificates(void)
{
    if (!s_cert_registry.initialized) {
        printf("Certificate manager not initialized\n");
        return;
    }

    printf("+BNCERT_LIST:%u,%u\r\n", (unsigned int)s_cert_registry.count, BNCERT_MAX_CERTIFICATES);
    
    for (size_t i = 0; i < BNCERT_MAX_CERTIFICATES; i++) {
        if (s_cert_registry.certificates[i].in_use) {
            bncert_metadata_t *cert = &s_cert_registry.certificates[i];
            
            // Try to detect certificate type for display
            uint8_t *cert_data = NULL;
            const char *type_name = "UNKNOWN";
            
            if (bncert_manager_load_cert(cert->address, cert->size, &cert_data)) {
                int cert_type = bncert_manager_detect_cert_type(cert_data, cert->size);
                switch (cert_type) {
                    case 1: type_name = "CERTIFICATE"; break;
                    case 2: type_name = "PRIVATE_KEY"; break;
                    default: type_name = "UNKNOWN"; break;
                }
                free(cert_data);
            }
            
            printf("+BNCERT_ENTRY:0x%08X,%u,\"%s\"\r\n",
                   (unsigned int)cert->address,
                   (unsigned int)cert->size,
                   type_name);
        }
    }
}

bool bncert_manager_get_cert_by_index(size_t index, bncert_metadata_t *metadata_out)
{
    if (!s_cert_registry.initialized || !metadata_out || index >= BNCERT_MAX_CERTIFICATES) {
        return false;
    }

    if (s_cert_registry.certificates[index].in_use) {
        memcpy(metadata_out, &s_cert_registry.certificates[index], sizeof(bncert_metadata_t));
        return true;
    }

    return false;
}

size_t bncert_manager_get_cert_count(void)
{
    if (!s_cert_registry.initialized) {
        return 0;
    }
    
    return s_cert_registry.count;
}

bool bncert_manager_validate_cert(const uint8_t *data, size_t size)
{
    if (!data || size == 0) {
        return false;
    }

    // Basic validation - check for common certificate/key markers
    
    // Check for PEM certificate format
    if (size > 27 && memcmp(data, "-----BEGIN CERTIFICATE-----", 27) == 0) {
        ESP_LOGD(TAG, "Detected PEM certificate format");
        return true;
    }
    
    // Check for PEM private key formats
    const char *pem_markers[] = {
        "-----BEGIN PRIVATE KEY-----",
        "-----BEGIN RSA PRIVATE KEY-----",
        "-----BEGIN EC PRIVATE KEY-----"
    };
    
    for (int i = 0; i < 3; i++) {
        size_t marker_len = strlen(pem_markers[i]);
        if (size > marker_len && memcmp(data, pem_markers[i], marker_len) == 0) {
            ESP_LOGD(TAG, "Detected PEM private key format");
            return true;
        }
    }
    
    // Check for DER format (X.509 certificates start with 0x30)
    if (size > 4 && data[0] == 0x30 && data[1] == 0x82) {
        ESP_LOGD(TAG, "Detected DER certificate/key format");
        return true;
    }

    ESP_LOGW(TAG, "Certificate validation failed - unrecognized format");
    return false;
}

char* bncert_manager_get_first_certificate(size_t *size_out)
{
    if (!size_out) {
        ESP_LOGE(TAG, "Invalid size_out parameter");
        return NULL;
    }

    *size_out = 0;

    if (!s_cert_registry.initialized) {
        ESP_LOGW(TAG, "Certificate manager not initialized");
        return NULL;
    }

    if (s_cert_registry.count == 0) {
        ESP_LOGD(TAG, "No certificates available");
        return NULL;
    }

    // Find the first certificate in the registry
    for (size_t i = 0; i < BNCERT_MAX_CERTIFICATES; i++) {
        if (s_cert_registry.certificates[i].in_use) {
            uint8_t *cert_data = NULL;
            bool success = bncert_manager_load_cert(
                s_cert_registry.certificates[i].address,
                s_cert_registry.certificates[i].size,
                &cert_data
            );

            if (success && cert_data) {
                *size_out = s_cert_registry.certificates[i].size;
                ESP_LOGI(TAG, "Retrieved first certificate: %u bytes from address 0x%08X", 
                         (unsigned int)*size_out, (unsigned int)s_cert_registry.certificates[i].address);
                return (char*)cert_data;
            } else {
                ESP_LOGW(TAG, "Failed to load certificate at index %u", (unsigned int)i);
            }
        }
    }

    ESP_LOGW(TAG, "No valid certificates found");
    return NULL;
}
