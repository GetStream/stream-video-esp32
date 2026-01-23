idf_component_register(INCLUDE_DIRS ./include
                       SRC_DIRS "src"
                       PRIV_REQUIRES mbedtls)

get_filename_component(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR} NAME)
add_prebuilt_library(${BASE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/libs/${IDF_TARGET}/libpeer_default.a"
                     PRIV_REQUIRES ${BASE_DIR} esp_timer)
target_link_libraries(${COMPONENT_LIB} PRIVATE "-L ${CMAKE_CURRENT_SOURCE_DIR}/libs/${IDF_TARGET}")
target_link_libraries(${COMPONENT_LIB} PRIVATE ${BASE_DIR})
