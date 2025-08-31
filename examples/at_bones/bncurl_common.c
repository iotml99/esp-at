/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_common.h"
#include "bncurl_methods.h"
#include "bncurl_config.h"
#include "bncurl_cookies.h"
#include "bncert_manager.h"
#include "bnkill.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "esp_at.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BNCURL_COMMON";

// Combined header callback that handles both content-length and cookies
size_t bncurl_combined_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)userdata;
    size_t total_size = size * nitems;
    
    // First handle regular header processing (content-length, HEAD streaming, etc.)
    size_t result = bncurl_common_header_callback(buffer, size, nitems, userdata);
    
    // Then handle Date header for kill switch (if not already captured)
    if (common_ctx && !common_ctx->http_date_header) {
        if (strncasecmp(buffer, "Date:", 5) == 0) {
            char *date_start = buffer + 5;
            while (*date_start == ' ' || *date_start == '\t') date_start++; // Skip whitespace
            
            // Create null-terminated date string
            size_t date_len = total_size - (date_start - buffer);
            
            // Remove trailing CRLF
            while (date_len > 0 && (date_start[date_len-1] == '\r' || date_start[date_len-1] == '\n')) {
                date_len--;
            }
            
            if (date_len > 0) {
                common_ctx->http_date_header = malloc(date_len + 1);
                if (common_ctx->http_date_header) {
                    memcpy(common_ctx->http_date_header, date_start, date_len);
                    common_ctx->http_date_header[date_len] = '\0';
                    ESP_LOGI(TAG, "Captured HTTP Date header: %s", common_ctx->http_date_header);
                }
            }
        }
    }
    
    // Then handle cookie processing if cookie context is available
    if (common_ctx && common_ctx->cookies) {
        // Look for Set-Cookie headers
        if (strncasecmp(buffer, "Set-Cookie:", 11) == 0) {
            char *cookie_start = buffer + 11;
            while (*cookie_start == ' ' || *cookie_start == '\t') cookie_start++; // Skip whitespace
            
            // Create null-terminated cookie string
            char cookie_string[512];
            size_t cookie_len = total_size - (cookie_start - buffer);
            if (cookie_len > sizeof(cookie_string) - 1) {
                cookie_len = sizeof(cookie_string) - 1;
            }
            
            memcpy(cookie_string, cookie_start, cookie_len);
            cookie_string[cookie_len] = '\0';
            
            // Remove trailing CRLF
            char *end = cookie_string + strlen(cookie_string) - 1;
            while (end >= cookie_string && (*end == '\r' || *end == '\n')) {
                *end = '\0';
                end--;
            }
            
            if (strlen(cookie_string) > 0) {
                ESP_LOGI(TAG, "Received Set-Cookie: %s", cookie_string);
                bncurl_cookies_parse_and_add(common_ctx->cookies, cookie_string);
            }
        }
    }
    
    return result;
}

/* Hardcoded CA bundle for HTTPS support */
static const char CA_BUNDLE_PEM[] =
/* Amazon Root CA 1 - for AWS/Amazon services */
"-----BEGIN CERTIFICATE-----\n"
"MIIEkjCCA3qgAwIBAgITBn+USionzfP6wq4rAfkI7rnExjANBgkqhkiG9w0BAQsF"
"ADCBmDELMAkGA1UEBhMCVVMxEDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNj"
"b3R0c2RhbGUxJTAjBgNVBAoTHFN0YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4x"
"OzA5BgNVBAMTMlN0YXJmaWVsZCBTZXJ2aWNlcyBSb290IENlcnRpZmljYXRlIEF1"
"dGhvcml0eSAtIEcyMB4XDTE1MDUyNTEyMDAwMFoXDTM3MTIzMTAxMDAwMFowOTEL"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv"
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj"
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM"
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw"
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6"
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L"
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm"
"jgSubJrIqg0CAwEAAaOCATEwggEtMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/"
"BAQDAgGGMB0GA1UdDgQWBBSEGMyFNOy8DJSULghZnMeyEE4KCDAfBgNVHSMEGDAW"
"gBScXwDfqgHXMCs4iKK4bUqc8hGRgzB4BggrBgEFBQcBAQRsMGowLgYIKwYBBQUH"
"MAGGImh0dHA6Ly9vY3NwLnJvb3RnMi5hbWF6b250cnVzdC5jb20wOAYIKwYBBQUH"
"MAKGLGh0dHA6Ly9jcnQucm9vdGcyLmFtYXpvbnRydXN0LmNvbS9yb290ZzIuY2Vy"
"MD0GA1UdHwQ2MDQwMqAwoC6GLGh0dHA6Ly9jcmwucm9vdGcyLmFtYXpvbnRydXN0"
"LmNvbS9yb290ZzIuY3JsMBEGA1UdIAQKMAgwBgYEVR0gADANBgkqhkiG9w0BAQsF"
"AAOCAQEAYjdCXLwQtT6LLOkMm2xF4gcAevnFWAu5CIw+7bMlPLVvUOTNNWqnkzSW"
"MiGpSESrnO09tKpzbeR/FoCJbM8oAxiDR3mjEH4wW6w7sGDgd9QIpuEdfF7Au/ma"
"eyKdpwAJfqxGF4PcnCZXmTA5YpaP7dreqsXMGz7KQ2hsVxa81Q4gLv7/wmpdLqBK"
"bRRYh5TmOTFffHPLkIhqhBGWJ6bt2YFGpn6jcgAKUj6DiAdjd4lpFw85hdKrCEVN"
"0FE6/V1dN2RMfjCyVSRCnTawXZwXgWHxyvkQAiSr6w10kY17RSlQOYiypok1JR4U"
"akcjMS9cmvqtmg5iUaQqqcT5NJ0hGA==\n"
"-----END CERTIFICATE-----\n"
/* ISRG Root X1 - Let's Encrypt root for most modern sites */
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n"
/* DigiCert Global Root G2 - for many commercial sites */
"-----BEGIN CERTIFICATE-----\n"
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADAi"
"MQswCQYDVQQGEwJVUzEZMBcGA1UEChMQRGlnaUNlcnQgSW5jIDEYMBYGA1UEAxMP"
"RGlnaUNlcnQgR2xvYmFsIFJvb3QgRzIwHhcNMTMwODAxMTIwMDAwWhcNMzgwMTE1"
"MTIwMDAwWjAiMQswCQYDVQQGEwJVUzEZMBcGA1UEChMQRGlnaUNlcnQgSW5jIDEY"
"MBYGA1UEAxMPRGlnaUNlcnQgR2xvYmFsIFJvb3QgRzIwggEiMA0GCSqGSIb3DQEB"
"AQUAA4IBDwAwggEKAoIBAQDiO+ERct6opNOjV6pQoo8Ld5DJoqXuEs6WWwEJIMwT"
"L6cpt7tkTU7wgWa6TiQhExcL8VhJLmB8nrCgKX2Rku0QAZmrCIEOY+EQp7LYjQGX"
"oc5YI4KyBT9EIaFHVgfq4zJgOVL0fdRs2uS1EuGvPW4+CAAamrCv3V/Nwi0Ixkm1"
"z2G4Kw4PdFKdXhH1+xN/3IzMSGOjKf5d2YmZiVzB+y/w/xHx1VcOdUlgZXhm6tI="
"-----END CERTIFICATE-----\n"
/* Baltimore CyberTrust Root - used by many Microsoft and other services */
"-----BEGIN CERTIFICATE-----\n"
"MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ"
"RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD"
"VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX"
"DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y"
"ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy"
"VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr"
"mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr"
"IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK"
"mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu"
"XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy"
"dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye"
"jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1"
"BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3"
"DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92"
"9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx"
"jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0"
"Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz"
"ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS"
"R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n"
"-----END CERTIFICATE-----\n"
/* Cloudflare Inc ECC CA-3 - for Cloudflare CDN sites like httpbin.org */
"-----BEGIN CERTIFICATE-----\n"
"MIIBljCCATygAwIBAgIQC5McOtY5Z+pnI7/Dr5r0SzAKBggqhkjOPQQDAjAmMQsw"
"CQYDVQQGEwJVUzEXMBUGA1UEChMOQ2xvdWRmbGFyZSwgSW5jLjAeFw0yMDEyMDMy"
"MzAwMDBaFw0zNTEyMDIyMzAwMDBaMCYxCzAJBgNVBAYTAlVTMRcwFQYDVQQKEw5D"
"bG91ZGZsYXJlLCBJbmMuMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEua1NZpkU"
"DaTGsb5+yrg7FkAsVjNrKh/lqnrqgf7kO4hXfbXVAv+5VdJ9P4FpXDdpJe7zEINb"
"1QKCCLOCqKO4faGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTADAQH/"
"MB0GA1UdDgQWBBSlzjfq67B1DpRniLRF+tkkEIeWHzAKBggqhkjOPQQDAgNIADBF"
"AiEAiZQb1gODuHNyZNkD2G2ByEQjW2p9cLbvv5dAE5wG5CgCIGV+HgAl0xRgJrW8"
"xP9x+nOgvv4U+2nfAM7S4/J8ydnl\n"
"-----END CERTIFICATE-----\n"
/* GeoTrust Global CA - widely used CA for many sites */
"-----BEGIN CERTIFICATE-----\n"
"MIIDVDCCAjygAwIBAgIDAjRWMA0GCSqGSIb3DQEBBQUAMEIxCzAJBgNVBAYTAlVT"
"MRYwFAYDVQQKEw1HZW9UcnVzdCBJbmMuMRswGQYDVQQDExJHZW9UcnVzdCBHbG9i"
"YWwgQ0EwHhcNMDIwNTIxMDQwMDAwWhcNMjIwNTIxMDQwMDAwWjBCMQswCQYDVQQG"
"EwJVUzEWMBQGA1UEChMNR2VvVHJ1c3QgSW5jLjEbMBkGA1UEAxMSR2VvVHJ1c3Qg"
"R2xvYmFsIENBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2swYYzD9"
"9BcjGlZ+W988bDjkcbd4kdS8odhM+KhDtgPpTSEHCIjaWC9mOSm9BXiLnTjoBbdq"
"fnGk5sRgprDvgOSJKA+eJdbtg/OtppHHmMlCGDUUna2YRpIuT8rxh0PBFpVXLVDv"
"iS2Aelet8u5fa9IAjbkU+BQVNdnARqN7csiRv8lVK83Qlz6cJmTM386DGXHKTubU"
"1XupGc1V3sjs0l44U+VcT4wt/lAjNvxm5suOpDkZALeVAjmRCw7+OC7RHQWa9k0+"
"bw8HHa8sHo9gOeL6NlMTOdReJivbPagUvTLrGAMoUgRx5aszPeE4uwc2hGKceeoW"
"MPRfwCvocWvk+QIDAQABo1MwUTAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBTA"
"ephojYn7qwVkDBF9qn1luMrMTjAfBgNVHSMEGDAWgBTAephojYn7qwVkDBF9qn1l"
"uMrMTjANBgkqhkiG9w0BAQUFAAOCAQEANeMpauUvXVSOKVCUn5kaFOSPeCpilKIn"
"Z57QzxpeR+nBsqTP3UEaBU6bS+5Kb1VSsyShNwrrZHYqLizz/Tt1kL/6cdjHPTfS"
"tQWVYrmm3ok9Nns4d0iXrKYgjy6myQzCsplFAMfOEVEiIuCl6rYVSAlk6l5PdPcF"
"PseKUgzbFbS9bZvlxrFUaKnjaZC2mqUPuLk/IH2uSrW4nOQdtqvmlKXBx4Ot2/Un"
"hw4EbNX/3aBd7YdStysVAq45pmp06drE57xNNB6pXE0zX5IJL4hmXXeXxx12E6nV"
"5fEWCRE11azbJHFwLJhWC9kXtNHjUStedejV0NxPNO3CBWaAocvmMw==\n"
"-----END CERTIFICATE-----\n"
/* GlobalSign Root CA - widely used worldwide */
"-----BEGIN CERTIFICATE-----\n"
"MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG"
"A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv"
"b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw"
"MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i"
"YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT"
"aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ"
"jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp"
"xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp"
"1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG"
"snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ"
"U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8"
"9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E"
"BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B"
"AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz"
"yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE"
"38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP"
"AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad"
"DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME"
"HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\n"
"-----END CERTIFICATE-----\n"
/* Starfield Services Root Certificate Authority - G2 */
"-----BEGIN CERTIFICATE-----\n"
"MIID7zCCAtegAwIBAgIBADANBgkqhkiG9w0BAQsFADCBmDELMAkGA1UEBhMCVVMx"
"EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxJTAjBgNVBAoT"
"HFN0YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4xOzA5BgNVBAMTMlN0YXJmaWVs"
"ZCBTZXJ2aWNlcyBSb290IENlcnRpZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5"
"MDkwMTAwMDAwMFoXDTM3MTIzMTIzNTk1OVowgZgxCzAJBgNVBAYTAlVTMRAwDgYD"
"VQQIEwdBcml6b25hMRMwEQYDVQQHEwpTY290dHNkYWxlMSUwIwYDVQQKExxTdGFy"
"ZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTswOQYDVQQDEzJTdGFyZmllbGQgU2Vy"
"dmljZXMgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIwDQYJKoZI"
"hvcNAQEBBQADggEPADCCAQoCggEBANUMOsQq+U7i9b4Zl1+OiFOxHz/Lz58gE20p"
"OsgPfTz3a3Y4Y9k2YKibXlwAgLIvWX/2h/klQ4bnaRtSmpDhcePYLQ1Ob/bISdm2"
"8xpWriu2dBTrz/sm4xq6HZYuajtYlIlHVv8loJNwU4PahHQUw2eeBGg6345AWh1K"
"Ts9DkTvnVtYAcMtS7nt9rjrnvDH5RfbCYM8TWQIrgMw0R9+53pBlbQLP1rTQ8MPz"
"GxMDm3KE8OBGPE8JT2BrEcjhZEXJayP9IQSyJo2A8xKOqcaHnm4Ib6c4DJoSjCKy"
"YKpQ7Y8dJQNJSNJR26sYXa3CTHgpYqYULQCO5LXGE9V8qPxwQBkCAwEAAaNjMGEw"
"DwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFJxfAN+q"
"AdcwKziIorhtSpzyEZGDMB8GA1UdIwQYMBaAFJxfAN+qAdcwKziIorhtSpzyEZGD"
"MA0GCSqGSIb3DQEBCwUAA4IBAQBLNqaEd2ndOxmfZyMIbw5hyf2E3F/YNoHN2BtB"
"LZ9g3ccaaNnRbobhiCPPE95Dz+I0swSdHynVv/heyNXBve6SbzJ08pGCL72CQnqt"
"KrcgfU28elUSwhXqvfdqlS5sdJ/PHLTyxQGjhdByPq1zqwubdQxtRbeOlKyWN7Wg"
"0I8VRw7j6IPdj/3vQQF3zCepYoUz8jcI73HPdwbeyBkdiEDPfUYd/x7H4c7/I9vG"
"3Gm+EpYPztN2pyUGvuA6OvTMsQ3mQD4O7PkL7oo/OOgMm7HZUgHZMJ4HGdnOH2v+"
"x3dGOqOOT6vAaWWYLW1wGI3h83LjQmFKd2J+Y1e0C80PlzNj\n"
"-----END CERTIFICATE-----\n";

// Common write callback for streaming with dual-buffer
size_t bncurl_common_write_callback(void *contents, size_t size, size_t nmemb, void *userdata)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)userdata;
    size_t total_size = size * nmemb;
    size_t bytes_written = 0;
    
    // Check if operation should be stopped
    if (!common_ctx->ctx->is_running) {
        return 0; // Abort the transfer
    }
    
    while (bytes_written < total_size) {
        bncurl_stream_buffer_t *active_buf = &common_ctx->stream->buffers[common_ctx->stream->active_buffer];
        size_t remaining_in_buffer = BNCURL_STREAM_BUFFER_SIZE - active_buf->size;
        size_t remaining_data = total_size - bytes_written;
        size_t bytes_to_copy = (remaining_in_buffer < remaining_data) ? remaining_in_buffer : remaining_data;
        
        // Copy data to active buffer
        memcpy(active_buf->data + active_buf->size, 
               (char *)contents + bytes_written, 
               bytes_to_copy);
        active_buf->size += bytes_to_copy;
        bytes_written += bytes_to_copy;
        
        // Check if buffer is full
        if (active_buf->size >= BNCURL_STREAM_BUFFER_SIZE) {
            active_buf->is_full = true;
            
            // Stream this buffer to output (file or UART)
            if (!bncurl_stream_buffer_to_output(common_ctx->stream, 
                                               common_ctx->stream->active_buffer)) {
                ESP_LOGE(TAG, "Failed to stream buffer to output");
                return 0; // Abort on error
            }
            
            // Switch to the other buffer
            common_ctx->stream->active_buffer = (common_ctx->stream->active_buffer + 1) % BNCURL_STREAM_BUFFER_COUNT;
            bncurl_stream_buffer_t *new_buf = &common_ctx->stream->buffers[common_ctx->stream->active_buffer];
            new_buf->size = 0;
            new_buf->is_full = false;
            new_buf->is_streaming = false;
        }
        
        // Update progress
        common_ctx->ctx->bytes_transferred += bytes_to_copy;
    }
    
    return total_size;
}

// Common header callback to get content length and stream headers for HEAD requests
size_t bncurl_common_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)userdata;
    size_t total_size = size * nitems;
    
    // For HEAD requests, stream headers using the streaming buffer system
    if (common_ctx && common_ctx->ctx && 
        strcmp(common_ctx->ctx->params.method, "HEAD") == 0) {
        // Only process HTTP headers (skip status line and empty lines)
        if (total_size > 2 && buffer[0] != '\r' && buffer[0] != '\n' &&
            strncmp(buffer, "HTTP/", 5) != 0) {
            
            // Clean up the header line by removing trailing CRLF
            char header_line[512];
            size_t copy_len = total_size < sizeof(header_line) - 3 ? total_size : sizeof(header_line) - 3; // Leave space for \r\n\0
            memcpy(header_line, buffer, copy_len);
            header_line[copy_len] = '\0';
            
            // Remove trailing \r\n
            char *end = header_line + strlen(header_line) - 1;
            while (end >= header_line && (*end == '\r' || *end == '\n')) {
                *end = '\0';
                end--;
            }
            
            // Add back clean CRLF for output
            if (strlen(header_line) > 0) {
                strcat(header_line, "\r\n");
                
                // Write header to streaming buffer
                bncurl_stream_context_t *stream = common_ctx->stream;
                bncurl_stream_buffer_t *active_buf = &stream->buffers[stream->active_buffer];
                size_t header_len = strlen(header_line);
                
                // Check if header fits in current buffer
                if (active_buf->size + header_len <= BNCURL_STREAM_BUFFER_SIZE) {
                    memcpy(active_buf->data + active_buf->size, header_line, header_len);
                    active_buf->size += header_len;
                } else {
                    // Current buffer full, stream it and switch to next buffer
                    if (active_buf->size > 0) {
                        bncurl_stream_buffer_to_output(stream, stream->active_buffer);
                        stream->active_buffer = (stream->active_buffer + 1) % BNCURL_STREAM_BUFFER_COUNT;
                        active_buf = &stream->buffers[stream->active_buffer];
                        active_buf->size = 0;
                    }
                    
                    // Add header to new buffer
                    if (header_len <= BNCURL_STREAM_BUFFER_SIZE) {
                        memcpy(active_buf->data, header_line, header_len);
                        active_buf->size = header_len;
                    }
                }
            }
        }
    }
    
    // Look for Content-Length header
    if (strncasecmp(buffer, "Content-Length:", 15) == 0) {
        char *length_str = buffer + 15;
        while (*length_str == ' ' || *length_str == '\t') length_str++; // Skip whitespace
        
        common_ctx->stream->total_size = strtoul(length_str, NULL, 10);
        common_ctx->ctx->bytes_total = common_ctx->stream->total_size;
        ESP_LOGI(TAG, "Content-Length detected: %u bytes", (unsigned int)common_ctx->stream->total_size);
    }
    
    return total_size;
}

// Common progress callback
int bncurl_common_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                   curl_off_t ultotal, curl_off_t ulnow)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)clientp;
    
    if (common_ctx && common_ctx->ctx) {
        // Update total if we didn't get it from headers
        if (dltotal > 0 && common_ctx->stream->total_size == 0) {
            common_ctx->stream->total_size = (size_t)dltotal;
            common_ctx->ctx->bytes_total = common_ctx->stream->total_size;
        }
        
        // Check if operation should be stopped
        if (!common_ctx->ctx->is_running) {
            return 1; // Abort the transfer
        }
    }
    
    return 0; // Continue
}

// Verbose debug callback to stream curl debug information to UART
int bncurl_common_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)userptr;
    
    // Only process if verbose mode is enabled
    if (!common_ctx || !common_ctx->ctx || !common_ctx->ctx->params.verbose) {
        return 0;
    }
    
    // Create a debug message prefix based on the info type
    const char *prefix = "";
    switch (type) {
        case CURLINFO_TEXT:
            prefix = "* ";
            break;
        case CURLINFO_HEADER_IN:
            prefix = "< ";
            break;
        case CURLINFO_HEADER_OUT:
            prefix = "> ";
            break;
        case CURLINFO_DATA_IN:
            prefix = "<< ";
            break;
        case CURLINFO_DATA_OUT:
            prefix = ">> ";
            break;
        case CURLINFO_SSL_DATA_IN:
        case CURLINFO_SSL_DATA_OUT:
            // Skip SSL data to avoid overwhelming output
            return 0;
        default:
            return 0;
    }
    
    // Process data line by line to add proper formatting
    char *data_copy = malloc(size + 1);
    if (!data_copy) {
        return 0;
    }
    
    memcpy(data_copy, data, size);
    data_copy[size] = '\0';
    
    // Split into lines and send each with prefix
    char *line = strtok(data_copy, "\r\n");
    while (line != NULL) {
        if (strlen(line) > 0) {
            // Create formatted debug line
            char debug_line[BNCURL_MAX_VERBOSE_LINE_LENGTH + 32];
            int line_len = snprintf(debug_line, sizeof(debug_line), "+VERBOSE:%s%s\r\n", prefix, line);
            
            if (line_len > 0 && line_len < sizeof(debug_line)) {
                esp_at_port_write_data((uint8_t *)debug_line, line_len);
            }
        }
        line = strtok(NULL, "\r\n");
    }
    
    free(data_copy);
    return 0;
}

bool bncurl_common_execute_request(bncurl_context_t *ctx, bncurl_stream_context_t *stream, 
                                   const char *method)
{
    if (!ctx || !stream || !method) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }
    
    CURL *curl;
    CURLcode res;
    bool success = false;
    char *post_data = NULL;  // Track allocated POST data for cleanup
    
    // Initialize cookie context
    bncurl_cookie_context_t cookie_ctx;
    bool has_cookie_save = (strlen(ctx->params.cookie_save) > 0);
    bool has_cookie_send = (strlen(ctx->params.cookie_send) > 0);
    
    if (has_cookie_save) {
        bncurl_cookies_init_context(&cookie_ctx, ctx->params.cookie_save);
    }
    
    // Initialize common context
    bncurl_common_context_t common_ctx;
    common_ctx.ctx = ctx;
    common_ctx.stream = stream;
    common_ctx.cookies = has_cookie_save ? &cookie_ctx : NULL;
    common_ctx.http_date_header = NULL;  // Initialize HTTP Date header to NULL
    
    // Initialize curl
    curl = curl_easy_init();
    if (!curl) {
        ESP_LOGE(TAG, "Failed to initialize curl");
        return false;
    }
    
    ctx->is_running = true;
    ctx->bytes_transferred = 0;
    ctx->bytes_total = 0;
    
    ESP_LOGI(TAG, "Starting %s request to: %s", method, ctx->params.url);
    
    // Log DNS configuration for debugging
    ESP_LOGI(TAG, "Using DNS servers: 8.8.8.8, 1.1.1.1, 208.67.222.222");
    
    // Check if this is an HTTPS request and ensure time is synchronized
    if (strncmp(ctx->params.url, "https://", 8) == 0) {
        ESP_LOGI(TAG, "HTTPS request detected - checking time synchronization");
        
        // Get current time
        time_t now;
        time(&now);
        
        // Check if time seems reasonable (after year 2020)
        if (now < 1577836800) { // January 1, 2020 00:00:00 UTC
            ESP_LOGW(TAG, "System time appears incorrect (before 2020). HTTPS may fail.");
            ESP_LOGW(TAG, "Current timestamp: %ld", (long)now);
            ESP_LOGW(TAG, "Use AT+CIPSNTPCFG and AT+CIPSNTPTIME to set correct time");
        } else {
            struct tm *timeinfo = gmtime(&now);
            ESP_LOGI(TAG, "System time: %04d-%02d-%02d %02d:%02d:%02d UTC", 
                     timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                     timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        }
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, ctx->params.url);
    
    // Set server response timeout (if no data received for timeout seconds, abort)
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, ctx->timeout);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L); // 1 byte/sec minimum speed
    
    // Keep a reasonable total timeout as safety net (much longer than response timeout)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, ctx->timeout * 10);
    
    // Set method-specific options
    if (strcmp(method, "GET") == 0) {
        // GET is default, no special setup needed
    } else if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        
        // Handle POST data if provided
        if (strlen(ctx->params.data_upload) > 0) {
            // Check if we have collected UART data (numeric -du parameter)
            if (ctx->params.is_numeric_upload) {
                if (ctx->params.collected_data != NULL && ctx->params.collected_data_size > 0) {
                    // Use collected UART data
                    ESP_LOGI(TAG, "POST: Using collected UART data, size: %zu bytes", ctx->params.collected_data_size);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ctx->params.collected_data);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, ctx->params.collected_data_size);
                } else {
                    // 0 bytes expected - send empty POST
                    ESP_LOGI(TAG, "POST: Sending empty POST (0 bytes)");
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
                }
                
            } else if (ctx->params.data_upload[0] == '@') {
                // File upload - read from file using POSIX operations
                const char *file_path = ctx->params.data_upload + 1; // Skip @
                ESP_LOGI(TAG, "POST: Uploading from file: %s", file_path);
                
                int fd = open(file_path, O_RDONLY);
                if (fd >= 0) {
                    // Get file size using fstat
                    struct stat file_stat;
                    if (fstat(fd, &file_stat) == 0) {
                        long file_size = file_stat.st_size;
                        
                        // Read file content
                        post_data = malloc(file_size);
                        if (post_data) {
                            ssize_t read_size = read(fd, post_data, file_size);
                            if (read_size == file_size) {
                                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
                                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, read_size);
                                ESP_LOGI(TAG, "POST: File uploaded, size: %d bytes", (int)read_size);
                            } else {
                                ESP_LOGE(TAG, "POST: Failed to read complete file: %d/%ld bytes (errno: %d)", 
                                         (int)read_size, file_size, errno);
                                free(post_data);
                                post_data = NULL;
                            }
                        } else {
                            ESP_LOGE(TAG, "POST: Failed to allocate memory for file: %ld bytes", file_size);
                        }
                    } else {
                        ESP_LOGE(TAG, "POST: Failed to get file stats: %s (errno: %d)", file_path, errno);
                    }
                    close(fd);
                } else {
                    ESP_LOGE(TAG, "POST: Failed to open file: %s (errno: %d)", file_path, errno);
                }
            } else if (ctx->params.is_numeric_upload) {
                // Should not reach here - numeric upload data should have been collected already
                ESP_LOGW(TAG, "POST: Numeric upload detected but no collected data available");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
            } else {
                // Legacy: Raw data size (for backwards compatibility)
                long data_size = atol(ctx->params.data_upload);
                ESP_LOGI(TAG, "POST: Empty data upload, size: %ld bytes", data_size);
                
                if (data_size > 0) {
                    char *empty_data = calloc(data_size, 1);
                    if (empty_data) {
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, empty_data);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data_size);
                        // Note: empty_data will be freed after curl_easy_perform
                    }
                } else {
                    // Send truly empty POST
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
                }
            }
        } else {
            // Empty POST request
            ESP_LOGI(TAG, "POST: Empty POST request (no data)");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        }
    } else if (strcmp(method, "HEAD") == 0) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        ESP_LOGI(TAG, "HEAD: Request configured (headers only)");
    }
    
    // Configure cookie loading if -b parameter is provided
    if (has_cookie_send) {
        if (!bncurl_cookies_load_from_file(curl, ctx->params.cookie_send)) {
            ESP_LOGW(TAG, "Failed to load cookies from file: %s", ctx->params.cookie_send);
        }
    }
    
    // Configure cookie saving if -c parameter is provided
    if (has_cookie_save) {
        if (!bncurl_cookies_configure_saving(curl, ctx->params.cookie_save, &cookie_ctx)) {
            ESP_LOGW(TAG, "Failed to configure cookie saving to: %s", ctx->params.cookie_save);
        }
    }
    
    // Set callbacks for data receiving (not for HEAD requests)
    if (strcmp(method, "HEAD") != 0) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bncurl_common_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &common_ctx);
    }
    
    // Set header callback (handles both content-length detection and cookie capturing)
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, bncurl_combined_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &common_ctx);
    
    // Set progress callback
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, bncurl_common_progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &common_ctx);
    
    // Set verbose debug callback if -v parameter is enabled
    if (ctx->params.verbose) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, bncurl_common_debug_callback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &common_ctx);
        ESP_LOGI(TAG, "Verbose mode enabled - debug info will be streamed to UART");
    }
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, BNCURL_MAX_REDIRECTS);
    
    // Set User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, BNCURL_DEFAULT_USER_AGENT);
    
    // Configure DNS and connection settings for better reliability
    curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, "8.8.8.8,1.1.1.1,208.67.222.222");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    
    // Configure HTTPS/TLS settings with certificate manager integration
    if (strncmp(ctx->params.url, "https://", 8) == 0) {
        ESP_LOGI(TAG, "HTTPS detected - configuring SSL with certificate manager integration");
        
        // Certificate Manager Integration with Smart Fallback Strategy
        bool ca_configured = false;
        bool client_configured = false;
        
        // Strategy 1: Try partition certificates first if manager is initialized
        if (bncert_manager_init()) {
            size_t cert_count = bncert_manager_get_cert_count();
            
            if (cert_count > 0) {
                ESP_LOGI(TAG, "Found %u certificates in partition, attempting to configure TLS", 
                         (unsigned int)cert_count);
                
                // Scan all certificates and configure them
                for (size_t i = 0; i < BNCERT_MAX_CERTIFICATES; i++) {
                    bncert_metadata_t cert_meta;
                    if (!bncert_manager_get_cert_by_index(i, &cert_meta)) {
                        continue;
                    }
                    
                    uint8_t *cert_data = NULL;
                    if (!bncert_manager_load_cert(cert_meta.address, cert_meta.size, &cert_data)) {
                        ESP_LOGW(TAG, "Failed to load certificate at 0x%08X", (unsigned int)cert_meta.address);
                        continue;
                    }
                    
                    // Validate certificate format
                    if (!bncert_manager_validate_cert(cert_data, cert_meta.size)) {
                        ESP_LOGW(TAG, "Invalid certificate format at 0x%08X", (unsigned int)cert_meta.address);
                        free(cert_data);
                        continue;
                    }
                    
                    // Detect certificate type
                    int cert_type = bncert_manager_detect_cert_type(cert_data, cert_meta.size);
                    
                    if (cert_type == 1 && !ca_configured) {
                        // Configure as CA certificate (partition takes precedence over hardcoded)
                        struct curl_blob ca_blob = { 
                            .data = cert_data, 
                            .len = cert_meta.size, 
                            .flags = CURL_BLOB_NOCOPY 
                        };
                        
                        CURLcode ca_result = curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &ca_blob);
                        if (ca_result == CURLE_OK) {
                            ESP_LOGI(TAG, "Using CA certificate from partition (%u bytes) - overriding hardcoded bundle", 
                                     (unsigned int)cert_meta.size);
                            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
                            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
                            ca_configured = true;
                            
                            // Store CA cert data pointer for cleanup later
                            ctx->ca_cert_data = cert_data;
                        } else {
                            ESP_LOGW(TAG, "Failed to set CA certificate from partition");
                            free(cert_data);
                        }
                    } else if (cert_type == 1 && ca_configured && !client_configured) {
                        // Configure as client certificate
                        struct curl_blob client_cert_blob = { 
                            .data = cert_data, 
                            .len = cert_meta.size, 
                            .flags = CURL_BLOB_NOCOPY 
                        };
                        
                        CURLcode cert_result = curl_easy_setopt(curl, CURLOPT_SSLCERT_BLOB, &client_cert_blob);
                        if (cert_result == CURLE_OK) {
                            ESP_LOGI(TAG, "Using client certificate from partition (%u bytes)", 
                                     (unsigned int)cert_meta.size);
                            ctx->client_cert_data = cert_data;
                            client_configured = true;
                        } else {
                            ESP_LOGW(TAG, "Failed to set client certificate from partition");
                            free(cert_data);
                        }
                    } else if (cert_type == 2 && client_configured) {
                        // Configure as client key
                        struct curl_blob client_key_blob = { 
                            .data = cert_data, 
                            .len = cert_meta.size, 
                            .flags = CURL_BLOB_NOCOPY 
                        };
                        
                        CURLcode key_result = curl_easy_setopt(curl, CURLOPT_SSLKEY_BLOB, &client_key_blob);
                        if (key_result == CURLE_OK) {
                            ESP_LOGI(TAG, "Using client key from partition (%u bytes)", 
                                     (unsigned int)cert_meta.size);
                            ctx->client_key_data = cert_data;
                        } else {
                            ESP_LOGW(TAG, "Failed to set client key from partition");
                            free(cert_data);
                        }
                    } else if (cert_type == 2 && !client_configured) {
                        // Found private key but no client certificate yet - store for later
                        ESP_LOGI(TAG, "Found private key in partition, waiting for client certificate");
                        free(cert_data); // Free for now, will be loaded again if needed
                    } else {
                        // Certificate not needed or duplicate
                        free(cert_data);
                    }
                }
            } else {
                ESP_LOGI(TAG, "No certificates found in partition");
            }
        }
        
        // Strategy 2: Use hardcoded CA bundle if no partition CA certificate was configured
        if (!ca_configured) {
            ESP_LOGI(TAG, "Using hardcoded CA bundle for SSL verification");
            struct curl_blob ca = { .data=(void*)CA_BUNDLE_PEM, .len=sizeof(CA_BUNDLE_PEM)-1, .flags=CURL_BLOB_COPY };
            CURLcode ca_result = curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &ca);
            
            if (ca_result == CURLE_OK) {
                ESP_LOGI(TAG, "Embedded CA bundle configured successfully");
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
                ca_configured = true;
            } else {
                ESP_LOGW(TAG, "Embedded CA bundle failed, using permissive SSL settings");
                // Strategy 3: Use permissive settings for broader compatibility
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // Disable peer verification
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);  // Disable hostname verification
                curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
            }
        }
        
        // Log final configuration
        if (ca_configured && client_configured) {
            ESP_LOGI(TAG, "SSL configured with CA certificate and client authentication");
        } else if (ca_configured) {
            ESP_LOGI(TAG, "SSL configured with CA certificate only");
        } else {
            ESP_LOGI(TAG, "SSL configured in permissive mode");
        }
        
        // Common SSL settings for better compatibility
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA | CURLSSLOPT_NO_REVOKE);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);
        
        ESP_LOGI(TAG, "SSL configuration complete - attempting HTTPS connection");
    }
    
    // Add custom headers if provided
    struct curl_slist *headers = NULL;
    if (ctx->params.header_count > 0) {
        for (int i = 0; i < ctx->params.header_count; i++) {
            headers = curl_slist_append(headers, ctx->params.headers[i]);
        }
    }
    
    // Add Range header if -r parameter is provided (GET requests only)
    if (strcmp(method, "GET") == 0 && strlen(ctx->params.range) > 0) {
        char range_header[128];
        snprintf(range_header, sizeof(range_header), "Range: bytes=%s", ctx->params.range);
        headers = curl_slist_append(headers, range_header);
        ESP_LOGI(TAG, "Added Range header: %s", range_header);
        
        // Log range download information
        ESP_LOGI(TAG, "Range download requested: %s", ctx->params.range);
        ESP_LOGI(TAG, "Data will be APPENDED to file: %s", ctx->params.data_download);
    }
    
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform the request
    res = curl_easy_perform(curl);
    
    // Update kill switch with HTTP Date header if available
    if (common_ctx.http_date_header) {
        bnkill_check_expiry(common_ctx.http_date_header);
        ESP_LOGI(TAG, "Updated kill switch with server date: %s", common_ctx.http_date_header);
    }
    
    if (res == CURLE_OK) {
        // Get response code
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        // Success criteria: 2xx status codes for all methods
        if (response_code >= 200 && response_code < 300) {
            // Stream any remaining data in the active buffer
            bncurl_stream_buffer_t *active_buf = &stream->buffers[stream->active_buffer];
            if (active_buf->size > 0) {
                bncurl_stream_buffer_to_output(stream, stream->active_buffer);
            }
            
            success = true;
            ESP_LOGI(TAG, "%s request completed successfully", method);
        } else {
            ESP_LOGW(TAG, "%s request failed with HTTP code: %ld", method, response_code);
        }
    } else {
        // Provide specific error messages for common issues
        if (res == CURLE_COULDNT_RESOLVE_HOST) {
            ESP_LOGE(TAG, "DNS resolution failed for %s", ctx->params.url);
            ESP_LOGE(TAG, "Check: 1) WiFi connection 2) DNS servers accessible 3) Hostname spelling");
            ESP_LOGE(TAG, "Suggestion: Try 'AT+CWJAP?' to check WiFi status");
        } else if (res == CURLE_COULDNT_CONNECT) {
            ESP_LOGE(TAG, "Connection failed - check network connectivity and firewall");
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            ESP_LOGE(TAG, "Operation timed out - check network stability");
        } else if (res == CURLE_SSL_CONNECT_ERROR) {
            ESP_LOGE(TAG, "SSL connection failed - certificate or TLS handshake issue");
            ESP_LOGE(TAG, "This may be due to certificate authority not being in embedded bundle");
            ESP_LOGE(TAG, "For testing, try an HTTP endpoint instead: http://httpbin.org/json");
            ESP_LOGE(TAG, "Or check if the service supports a different certificate authority");
        } else if (res == CURLE_PEER_FAILED_VERIFICATION) {
            ESP_LOGE(TAG, "SSL certificate verification failed - certificate not trusted");
            ESP_LOGE(TAG, "Certificate authority may not be in embedded CA bundle");
            ESP_LOGE(TAG, "For api.openweathermap.org, this is a known limitation");
            ESP_LOGE(TAG, "Consider using HTTP endpoints when available for testing");
        } else if (res == CURLE_SSL_CACERT) {
            ESP_LOGE(TAG, "SSL CA certificate problem - certificate authority not recognized");
            ESP_LOGE(TAG, "The embedded CA bundle may not include this service's certificate authority");
            ESP_LOGE(TAG, "This is common with some API services like OpenWeatherMap");
        } else {
            ESP_LOGE(TAG, "Curl error: %s (code: %d)", curl_easy_strerror(res), res);
        }
    }
    
    // Cleanup
    if (headers) {
        curl_slist_free_all(headers);
    }
    if (post_data) {
        free(post_data);
    }
    
    // Clean up HTTP date header if allocated
    if (common_ctx.http_date_header) {
        free(common_ctx.http_date_header);
        common_ctx.http_date_header = NULL;
    }
    
    // Clean up cookies if saving was enabled
    if (has_cookie_save) {
        bncurl_cookies_cleanup_context(&cookie_ctx);
    }
    
    // Cleanup parameters (especially collected data)
    bncurl_params_cleanup(&ctx->params);
    
    // Cleanup certificate data allocated during SSL configuration
    bncurl_cleanup_certificates(ctx);
    
    curl_easy_cleanup(curl);
    
    ctx->is_running = false;
    
    return success;
}

// Structure to capture content length from HEAD request
typedef struct {
    size_t content_length;
    bool found;
} head_response_t;

// Header callback for HEAD request to extract content length
static size_t head_header_callback_for_length(char *buffer, size_t size, size_t nitems, void *userdata)
{
    head_response_t *head_response = (head_response_t *)userdata;
    size_t total_size = size * nitems;
    
    // Look for Content-Length header
    if (strncasecmp(buffer, "Content-Length:", 15) == 0) {
        char *length_str = buffer + 15;
        while (*length_str == ' ' || *length_str == '\t') length_str++; // Skip whitespace
        
        head_response->content_length = strtoul(length_str, NULL, 10);
        head_response->found = true;
        ESP_LOGI(TAG, "HEAD request detected Content-Length: %u bytes", (unsigned int)head_response->content_length);
    }
    
    return total_size;
}

// Combined header callback for HEAD requests that captures both Content-Length and HTTP Date
typedef struct {
    head_response_t *head_response;
    bncurl_common_context_t *common_ctx;
} head_combined_context_t;

static size_t head_combined_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    head_combined_context_t *ctx = (head_combined_context_t *)userdata;
    size_t total_size = size * nitems;
    
    // Handle Content-Length
    if (strncasecmp(buffer, "Content-Length:", 15) == 0) {
        char *length_str = buffer + 15;
        while (*length_str == ' ' || *length_str == '\t') length_str++; // Skip whitespace
        
        ctx->head_response->content_length = strtoul(length_str, NULL, 10);
        ctx->head_response->found = true;
        ESP_LOGI(TAG, "HEAD request detected Content-Length: %u bytes", (unsigned int)ctx->head_response->content_length);
    }
    
    // Handle HTTP Date header for kill switch (if not already captured)
    if (ctx->common_ctx && !ctx->common_ctx->http_date_header) {
        if (strncasecmp(buffer, "Date:", 5) == 0) {
            char *date_start = buffer + 5;
            while (*date_start == ' ' || *date_start == '\t') date_start++; // Skip whitespace
            
            // Create null-terminated date string
            size_t date_len = total_size - (date_start - buffer);
            
            // Remove trailing CRLF
            while (date_len > 0 && (date_start[date_len-1] == '\r' || date_start[date_len-1] == '\n')) {
                date_len--;
            }
            
            if (date_len > 0) {
                ctx->common_ctx->http_date_header = malloc(date_len + 1);
                if (ctx->common_ctx->http_date_header) {
                    memcpy(ctx->common_ctx->http_date_header, date_start, date_len);
                    ctx->common_ctx->http_date_header[date_len] = '\0';
                    ESP_LOGI(TAG, "Captured HTTP Date header: %s", ctx->common_ctx->http_date_header);
                }
            }
        }
    }
    
    return total_size;
}

// Simple debug callback for content length HEAD requests  
static int content_length_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr)
{
    bncurl_context_t *ctx = (bncurl_context_t *)userptr;
    
    // Only process if verbose mode is enabled
    if (!ctx || !ctx->params.verbose) {
        return 0;
    }
    
    // Create a debug message prefix based on the info type
    const char *prefix = "";
    switch (type) {
        case CURLINFO_TEXT:
            prefix = "* ";
            break;
        case CURLINFO_HEADER_IN:
            prefix = "< ";
            break;
        case CURLINFO_HEADER_OUT:
            prefix = "> ";
            break;
        case CURLINFO_DATA_IN:
            prefix = "<< ";
            break;
        case CURLINFO_DATA_OUT:
            prefix = ">> ";
            break;
        case CURLINFO_SSL_DATA_IN:
        case CURLINFO_SSL_DATA_OUT:
            // Skip SSL data to avoid overwhelming output
            return 0;
        default:
            return 0;
    }
    
    // Process data line by line to add proper formatting
    char *data_copy = malloc(size + 1);
    if (!data_copy) {
        return 0;
    }
    
    memcpy(data_copy, data, size);
    data_copy[size] = '\0';
    
    // Split into lines and send each with prefix
    char *line = strtok(data_copy, "\r\n");
    while (line != NULL) {
        if (strlen(line) > 0) {
            // Create formatted debug line for content length HEAD request
            char debug_line[BNCURL_MAX_VERBOSE_LINE_LENGTH + 32];
            int line_len = snprintf(debug_line, sizeof(debug_line), "+VERBOSE:%s%s\r\n", prefix, line);
            
            if (line_len > 0 && line_len < sizeof(debug_line)) {
                esp_at_port_write_data((uint8_t *)debug_line, line_len);
            }
        }
        line = strtok(NULL, "\r\n");
    }
    
    free(data_copy);
    return 0;
}

bool bncurl_common_get_content_length(bncurl_context_t *ctx, size_t *content_length)
{
    if (!ctx || !content_length) {
        ESP_LOGE(TAG, "Invalid parameters");
        *content_length = SIZE_MAX; // Use SIZE_MAX to indicate -1
        return false;
    }

    CURL *curl;
    CURLcode res;
    head_response_t head_response = {0, false};
    bool success = false;
    
    // Create minimal common context for HTTP Date header capture
    bncurl_common_context_t common_ctx;
    common_ctx.ctx = ctx;
    common_ctx.stream = NULL;
    common_ctx.cookies = NULL;
    common_ctx.http_date_header = NULL;
    
    // Create combined context for the header callback
    head_combined_context_t head_combined_ctx = {
        .head_response = &head_response,
        .common_ctx = &common_ctx
    };

    *content_length = SIZE_MAX; // Initialize to SIZE_MAX (will be converted to -1)    // Check if this is an HTTPS request
    bool is_https = (strncmp(ctx->params.url, "https://", 8) == 0);
    
    // Initialize curl for HEAD request
    curl = curl_easy_init();
    if (!curl) {
        ESP_LOGE(TAG, "Failed to initialize curl for HEAD request");
        return false;
    }
    
    ESP_LOGI(TAG, "Making HEAD request to get content length: %s", ctx->params.url);
    
    // Small delay for HTTPS to allow any previous connections to settle
    if (is_https) {
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay for HTTPS
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, ctx->params.url);
    
    // Configure as HEAD request
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    
    // Configure server response timeout for HEAD request
    long timeout = is_https ? 30L : 15L;  // Longer timeout for HTTPS
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, timeout);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L); // 1 byte/sec minimum speed
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout * 5); // Safety net
    
    // Set header callback to capture Content-Length and HTTP Date
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, head_combined_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &head_combined_ctx);
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, BNCURL_MAX_REDIRECTS);
    
    // Set User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, BNCURL_DEFAULT_USER_AGENT);
    
    // Configure verbose debug output if -v parameter is enabled
    if (ctx->params.verbose) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, content_length_debug_callback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, ctx);
        ESP_LOGI(TAG, "Verbose mode enabled for content length HEAD request");
    }
    
    // Configure DNS and connection settings (longer timeouts for HTTPS)
    curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, "8.8.8.8,1.1.1.1,208.67.222.222");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, is_https ? 20L : 10L);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    
    // Configure HTTPS/TLS settings with more permissive approach for HEAD requests
    if (is_https) {
        // For HEAD requests, use permissive SSL settings for broader compatibility
        // since we're only getting headers, not sensitive content
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // Disable peer verification for HEAD
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);  // Disable hostname verification for HEAD
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA | CURLSSLOPT_NO_REVOKE);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);
        curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
        
        ESP_LOGI(TAG, "HEAD request using permissive HTTPS configuration for compatibility");
    }
    
    // Add custom headers if provided (but skip content-related headers for HEAD)
    struct curl_slist *headers = NULL;
    
    // Check if this is a range request and add Range header
    if (strlen(ctx->params.range) > 0) {
        char range_header[256];
        snprintf(range_header, sizeof(range_header), "Range: bytes=%s", ctx->params.range);
        headers = curl_slist_append(headers, range_header);
        ESP_LOGI(TAG, "Adding Range header for HEAD request: %s", range_header);
    }
    
    if (ctx->params.header_count > 0) {
        for (int i = 0; i < ctx->params.header_count; i++) {
            // Skip content-type and content-length headers for HEAD request
            if (strncasecmp(ctx->params.headers[i], "Content-Type:", 13) != 0 &&
                strncasecmp(ctx->params.headers[i], "Content-Length:", 15) != 0) {
                headers = curl_slist_append(headers, ctx->params.headers[i]);
            }
        }
    }
    
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform the HEAD request
    ESP_LOGI(TAG, "Executing HEAD request with %ld second server response timeout...", timeout);
    res = curl_easy_perform(curl);
    
    // Update kill switch with HTTP Date header if available
    if (common_ctx.http_date_header) {
        bnkill_check_expiry(common_ctx.http_date_header);
        ESP_LOGI(TAG, "Updated kill switch with server date: %s", common_ctx.http_date_header);
    }
    
    if (res == CURLE_OK) {
        // Get response code
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        ESP_LOGI(TAG, "HEAD request completed with HTTP code: %ld", response_code);
        
        if (response_code >= 200 && response_code < 300) {
            if (head_response.found) {
                *content_length = head_response.content_length;
                success = true;
                ESP_LOGI(TAG, "HEAD request successful, Content-Length: %u bytes", (unsigned int)*content_length);
            } else {
                ESP_LOGW(TAG, "HEAD request successful but no Content-Length header found");
                *content_length = SIZE_MAX; // No content length available = -1
                success = false;
            }
        } else {
            ESP_LOGW(TAG, "HEAD request failed with HTTP code: %ld", response_code);
            *content_length = SIZE_MAX; // Failed request = -1
        }
    } else {
        ESP_LOGW(TAG, "HEAD request curl error: %s (code: %d)", curl_easy_strerror(res), res);
        *content_length = SIZE_MAX; // Curl error = -1
    }
    
    // Cleanup
    if (headers) {
        curl_slist_free_all(headers);
    }
    
    // Clean up HTTP date header if allocated
    if (common_ctx.http_date_header) {
        free(common_ctx.http_date_header);
        common_ctx.http_date_header = NULL;
    }
    
    // Cleanup certificate data allocated during SSL configuration
    bncurl_cleanup_certificates(ctx);
    
    curl_easy_cleanup(curl);
    
    return success;
}
