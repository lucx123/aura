idf_component_register(
    SRCS "main.c" "aura_services.c" "aura_wifi.c" "aura_ui.c" "aura_motion.c"
    INCLUDE_DIRS "."
    REQUIRES esp_event esp_http_server esp_netif esp_timer esp_wifi lwip nvs_flash
)
