if (${IDF_TARGET} STREQUAL "esp32p4")
list(APPEND COMPONENT_REQUIRES  esp_driver_ppa esp_hw_support esp_mm)
endif()

idf_component_register(SRC_DIRS "./"
                       INCLUDE_DIRS "./include"
                       REQUIRES ${COMPONENT_REQUIRES}
                       PRIV_INCLUDE_DIRS "./private_include")
