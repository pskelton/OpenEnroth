cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

set(ENGINE_COMPONENTS_DETERMINISTIC_SOURCES
        EngineDeterministicComponent.cpp)

set(ENGINE_COMPONENTS_DETERMINISTIC_HEADERS
        EngineDeterministicComponent.h)

add_library(engine_components_deterministic STATIC ${ENGINE_COMPONENTS_DETERMINISTIC_SOURCES} ${ENGINE_COMPONENTS_DETERMINISTIC_HEADERS})
target_check_style(engine_components_deterministic)

target_link_libraries(engine_components_deterministic PUBLIC
        engine_components_random
        library_platform_application)
