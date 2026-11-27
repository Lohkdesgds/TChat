cmake_minimum_required(VERSION 3.28)

project(TwitchChatPlus
    VERSION 1.0.0
    DESCRIPTION "TwitchChat is a standalone chat app for Twitch streamers"
    LANGUAGES CXX
)

#set(SDL3PLUS_ENABLE_GRAPHICS OFF)
set(SDL3PLUS_ENABLE_AUDIO OFF)
set(SDL3PLUS_ENABLE_SOCKETS OFF)

include(cmake/installSDL3plus.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DUNICODE -D_UNICODE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNICODE -D_UNICODE")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS OFF)

file(GLOB_RECURSE MAIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

sdl3plus_add_game(${PROJECT_NAME}
    SOURCES
        ${MAIN_SOURCES}
    
#    ICON
#        resources/icon.png
    STUDIO
        lohksstudiosworks
)

sdl3plus_add_android_key_file(${PROJECT_NAME} resources/release-key.jks)


target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(${PROJECT_NAME} PUBLIC APP_VERSION="${CMAKE_PROJECT_VERSION}")

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

b_embed(${PROJECT_NAME} "resources/config.json")
b_embed(${PROJECT_NAME} "resources/calibri_fix.ttf")