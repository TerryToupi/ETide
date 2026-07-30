set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(SDL)
add_subdirectory(imgui)
add_subdirectory(tree-sitter)

add_library(ETideLibraries INTERFACE)
add_library(ETide::Libraries ALIAS ETideLibraries)

target_link_libraries(ETideLibraries INTERFACE
  tree-sitter
  ImGui::ImGui
  SDL3::SDL3)
