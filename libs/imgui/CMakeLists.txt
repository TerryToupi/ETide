add_library(ImGui STATIC
  LICENSE.txt
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  backends/imgui_impl_sdl3.cpp
  backends/imgui_impl_sdlgpu3.cpp)

add_library(ImGui::ImGui ALIAS ImGui)

target_include_directories(ImGui PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/backends)

target_link_libraries(ImGui PUBLIC
  SDL3::SDL3)
