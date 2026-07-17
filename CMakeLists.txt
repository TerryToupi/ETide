cmake_minimum_required(VERSION 3.23)

project(ETide
  VERSION 0.0.1
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
add_subdirectory(libs/SDL)

add_executable(ETide
  src/main.cpp
  src/core/core.cpp

  libs/imgui/imgui.cpp
  libs/imgui/imgui_draw.cpp
  libs/imgui/imgui_tables.cpp
  libs/imgui/imgui_widgets.cpp

  libs/imgui/misc/cpp/imgui_stdlib.cpp

  libs/imgui/backends/imgui_impl_sdl3.cpp
  libs/imgui/backends/imgui_impl_sdlrenderer3.cpp)

target_include_directories(ETide PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui)

target_link_libraries(ETide PUBLIC
  SDL3::SDL3)
