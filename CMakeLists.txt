cmake_minimum_required(VERSION 3.9)

project(blw4)

set(PROJECT_SOURCE game.cpp src/backend/wasm_wasm3.c)

file(GLOB COMMON_SOURCES RELATIVE "${CMAKE_SOURCE_DIR}" "src/*.c")
file(GLOB COMMON_SOURCES_CPP RELATIVE "${CMAKE_SOURCE_DIR}" "src/*.cpp")
file(GLOB COMMON_SOURCES_HPP RELATIVE "${CMAKE_SOURCE_DIR}" "src/*.hpp")
file(GLOB M3_SOURCES RELATIVE "${CMAKE_SOURCE_DIR}" "vendor/wasm3/source/*.c")

set(PROJECT_DISTRIBS LICENSE README.md)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Build configuration; approach this with caution!
if(MSVC)
  add_compile_options("/W4" "/wd4244" "/wd4324" "/wd4458" "/wd4100")
else()
  add_compile_options("-Wall" "-Wextra" "-Wdouble-promotion" "-Wno-unused-parameter")
endif()

find_package (32BLIT CONFIG REQUIRED PATHS ../32blit-sdk $ENV{PATH_32BLIT_SDK})

blit_executable (${PROJECT_NAME} ${PROJECT_SOURCE} ${M3_SOURCES} ${COMMON_SOURCES} ${COMMON_SOURCES_CPP} ${COMMON_SOURCES_HPP})
blit_assets_yaml (${PROJECT_NAME} assets.yml)
blit_metadata (${PROJECT_NAME} metadata.yml)
add_custom_target (flash DEPENDS ${PROJECT_NAME}.flash)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/vendor/wasm3/source")
if(NOT MSVC)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-error=double-promotion)
endif()

# setup release packages
install (FILES ${PROJECT_DISTRIBS} DESTINATION .)
set (CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set (CPACK_GENERATOR "ZIP" "TGZ")
include (CPack)
