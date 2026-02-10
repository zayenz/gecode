#
# Native source inventory for the CMake build.
# This file intentionally avoids any parsing of Makefile.in.
#

function(gecode_collect_glob out_var)
  set(result)
  foreach(pattern IN LISTS ARGN)
    file(GLOB matches RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${pattern}")
    list(APPEND result ${matches})
  endforeach()
  list(REMOVE_DUPLICATES result)
  list(SORT result)
  set(${out_var} ${result} PARENT_SCOPE)
endfunction()

function(gecode_collect_glob_recurse out_var)
  set(result)
  foreach(pattern IN LISTS ARGN)
    file(GLOB_RECURSE matches RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${pattern}")
    list(APPEND result ${matches})
  endforeach()
  list(REMOVE_DUPLICATES result)
  list(SORT result)
  set(${out_var} ${result} PARENT_SCOPE)
endfunction()

gecode_collect_glob(
  GECODE_SUPPORT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/support/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/support/thread/*.cpp")

gecode_collect_glob(
  GECODE_KERNEL_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/kernel/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/kernel/data/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/kernel/branch/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/kernel/memory/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/kernel/trace/*.cpp")

gecode_collect_glob(
  GECODE_SEARCH_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/search/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/search/seq/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/search/par/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/search/cpprofiler/*.cpp")

gecode_collect_glob_recurse(
  GECODE_INT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/int/*.cpp")

gecode_collect_glob_recurse(
  GECODE_SET_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/set/*.cpp")

gecode_collect_glob_recurse(
  GECODE_FLOAT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/float/*.cpp")

gecode_collect_glob(
  GECODE_MINIMODEL_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/minimodel/*.cpp")

gecode_collect_glob(
  GECODE_DRIVER_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/driver/*.cpp")

gecode_collect_glob(
  GECODE_GIST_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/gist/*.cpp")

gecode_collect_glob(
  GECODE_FLATZINC_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/gecode/flatzinc/*.cpp")

gecode_collect_glob_recurse(
  GECODE_TEST_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

set(GECODE_FLATZINC_EXE_SOURCE tools/flatzinc/fzn-gecode.cpp)
