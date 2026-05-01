cmake_minimum_required(VERSION 3.18)

# enable colors for ninja
set(CMAKE_COLOR_DIAGNOSTICS ON)

project(cardflow
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
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip
)
if(EMSCRIPTEN)
  set(PLATFORM Web CACHE STRING "" FORCE)
endif()

FetchContent_MakeAvailable(raylib)

add_executable(${PROJECT_NAME} src/main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(WIN32)
  target_compile_definitions(vmlib PRIVATE
    NOMINMAX
    WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP
  )
  target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
endif()

# ---------------------------------------------------------------------------
# Emscripten / WebAssembly
# ---------------------------------------------------------------------------
# Build with:
#   emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-web
#
# Output: wbuild/cardflow.mjs (wasm inlined) + wbuild/cardflow.data (preloaded assets)
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
  # Emit a .mjs ES-module instead of .html
  set(CMAKE_EXECUTABLE_SUFFIX ".mjs")

  # Tell raylib we are targeting the web platform
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WEB)

  target_link_options(${PROJECT_NAME} PRIVATE
    # ES6 module output so you can: import cardflow from './cardflow.mjs'
    -sSINGLE_FILE=1  # inlines .wasm into the .mjs; .data stays separate
    -sASYNCIFY
    -sUSE_GLFW=3
    -O3
    -sINITIAL_MEMORY=67108864
    -sALLOW_MEMORY_GROWTH=1
    --preload-file ${CMAKE_SOURCE_DIR}/poster.png@poster.png
  )
endif()
