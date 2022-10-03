cmake_minimum_required(VERSION 3.20.4 FATAL_ERROR)

set(MEDIA_SOURCES Media.cpp
                  MediaPlayer.cpp
                  Audio/AudioPlayer.cpp
                  Audio/OpenALSoundProvider.cpp)

set(MEDIA_HEADERS Media.h
                  MediaPlayer.h
                  Audio/AudioPlayer.h
                  Audio/OpenALSoundProvider.h)

if(APPLE)
  add_definitions(-DFF_API_NEXT -Wno-deprecated-declarations)
endif()

add_library(media STATIC ${MEDIA_SOURCES} ${MEDIA_HEADERS})
target_link_libraries(media utility)

include_directories(${INCLUDE_DIRECTORIES} ${OPENAL_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(media ${OPENAL_LIBRARY} ${AVCODEC_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVUTIL_LIBRARIES} ${SWSCALE_LIBRARIES} ${SWRESAMPLE_LIBRARIES})

target_check_style(media)

message(VERBOSE "FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES}")
message(VERBOSE "OPENAL_LIBRARY: ${OPENAL_LIBRARY}")