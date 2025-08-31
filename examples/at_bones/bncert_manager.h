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
 * @return true on success, false on failure
 */
bool bncert_manager_register(uint32_t address, size_t size, bncert_type_t type, const char *name);

/**
 * @brief Unregister a certificate from the manager
 * 
 * Removes a certificate from the registry (does not erase from flash).
 * 
 * @param address Flash address of certificate to unregister
 * @return true on success, false if not found
 */
bool bncert_manager_unregister(uint32_t address);

/**
 * @brief Load certificate data from partition
 * 
 * Reads certificate data from the certificate partition into a buffer.
 * The caller is responsible for freeing the returned buffer.
 * 
 * @param address Flash address of certificate
 * @param size Expected size of certificate
 * @param data_out Pointer to receive allocated buffer with certificate data
 * @return true on success, false on failure
 */
bool bncert_manager_load_cert(uint32_t address, size_t size, uint8_t **data_out);

/**
 * @brief Find certificate by type
 * 
 * Searches the registry for the first certificate of the specified type.
 * 
 * @param type Certificate type to search for
 * @param metadata_out Pointer to receive certificate metadata
 * @return true if found, false if not found
 */
bool bncert_manager_find_by_type(bncert_type_t type, bncert_metadata_t *metadata_out);

/**
 * @brief Find certificate by name
 * 
 * Searches the registry for a certificate with the specified name.
 * 
 * @param name Certificate name to search for
 * @param metadata_out Pointer to receive certificate metadata
 * @return true if found, false if not found
 */
bool bncert_manager_find_by_name(const char *name, bncert_metadata_t *metadata_out);

/**
 * @brief Configure TLS context with certificates from partition
 * 
 * Sets up an esp_tls_cfg_t structure with certificates loaded from the
 * certificate partition. Partition certificates take precedence over
 * hardcoded certificates.
 * 
 * @param tls_cfg TLS configuration structure to modify
 * @param use_client_cert Whether to include client certificate/key for mutual TLS
 * @return true on success, false on failure
 */
bool bncert_manager_configure_tls(esp_tls_cfg_t *tls_cfg, bool use_client_cert);

/**
 * @brief Clean up TLS configuration allocated by manager
 * 
 * Frees any memory allocated by bncert_manager_configure_tls().
 * 
 * @param tls_cfg TLS configuration to clean up
 */
void bncert_manager_cleanup_tls(esp_tls_cfg_t *tls_cfg);

/**
 * @brief List all registered certificates
 * 
 * Prints information about all certificates in the registry to the console.
 */
void bncert_manager_list_certificates(void);

/**
 * @brief Get certificate type name
 * 
 * Returns a human-readable name for the certificate type.
 * 
 * @param type Certificate type
 * @return Pointer to type name string
 */
const char* bncert_manager_get_type_name(bncert_type_t type);

/**
 * @brief Validate certificate format
 * 
 * Performs basic validation on certificate data to ensure it's properly formatted.
 * 
 * @param data Certificate data buffer
 * @param size Size of certificate data
 * @param type Expected certificate type
 * @return true if valid, false if invalid
 */
bool bncert_manager_validate_cert(const uint8_t *data, size_t size, bncert_type_t type);

#ifdef __cplusplus
}
#endif

#endif // BNCERT_MANAGER_H
