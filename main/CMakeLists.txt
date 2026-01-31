idf_component_register(SRCS "main.c"
                      INCLUDE_DIRS "."
                      PRIV_REQUIRES spi_flash esp_timer unity m5stack_tab5 esp_driver_ppa)

# 使用自定义的 lv_conf.h
target_compile_definitions(${COMPONENT_TARGET} PUBLIC LV_CONF_INCLUDE_SIMPLE)