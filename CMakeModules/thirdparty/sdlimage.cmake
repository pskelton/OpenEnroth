# hack to be able to create a convenience library without sources
file(WRITE ${LIB_DIR}/sdl2image.c "")
add_library(
    sdl2image STATIC
    ${LIB_DIR}/sdl2image.c
)

if (WIN32)
set(SDL2IMG_DIR "${LIBRARY_DIR}/SDL2image-2.0.5")
set(SDL2IMG_BIN_DIR "${SDL2IMG_DIR}/bin")
set(SDL2IMG_LIB_DIR "${SDL2IMG_DIR}/lib")
set(SDL2IMG_INCLUDE_DIR "${SDL2IMG_DIR}/include")

set_target_properties(
    sdl2image PROPERTIES
    PUBLIC_HEADER "${SDL2IMG_INCLUDE_DIR}/SDL_image.h"
)

target_link_libraries(
    sdl2image
    "${SDL2IMG_LIB_DIR}/SDL2_image.lib"
)

include_directories(
    "${SDL2IMG_INCLUDE_DIR}"
)

ADD_GLOBAL_DEPENDENCY("${SDL2IMG_BIN_DIR}/SDL2_image.dll")
ADD_GLOBAL_DEPENDENCY("${SDL2IMG_BIN_DIR}/libjpeg-9.dll")
ADD_GLOBAL_DEPENDENCY("${SDL2IMG_BIN_DIR}/libpng16-16.dll")
ADD_GLOBAL_DEPENDENCY("${SDL2IMG_BIN_DIR}/libtiff-5.dll")
ADD_GLOBAL_DEPENDENCY("${SDL2IMG_BIN_DIR}/libwebp-7.dll")
ADD_GLOBAL_DEPENDENCY("${SDL2IMG_BIN_DIR}/zlib1.dll")

else()  # WIN32
find_package(SDL2IMAGE REQUIRED)
target_link_libraries(sdl2image SDL2IMAGE)
include_directories("${SDLIMG_INCLUDE_DIR}")
endif()
