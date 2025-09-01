/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCERT_MANAGER_H
#define BNCERT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_tls.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Certificate type enumeration (for legacy compatibility)
 */
typedef enum {
    BNCERT_TYPE_UNKNOWN = 0,        ///< Unknown or invalid certificate type
    BNCERT_TYPE_CA = 1,             ///< CA certificate for server verification
    BNCERT_TYPE_CLIENT = 2,         ///< Client certificate for client authentication
    BNCERT_TYPE_PRIVATE_KEY = 3     ///< Private key for client authentication
} bncert_type_t;

/**
 * @brief Certificate metadata structure (simplified)
 */
typedef struct {
    uint32_t address;               ///< Flash address where certificate is stored
    size_t size;                    ///< Size of certificate data
    bool in_use;                    ///< Whether this slot is occupied
} bncert_metadata_t;

/**
 * @brief Maximum number of certificates that can be managed
 */
#define BNCERT_MAX_CERTIFICATES 16

/**
 * @brief Certificate registry structure
 */
typedef struct {
    bncert_metadata_t certificates[BNCERT_MAX_CERTIFICATES];
    size_t count;                   ///< Number of registered certificates
    bool initialized;               ///< Manager initialization status
} bncert_registry_t;

/**
 * @brief Initialize certificate manager
 * 
 * Sets up the certificate manager and scans the certificate partition
 * for existing certificates.
 * 
 * @return true on success, false on failure
 */
bool bncert_manager_init(void);

/**
 * @brief Scan certificate partition for existing certificates
 * 
 * Scans the certificate partition at 4KB boundaries looking for valid
 * certificates and automatically registers them.
 * 
 * @return true on success, false on failure
 */
bool bncert_manager_scan_partition(void);

/**
 * @brief Estimate certificate size from header data
 * 
 * Analyzes certificate header to determine the full certificate size.
 * Supports both PEM and DER formats.
 * 
 * @param address Flash address where certificate starts
 * @param header Certificate header data (first 512 bytes)
 * @param header_size Size of header data
 * @return Estimated certificate size in bytes, or 0 if unable to determine
 */
size_t bncert_manager_estimate_cert_size(uint32_t address, const uint8_t *header, size_t header_size);

/**
 * @brief Find PEM certificate end marker
 * 
 * Searches for PEM end marker starting from given address to determine
 * the complete certificate size.
 * 
 * @param start_address Address where certificate begins
 * @param end_marker PEM end marker to search for (e.g. "-----END CERTIFICATE-----")
 * @return Certificate size including end marker, or 0 if not found
 */
size_t bncert_manager_find_pem_end(uint32_t start_address, const char *end_marker);

/**
 * @brief Deinitialize certificate manager
 * 
 * Cleans up certificate manager resources.
 */
void bncert_manager_deinit(void);

/**
 * @brief Register a certificate in the manager
 * 
 * Adds a certificate to the registry with metadata about its location
 * in the certificate partition.
 * 
 * @param address Flash address where certificate is stored
 * @param size Size of certificate data
 * @return true on success, false on failure
 */
bool bncert_manager_register(uint32_t address, size_t size);

/**
 * @brief Unregister a certificate from the manager
 * 
 * Removes a certificate from the registry.
 * 
 * @param address Flash address of certificate to unregister
 * @return true on success, false on failure
 */
bool bncert_manager_unregister(uint32_t address);

/**
 * @brief Load certificate data from partition
 * 
 * Reads certificate data from the certificate partition.
 * Caller is responsible for freeing the returned data.
 * 
 * @param address Flash address of certificate
 * @param size Size of certificate data
 * @param data_out Pointer to store allocated certificate data
 * @return true on success, false on failure
 */
bool bncert_manager_load_cert(uint32_t address, size_t size, uint8_t **data_out);

/**
 * @brief Configure TLS with available certificates
 * 
 * Automatically detects and configures available certificates for TLS.
 * Tries to identify certificate types by content and configures accordingly.
 * 
 * @param tls_cfg TLS configuration structure to populate
 * @return true if certificates were configured, false otherwise
 */
bool bncert_manager_configure_tls(esp_tls_cfg_t *tls_cfg);

/**
 * @brief Cleanup TLS configuration
 * 
 * Frees certificate data allocated during TLS configuration.
 * 
 * @param tls_cfg TLS configuration structure to cleanup
 */
void bncert_manager_cleanup_tls(esp_tls_cfg_t *tls_cfg);

/**
 * @brief List all registered certificates
 * 
 * Prints information about all certificates in the registry to the AT output.
 */
void bncert_manager_list_certificates(void);

/**
 * @brief Get certificate metadata by index
 * 
 * Retrieves certificate metadata from the registry by index.
 * 
 * @param index Index in the certificate registry
 * @param metadata_out Pointer to store certificate metadata
 * @return true if certificate exists at index, false otherwise
 */
bool bncert_manager_get_cert_by_index(size_t index, bncert_metadata_t *metadata_out);

/**
 * @brief Get total number of registered certificates
 * 
 * @return Number of certificates currently registered
 */
size_t bncert_manager_get_cert_count(void);

/**
 * @brief Automatically detect certificate type by content
 * 
 * Analyzes certificate data to determine if it's a CA certificate,
 * client certificate, or private key.
 * 
 * @param data Certificate data
 * @param size Size of certificate data
 * @return Detected certificate type (0=unknown, 1=CA/cert, 2=private key)
 */
int bncert_manager_detect_cert_type(const uint8_t *data, size_t size);

/**
 * @brief Validate certificate data format
 * 
 * Checks if certificate data is in valid PEM or DER format.
 * 
 * @param data Certificate data
 * @param size Size of certificate data
 * @return true if format is valid, false otherwise
 */
bool bncert_manager_validate_cert(const uint8_t *data, size_t size);

/**
 * @brief Get the first available certificate
 * 
 * Helper function to get any available certificate for quick TLS setup.
 * 
 * @param size_out Pointer to store certificate size
 * @return Pointer to certificate data (caller must free), or NULL if no certificates
 */
char* bncert_manager_get_first_certificate(size_t *size_out);

#ifdef __cplusplus
}
#endif

#endif // BNCERT_MANAGER_H
