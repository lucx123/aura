idf_component_register(
    SRCS "main.c" "aura_services.c" "aura_wifi.c" "aura_portal.c" "aura_ui.c" "aura_motion.c" "aura_eclipse.c" "aura_network_tools.c" "aura_engineer_tools.c" "aura_storage_format.c" "aura_evidence_store.c"
    INCLUDE_DIRS "."
    REQUIRES esp_pm esp_app_format json esp_event esp_http_server esp_netif esp_timer esp_wifi fatfs sdmmc esp_driver_sdmmc lwip mbedtls nvs_flash
)
