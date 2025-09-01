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
#include "esp_at.h"
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

    // Scan partition for existing certificates
    if (!bncert_manager_scan_partition()) {
        ESP_LOGW(TAG, "Certificate partition scan failed, but manager is still functional");
    }

    return true;
}

bool bncert_manager_scan_partition(void)
{
    if (!s_cert_partition) {
        ESP_LOGE(TAG, "Certificate partition not available for scanning");
        return false;
    }

    ESP_LOGI(TAG, "Scanning certificate partition for existing certificates...");
    
    uint32_t partition_start = s_cert_partition->address;
    uint32_t partition_end = s_cert_partition->address + s_cert_partition->size;
    size_t certificates_found = 0;
    
    // Scan every 4KB boundary for certificates
    for (uint32_t addr = partition_start; addr < partition_end; addr += 0x1000) {
        // Read first 512 bytes to check for certificate headers
        const size_t header_size = 512;
        uint8_t header_buffer[header_size];
        
        uint32_t offset = addr - partition_start;
        esp_err_t err = esp_partition_read(s_cert_partition, offset, header_buffer, header_size);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "Failed to read from offset 0x%08X: %s", (unsigned int)offset, esp_err_to_name(err));
            continue;
        }
        
        // Check if this looks like a valid certificate
        if (!bncert_manager_validate_cert(header_buffer, header_size)) {
            continue;
        }
        
        // Try to determine the actual certificate size
        size_t cert_size = bncert_manager_estimate_cert_size(addr, header_buffer, header_size);
        if (cert_size == 0) {
            ESP_LOGD(TAG, "Could not determine certificate size at 0x%08X", (unsigned int)addr);
            continue;
        }
        
        // Register the found certificate
        if (bncert_manager_register(addr, cert_size)) {
            certificates_found++;
            ESP_LOGI(TAG, "Discovered certificate at 0x%08X (%u bytes)", 
                     (unsigned int)addr, (unsigned int)cert_size);
        } else {
            ESP_LOGW(TAG, "Failed to register discovered certificate at 0x%08X", (unsigned int)addr);
        }
    }
    
    ESP_LOGI(TAG, "Certificate partition scan complete: %u certificates found", 
             (unsigned int)certificates_found);
    
    return true;
}

size_t bncert_manager_estimate_cert_size(uint32_t address, const uint8_t *header, size_t header_size)
{
    if (!header || header_size == 0) {
        return 0;
    }
    
    // For PEM format, look for END marker
    if (header_size > 27 && memcmp(header, "-----BEGIN CERTIFICATE-----", 27) == 0) {
        return bncert_manager_find_pem_end(address, "-----END CERTIFICATE-----");
    }
    
    // Check for private key PEM formats
    const char *key_markers[][2] = {
        {"-----BEGIN PRIVATE KEY-----", "-----END PRIVATE KEY-----"},
        {"-----BEGIN RSA PRIVATE KEY-----", "-----END RSA PRIVATE KEY-----"},
        {"-----BEGIN EC PRIVATE KEY-----", "-----END EC PRIVATE KEY-----"}
    };
    
    for (int i = 0; i < 3; i++) {
        size_t begin_len = strlen(key_markers[i][0]);
        if (header_size > begin_len && memcmp(header, key_markers[i][0], begin_len) == 0) {
            return bncert_manager_find_pem_end(address, key_markers[i][1]);
        }
    }
    
    // For DER format, parse ASN.1 length
    if (header_size > 4 && header[0] == 0x30 && header[1] == 0x82) {
        // DER encoded certificate: 0x30 0x82 [length-high] [length-low]
        size_t der_length = (header[2] << 8) | header[3];
        return der_length + 4; // Include the header
    }
    
    ESP_LOGD(TAG, "Could not estimate certificate size for unknown format");
    return 0;
}

size_t bncert_manager_find_pem_end(uint32_t start_address, const char *end_marker)
{
    if (!s_cert_partition || !end_marker) {
        return 0;
    }
    
    const size_t chunk_size = 1024;
    const size_t max_cert_size = 65536; // Maximum certificate size to search
    size_t end_marker_len = strlen(end_marker);
    uint32_t partition_start = s_cert_partition->address;
    uint32_t partition_end = s_cert_partition->address + s_cert_partition->size;
    
    // Search for end marker in chunks
    for (size_t offset = 0; offset < max_cert_size; offset += chunk_size - end_marker_len) {
        uint32_t read_addr = start_address + offset;
        
        // Don't read beyond partition boundary
        if (read_addr >= partition_end) {
            break;
        }
        
        size_t read_size = chunk_size;
        if (read_addr + read_size > partition_end) {
            read_size = partition_end - read_addr;
        }
        
        uint8_t *chunk = malloc(read_size);
        if (!chunk) {
            ESP_LOGE(TAG, "Failed to allocate memory for PEM end search");
            return 0;
        }
        
        uint32_t partition_offset = read_addr - partition_start;
        esp_err_t err = esp_partition_read(s_cert_partition, partition_offset, chunk, read_size);
        if (err != ESP_OK) {
            free(chunk);
            ESP_LOGD(TAG, "Failed to read chunk at offset %u: %s", 
                     (unsigned int)partition_offset, esp_err_to_name(err));
            return 0;
        }
        
        // Search for end marker in this chunk
        for (size_t i = 0; i <= read_size - end_marker_len; i++) {
            if (memcmp(chunk + i, end_marker, end_marker_len) == 0) {
                // Found end marker, calculate total size
                size_t total_size = offset + i + end_marker_len;
                
                // Add newline if present
                if (i + end_marker_len < read_size && chunk[i + end_marker_len] == '\n') {
                    total_size++;
                }
                
                free(chunk);
                ESP_LOGD(TAG, "Found PEM end marker, certificate size: %u bytes", (unsigned int)total_size);
                return total_size;
            }
        }
        
        free(chunk);
    }
    
    ESP_LOGD(TAG, "PEM end marker not found within %u bytes", (unsigned int)max_cert_size);
    return 0;
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

bool bncert_manager_clear_cert(uint32_t address)
{
    if (!s_cert_registry.initialized) {
        ESP_LOGE(TAG, "Certificate manager not initialized");
        return false;
    }

    if (!s_cert_partition) {
        ESP_LOGE(TAG, "Certificate partition not available");
        return false;
    }

    // Check if address is within partition bounds and 4KB aligned
    uint32_t partition_start = s_cert_partition->address;
    uint32_t partition_end = s_cert_partition->address + s_cert_partition->size;
    
    if (address < partition_start || address >= partition_end) {
        ESP_LOGE(TAG, "Address 0x%08X outside certificate partition bounds", (unsigned int)address);
        return false;
    }
    
    if (address % 0x1000 != 0) {
        ESP_LOGE(TAG, "Address 0x%08X not 4KB aligned", (unsigned int)address);
        return false;
    }

    // Unregister from manager first
    bool was_registered = bncert_manager_unregister(address);
    
    // Erase the 4KB sector at this address
    uint32_t offset = address - partition_start;
    esp_err_t err = esp_partition_erase_range(s_cert_partition, offset, 0x1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase certificate at 0x%08X: %s", 
                 (unsigned int)address, esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Certificate at 0x%08X cleared (erased 4KB)", (unsigned int)address);
    
    // Reload all certificates to update registry
    bncert_manager_reload_certificates();
    
    return true;
}

void bncert_manager_reload_certificates(void)
{
    if (!s_cert_registry.initialized) {
        ESP_LOGW(TAG, "Certificate manager not initialized");
        return;
    }

    ESP_LOGI(TAG, "Reloading all certificates from partition");
    
    // Clear current registry but keep initialized flag
    size_t old_count = s_cert_registry.count;
    memset(s_cert_registry.certificates, 0, sizeof(s_cert_registry.certificates));
    s_cert_registry.count = 0;
    
    // Rescan partition for certificates
    bncert_manager_scan_partition();
    
    ESP_LOGI(TAG, "Certificate reload complete: %u certificates (was %u)", 
             (unsigned int)s_cert_registry.count, (unsigned int)old_count);
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
    
    // Print certificate content summary for debugging
    const char *cert_type = "UNKNOWN";
    if (size >= 27 && memcmp(buffer, "-----BEGIN CERTIFICATE-----", 27) == 0) {
        cert_type = "X.509 Certificate";
    } else if (size >= 28 && memcmp(buffer, "-----BEGIN PRIVATE KEY-----", 28) == 0) {
        cert_type = "Private Key (PKCS#8)";
    } else if (size >= 32 && memcmp(buffer, "-----BEGIN RSA PRIVATE KEY-----", 32) == 0) {
        cert_type = "RSA Private Key";
    } else if (size >= 31 && memcmp(buffer, "-----BEGIN EC PRIVATE KEY-----", 31) == 0) {
        cert_type = "EC Private Key";
    } else if (size >= 4 && buffer[0] == 0x30 && buffer[1] == 0x82) {
        cert_type = "DER Format";
    }
    
    printf("BNCERT_MGR: Certificate loaded from flash at 0x%08X (%u bytes) - Type: %s\n", 
           (unsigned int)address, (unsigned int)size, cert_type);
    
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
        esp_at_port_write_data((uint8_t *)"ERROR: Certificate manager not initialized\r\n", 45);
        return;
    }

    char response_buffer[128];
    int len = snprintf(response_buffer, sizeof(response_buffer),
                      "+BNCERT_LIST:%u,%u\r\n", 
                      (unsigned int)s_cert_registry.count, 
                      BNCERT_MAX_CERTIFICATES);
    esp_at_port_write_data((uint8_t *)response_buffer, len);
    
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
            
            len = snprintf(response_buffer, sizeof(response_buffer),
                          "+BNCERT_ENTRY:0x%08X,%u,\"%s\"\r\n",
                          (unsigned int)cert->address,
                          (unsigned int)cert->size,
                          type_name);
            esp_at_port_write_data((uint8_t *)response_buffer, len);
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

    // Strict validation - certificate MUST start with proper markers
    // No garbage characters allowed at the beginning
    
    // Check for PEM certificate format (must start exactly with the marker)
    if (size >= 27 && memcmp(data, "-----BEGIN CERTIFICATE-----", 27) == 0) {
        ESP_LOGD(TAG, "Detected PEM certificate format");
        return true;
    }
    
    // Check for PEM private key formats (must start exactly with the marker)
    const char *pem_markers[] = {
        "-----BEGIN PRIVATE KEY-----",
        "-----BEGIN RSA PRIVATE KEY-----",
        "-----BEGIN EC PRIVATE KEY-----"
    };
    
    for (int i = 0; i < 3; i++) {
        size_t marker_len = strlen(pem_markers[i]);
        if (size >= marker_len && memcmp(data, pem_markers[i], marker_len) == 0) {
            ESP_LOGD(TAG, "Detected PEM private key format: %s", pem_markers[i]);
            return true;
        }
    }
    
    // Check for DER format (X.509 certificates start with 0x30)
    if (size >= 4 && data[0] == 0x30 && data[1] == 0x82) {
        ESP_LOGD(TAG, "Detected DER certificate/key format");
        return true;
    }

    // Log the first few bytes to help with debugging
    char debug_str[32] = {0};
    size_t debug_len = size < 15 ? size : 15;
    for (size_t i = 0; i < debug_len; i++) {
        if (data[i] >= 32 && data[i] <= 126) {
            debug_str[i] = data[i];
        } else {
            debug_str[i] = '.';
        }
    }
    debug_str[debug_len] = '\0';
    
    ESP_LOGW(TAG, "Certificate validation failed - invalid format. First %u bytes: '%s'", 
             (unsigned int)debug_len, debug_str);
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
