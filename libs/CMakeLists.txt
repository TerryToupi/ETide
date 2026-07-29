set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)

add_subdirectory(SDL)
add_subdirectory(imgui)

add_library(ETideLibraries INTERFACE)

add_library(ETide::Libraries ALIAS ETideLibraries)

target_link_libraries(ETideLibraries INTERFACE
  ImGui::ImGui
  SDL3::SDL3)
