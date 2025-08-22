file(GLOB CURL_SOURCES
    "src/*.c"
    "src/vtls/*.c"
    "src/vssh/*.c"
    # Needed for Curl_conn_may_http3() even when HTTP3 not enabled
    "src/vauth/*.c"
    "src/vquic/*.c"  # include all vquic sources (was only two files before)
)

# You asked to use GLOB. Note: ESP-IDF normally recommends listing sources explicitly
# to avoid stale builds; CONFIGURE_DEPENDS mitigates this by re-checking on reconfigure.
message(STATUS "libcurl-esp32: collected sources: ${CURL_SOURCES}")

idf_component_register(
    SRCS ${CURL_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "." "src"  # add src so tools find curl_setup.h & internal headers
    REQUIRES mbedtls lwip esp_netif esp_event
)

# Required by this repo:
target_compile_definitions(${COMPONENT_LIB} PRIVATE BUILDING_LIBCURL HAVE_CONFIG_H)

# Optional but useful on IDF builds
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function -Wno-sign-compare)
