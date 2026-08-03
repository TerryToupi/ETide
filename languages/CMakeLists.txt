# Vendored tree-sitter grammars.
#
# Each directory is generated output from the tree-sitter CLI, not a project of
# its own: src/parser.c holds the parse tables, src/scanner.c is the optional
# external scanner, and src/tree_sitter/ holds the headers those two include.
# Because they are all structurally identical, one function builds them instead
# of a per-grammar CMakeLists.
#
# Adding a language: drop the generated src/ tree in a directory here and add a
# single etide_add_language() line below.

add_library(ETideLanguages INTERFACE)
add_library(ETide::Languages ALIAS ETideLanguages)

function(etide_add_language name)
  set(source_dir ${CMAKE_CURRENT_SOURCE_DIR}/${name}/src)

  if(NOT EXISTS ${source_dir}/parser.c)
    message(FATAL_ERROR "Grammar '${name}' has no src/parser.c")
  endif()

  set(sources ${source_dir}/parser.c)
  if(EXISTS ${source_dir}/scanner.c)
    list(APPEND sources ${source_dir}/scanner.c)
  endif()

  add_library(${name} STATIC ${sources})

  # The grammar's own tree_sitter/ headers must win over the runtime's, so this
  # stays PRIVATE and is not forwarded to consumers.
  target_include_directories(${name} PRIVATE ${source_dir})

  target_link_libraries(${name} PUBLIC tree-sitter)

  # Generated code; its warnings are not actionable for us.
  if(MSVC)
    target_compile_options(${name} PRIVATE /w)
  else()
    target_compile_options(${name} PRIVATE -w)
  endif()

  target_link_libraries(ETideLanguages INTERFACE ${name})
endfunction()

etide_add_language(tree-sitter-c)
etide_add_language(tree-sitter-cpp)
etide_add_language(tree-sitter-json)
etide_add_language(tree-sitter-odin)
etide_add_language(tree-sitter-rust)
