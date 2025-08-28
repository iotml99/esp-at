
set(srcs 
    "custom/at_custom_cmd.c"
    "custom/sd_card.c"
)

set(includes "include" "custom")

# Add more required components you need here, separated by spaces
set(require_components at freertos nvs_flash fatfs sdmmc driver esp_driver_spi esp_driver_sdspi)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${includes}
    REQUIRES ${require_components})

idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE TRUE)
