cmake_minimum_required(VERSION 3.18)

set(CMAKE_COLOR_DIAGNOSTICS ON)

project(cardretro
  DESCRIPTION "Example of a cardflow in raylib"
  HOMEPAGE_URL "https://github.com/konsumer/raylib-cardflow"
  VERSION 0.0.1
  LANGUAGES C
)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
endif()

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
endif()

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0169 NEW)
endif()

include(FetchContent)

FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.zip
    OVERRIDE_FIND_PACKAGE
)

set(BUILD_RAYLIB_LIBRETRO_EXAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_RAYLIB_LIBRETRO         OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib_libretro
    GIT_REPOSITORY https://github.com/RobLoach/raylib-libretro.git
    GIT_TAG        master
    GIT_SUBMODULES "vendor/libretro-common"
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(raylib raylib_libretro)

add_executable(${PROJECT_NAME} src/main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib raylib-libretro-static)

if(WIN32)
  target_compile_definitions(vmlib PRIVATE
    NOMINMAX
    WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP
  )
  target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
endif()
