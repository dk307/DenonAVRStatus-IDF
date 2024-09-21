idf_component_register(SRCS "main.cpp" 
                            "util/helper.cpp"
                            "util/timer/timer.cpp"
                            INCLUDE_DIRS "."
                    REQUIRES esp_hw_support esp_event esp_timer)

target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>)
