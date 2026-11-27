# Shared BLE, protocol and SD-history headers come unchanged from the Waveshare
# controller sources; only the board layer and the UI are T-Deck specific.
get_filename_component(MCX_SHARED "${CMAKE_CURRENT_LIST_DIR}/../../display-prototype" ABSOLUTE)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" MCX_TDECK_VERSION)
string(STRIP "${MCX_TDECK_VERSION}" MCX_TDECK_VERSION)
idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "." "${MCX_SHARED}"
                       REQUIRES bt nvs_flash esp_timer esp_lcd esp_adc fatfs sdmmc mbedtls efuse driver esp_driver_gpio esp_driver_spi esp_driver_i2c esp_driver_sdspi)
target_compile_definitions(${COMPONENT_LIB} PRIVATE MCX_TDECK_VERSION="${MCX_TDECK_VERSION}" MCX_MEMORY_DIAGNOSTICS=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wframe-larger-than=4096 -fstack-usage)
# LVGL allocations go to PSRAM through the shared allocator header.
foreach(name lvgl lvgl__lvgl)
  if(TARGET __idf_${name})
    idf_component_get_property(mcx_lvgl_lib ${name} COMPONENT_LIB)
    target_include_directories(${mcx_lvgl_lib} PRIVATE "${MCX_SHARED}")
  endif()
endforeach()
