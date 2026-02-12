#
# CMake build script for Gecode.
#

cmake_minimum_required(VERSION 3.8.0)

project(GECODE LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake_modules)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.11.0)
  cmake_policy(SET CMP0072 NEW)
  set(OpenGL_GL_PREFERENCE LEGACY)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.17.0)
  cmake_policy(SET CMP0100 NEW)
endif()

# ---------------------------------------------------------------------------
# Build options (configure/autoconf parity-oriented)
# ---------------------------------------------------------------------------
# Legacy aliases are translated after option() initialization so they also
# override stale cache values from previous configure runs.
if(DEFINED ENABLE_THREADS)
  set(GECODE_ALIAS_ENABLE_THREAD_VALUE "${ENABLE_THREADS}")
  set(GECODE_USED_ENABLE_THREADS_ALIAS TRUE)
endif()
if(DEFINED ENABLE_GIST)
  set(GECODE_ALIAS_ENABLE_GIST_VALUE "${ENABLE_GIST}")
  set(GECODE_USED_ENABLE_GIST_ALIAS TRUE)
endif()
if(DEFINED BUILD_EXAMPLES)
  set(GECODE_ALIAS_BUILD_EXAMPLES_VALUE "${BUILD_EXAMPLES}")
  set(GECODE_USED_BUILD_EXAMPLES_ALIAS TRUE)
endif()
if(DEFINED ENABLE_CPPROFILER)
  set(GECODE_ALIAS_ENABLE_CPPROFILER_VALUE "${ENABLE_CPPROFILER}")
  set(GECODE_USED_ENABLE_CPPROFILER_ALIAS TRUE)
endif()

option(GECODE_BUILD_SHARED "Build shared libraries" ON)
option(GECODE_BUILD_STATIC "Build static libraries" OFF)

option(GECODE_ENABLE_THREAD "Enable thread support" ON)
option(GECODE_ENABLE_QT "Enable Qt support" ON)
set(GECODE_QT_DETECTION "cmake" CACHE STRING
  "Qt detection policy: cmake or autoconf")
set_property(CACHE GECODE_QT_DETECTION PROPERTY STRINGS cmake autoconf)
option(GECODE_ENABLE_GIST "Enable Gist" ON)
option(GECODE_ENABLE_CPPROFILER "Enable CPProfiler support" ON)
option(GECODE_ENABLE_CBS "Enable counting-based search support" OFF)
option(GECODE_ENABLE_EXAMPLES "Build examples" ON)

option(GECODE_ENABLE_SEARCH "Build search module" ON)
option(GECODE_ENABLE_INT_VARS "Build int variables module" ON)
option(GECODE_ENABLE_SET_VARS "Build set variables module" ON)
option(GECODE_ENABLE_FLOAT_VARS "Build float variables module" ON)
option(GECODE_ENABLE_MINIMODEL "Build minimodel module" ON)
option(GECODE_ENABLE_DRIVER "Build driver module" ON)
option(GECODE_ENABLE_FLATZINC "Build FlatZinc module" ON)

option(GECODE_ENABLE_MPFR "Enable MPFR support" ON)
option(GECODE_ENABLE_ALLOCATOR "Enable default allocator" ON)
option(GECODE_ENABLE_AUDIT "Enable audit code" OFF)
option(GECODE_ENABLE_GCC_VISIBILITY "Enable GCC visibility attributes" ON)
option(GECODE_ENABLE_OSX_UNFAIR_MUTEX "Enable macOS unfair mutexes" ON)
set(GECODE_FREELIST32_SIZE_MAX "" CACHE STRING "Max freelist size on 32-bit platforms")
set(GECODE_FREELIST64_SIZE_MAX "" CACHE STRING "Max freelist size on 64-bit platforms")
set(GECODE_WITH_VIS "" CACHE STRING "Additional .vis files (comma-separated)")

# Compatibility aliases (temporary)
if(GECODE_USED_ENABLE_THREADS_ALIAS)
  set(GECODE_ENABLE_THREAD ${GECODE_ALIAS_ENABLE_THREAD_VALUE} CACHE BOOL "Enable thread support" FORCE)
  message(WARNING "ENABLE_THREADS is deprecated; use GECODE_ENABLE_THREAD")
endif()
if(GECODE_USED_ENABLE_GIST_ALIAS)
  set(GECODE_ENABLE_GIST ${GECODE_ALIAS_ENABLE_GIST_VALUE} CACHE BOOL "Enable Gist" FORCE)
  message(WARNING "ENABLE_GIST is deprecated; use GECODE_ENABLE_GIST")
endif()
if(GECODE_USED_BUILD_EXAMPLES_ALIAS)
  set(GECODE_ENABLE_EXAMPLES ${GECODE_ALIAS_BUILD_EXAMPLES_VALUE} CACHE BOOL "Build examples" FORCE)
  message(WARNING "BUILD_EXAMPLES is deprecated; use GECODE_ENABLE_EXAMPLES")
endif()
if(GECODE_USED_ENABLE_CPPROFILER_ALIAS)
  set(GECODE_ENABLE_CPPROFILER ${GECODE_ALIAS_ENABLE_CPPROFILER_VALUE} CACHE BOOL "Enable CPProfiler support" FORCE)
  message(WARNING "ENABLE_CPPROFILER is deprecated; use GECODE_ENABLE_CPPROFILER")
endif()

if(NOT GECODE_BUILD_SHARED AND NOT GECODE_BUILD_STATIC)
  message(FATAL_ERROR "At least one of GECODE_BUILD_SHARED or GECODE_BUILD_STATIC must be ON")
endif()
if(WIN32 AND GECODE_BUILD_SHARED AND GECODE_BUILD_STATIC)
  message(FATAL_ERROR "Building both shared and static in one tree is not supported on Windows")
endif()

# Keep dependency closure behavior similar to configure.
if(GECODE_ENABLE_SET_VARS OR GECODE_ENABLE_FLOAT_VARS)
  set(GECODE_ENABLE_INT_VARS ON CACHE BOOL "Build int variables module" FORCE)
endif()
if(GECODE_ENABLE_DRIVER)
  set(GECODE_ENABLE_SEARCH ON CACHE BOOL "Build search module" FORCE)
  set(GECODE_ENABLE_INT_VARS ON CACHE BOOL "Build int variables module" FORCE)
endif()
if(GECODE_ENABLE_FLATZINC)
  set(GECODE_ENABLE_SEARCH ON CACHE BOOL "Build search module" FORCE)
  set(GECODE_ENABLE_DRIVER ON CACHE BOOL "Build driver module" FORCE)
  set(GECODE_ENABLE_MINIMODEL ON CACHE BOOL "Build minimodel module" FORCE)
endif()
if(GECODE_ENABLE_EXAMPLES)
  set(GECODE_ENABLE_SEARCH ON CACHE BOOL "Build search module" FORCE)
  set(GECODE_ENABLE_DRIVER ON CACHE BOOL "Build driver module" FORCE)
  set(GECODE_ENABLE_MINIMODEL ON CACHE BOOL "Build minimodel module" FORCE)
endif()

include(CheckCXXCompilerFlag)
if(GECODE_ENABLE_GCC_VISIBILITY)
  check_cxx_compiler_flag(-fvisibility=hidden HAVE_VISIBILITY_HIDDEN_FLAG)
  if(HAVE_VISIBILITY_HIDDEN_FLAG)
    add_compile_options(-fvisibility=hidden)
  endif()
endif()

find_package(Perl REQUIRED)

if(GECODE_ENABLE_MPFR)
  find_package(MPFR)
endif()

set(GECODE_HAS_QT_RUNTIME FALSE)
string(TOLOWER "${GECODE_QT_DETECTION}" GECODE_QT_DETECTION_MODE)
if(NOT GECODE_QT_DETECTION_MODE STREQUAL "cmake"
   AND NOT GECODE_QT_DETECTION_MODE STREQUAL "autoconf")
  message(FATAL_ERROR
    "GECODE_QT_DETECTION must be one of: cmake, autoconf")
endif()
if(GECODE_ENABLE_QT)
  set(GECODE_QT_PROBE_ALLOWED TRUE)
  if(GECODE_QT_DETECTION_MODE STREQUAL "autoconf")
    find_program(GECODE_QMAKE_EXECUTABLE NAMES qmake-qt4 qmake)
    find_program(GECODE_MOC_EXECUTABLE NAMES moc-qt4 moc)
    if(NOT GECODE_QMAKE_EXECUTABLE OR NOT GECODE_MOC_EXECUTABLE)
      set(GECODE_QT_PROBE_ALLOWED FALSE)
      message(STATUS
        "GECODE_QT_DETECTION=autoconf: qmake or moc not found, disabling Qt")
    else()
      execute_process(
        COMMAND "${GECODE_QMAKE_EXECUTABLE}" -query QT_VERSION
        OUTPUT_VARIABLE GECODE_QMAKE_QT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GECODE_QMAKE_QT_VERSION_RESULT)
      if(NOT GECODE_QMAKE_QT_VERSION_RESULT EQUAL 0)
        set(GECODE_QT_PROBE_ALLOWED FALSE)
        message(STATUS
          "GECODE_QT_DETECTION=autoconf: unable to query Qt version from qmake, disabling Qt")
      elseif(NOT GECODE_QMAKE_QT_VERSION MATCHES "^([0-9]+)\\.([0-9]+)")
        set(GECODE_QT_PROBE_ALLOWED FALSE)
        message(STATUS
          "GECODE_QT_DETECTION=autoconf: invalid Qt version from qmake, disabling Qt")
      else()
        set(GECODE_QT_MAJOR "${CMAKE_MATCH_1}")
        set(GECODE_QT_MINOR "${CMAKE_MATCH_2}")
        if(GECODE_QT_MAJOR LESS 4 OR
           (GECODE_QT_MAJOR EQUAL 4 AND GECODE_QT_MINOR LESS 3))
          set(GECODE_QT_PROBE_ALLOWED FALSE)
          message(STATUS
            "GECODE_QT_DETECTION=autoconf: Qt ${GECODE_QMAKE_QT_VERSION} is too old (< 4.3), disabling Qt")
        endif()
      endif()
    endif()
  endif()

  if(GECODE_QT_PROBE_ALLOWED)
    find_package(Qt6 QUIET COMPONENTS Core Gui Widgets PrintSupport)
    if(Qt6_FOUND)
      set(GECODE_HAS_QT_RUNTIME TRUE)
      set(GECODE_QT_CORE Qt6::Core)
      set(GECODE_QT_WIDGETS Qt6::Widgets Qt6::Gui Qt6::PrintSupport)
      set(CMAKE_AUTOMOC TRUE)
    else()
      find_package(Qt5 QUIET COMPONENTS Core Gui Widgets PrintSupport)
      if(Qt5_FOUND)
        set(GECODE_HAS_QT_RUNTIME TRUE)
        set(GECODE_QT_CORE Qt5::Core)
        set(GECODE_QT_WIDGETS Qt5::Widgets Qt5::Gui Qt5::PrintSupport)
        set(CMAKE_AUTOMOC TRUE)
      else()
        find_package(Qt4 QUIET)
        if(QT4_FOUND)
          include(${QT_USE_FILE})
          set(GECODE_HAS_QT_RUNTIME TRUE)
          set(GECODE_QT_CORE ${QT_LIBRARIES})
          set(GECODE_QT_WIDGETS ${QT_LIBRARIES})
          set(CMAKE_AUTOMOC TRUE)
        endif()
      endif()
    endif()
  endif()
endif()

set(GECODE_BUILD_GIST_TARGET ${GECODE_ENABLE_GIST})
if(GECODE_BUILD_GIST_TARGET AND NOT GECODE_HAS_QT_RUNTIME)
  message(WARNING "GECODE_ENABLE_GIST is ON but Qt was not found. Disabling Gist.")
  set(GECODE_BUILD_GIST_TARGET FALSE)
endif()

# ---------------------------------------------------------------------------
# Configure-time metadata and config.hpp generation
# ---------------------------------------------------------------------------
file(READ gecode/support/config.hpp.in CONFIG)
string(REGEX REPLACE "^/\\*([^*]|\\*[^/])*\\*/" "" CONFIG ${CONFIG})
string(REGEX MATCHALL "[^\n]*\n" CONFIG
"${CONFIG}
/* Define to 1 if you have a working `mmap' system call. */
#undef HAVE_MMAP

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the home page for this package. */
#undef PACKAGE_URL

/* Define to the version of this package. */
#undef PACKAGE_VERSION
")

# Pull package info from configure.ac to keep one version source of truth.
file(READ configure.ac LINES)
string(REPLACE ";" "<semi>" LINES "${LINES}")
string(REGEX MATCHALL "[^\n]*\n" LINES "${LINES}")
set(ah_command FALSE)
foreach(line "${EXTRA_CONFIG}" ${LINES})
  string(REPLACE ";" "" line "${line}")
  if(ah_command)
    # keep collecting
  elseif(line MATCHES "AC_INIT\\(([^,]*), *([^,]*), *([^)]*)\\)")
    set(PACKAGE ${CMAKE_MATCH_1})
    set(VERSION ${CMAKE_MATCH_2})
    set(PACKAGE_BUGREPORT ${CMAKE_MATCH_3})
    message(STATUS "Got VERSION=${VERSION} from configure.ac")
  elseif(line MATCHES "ac_gecode_soversion=(.*)\n")
    set(GECODE_SOVERSION "${CMAKE_MATCH_1}")
  elseif(line MATCHES "ac_gecode_flatzincversion=(.*)\n")
    set(GECODE_FLATZINC_VERSION "${CMAKE_MATCH_1}")
  elseif(line MATCHES "AH_BOTTOM\\(\\[(.*)")
    set(ah_command bottom)
    set(line "${CMAKE_MATCH_1}")
  elseif(line MATCHES "AH_VERBATIM[^,]+,(.*)")
    set(ah_command verbatim)
    set(line "${CMAKE_MATCH_1}")
  endif()

  if(ah_command)
    set(saved_ah_command ${ah_command})
    if(line MATCHES "^\\[(.*)")
      set(line "${CMAKE_MATCH_1}")
    endif()
    if(line MATCHES "\\]\\)")
      set(ah_command FALSE)
      string(REPLACE "])" "" line "${line}")
    endif()
    string(REGEX MATCHALL "[^\n]*\n" sublines "${line}")
    set(config_add "")
    foreach(subline ${sublines})
      set(config_add ${config_add} "${subline}")
    endforeach()
    if(saved_ah_command STREQUAL "verbatim")
      set(CONFIG ${config_add} ${CONFIG})
    else()
      set(CONFIG ${CONFIG} "\n" ${config_add})
    endif()
  endif()
endforeach()

if(NOT DEFINED GECODE_SOVERSION)
  set(GECODE_SOVERSION 51)
endif()

set(PACKAGE_NAME ${PACKAGE})
string(TOLOWER ${PACKAGE} PACKAGE_TARNAME)
set(PACKAGE_URL "")
set(PACKAGE_VERSION ${VERSION})
set(${PACKAGE}_VERSION ${VERSION})
set(GECODE_PROJECT_VERSION ${VERSION})
string(REPLACE "." "-" GECODE_LIBRARY_VERSION "${VERSION}")
set(PACKAGE_STRING "${PACKAGE} ${VERSION}")
if(VERSION MATCHES "(.*)\\.(.*)\\.(.*)")
  math(EXPR GECODE_VERSION_NUMBER
    "${CMAKE_MATCH_1} * 100000 + ${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}")
endif()

set(GECODE_DLL_USERPREFIX "")
set(GECODE_DLL_USERSUFFIX "")
if(GECODE_ENABLE_INT_VARS)
  set(GECODE_HAS_INT_VARS "/**/")
endif()
if(GECODE_ENABLE_SET_VARS)
  set(GECODE_HAS_SET_VARS "/**/")
endif()
if(GECODE_ENABLE_FLOAT_VARS)
  set(GECODE_HAS_FLOAT_VARS "/**/")
endif()
if(GECODE_BUILD_STATIC)
  set(GECODE_STATIC_LIBS 1)
endif()
if(GECODE_ENABLE_ALLOCATOR)
  set(GECODE_ALLOCATOR "/**/")
endif()
if(GECODE_ENABLE_AUDIT)
  set(GECODE_AUDIT "/**/")
endif()
if(GECODE_ENABLE_CPPROFILER)
  set(GECODE_HAS_CPPROFILER "/**/")
endif()
if(GECODE_ENABLE_CBS)
  set(GECODE_HAS_CBS "/**/")
endif()
if(GECODE_ENABLE_MPFR AND MPFR_FOUND AND GECODE_ENABLE_FLOAT_VARS)
  set(GECODE_HAS_MPFR "/**/")
endif()
if(HAVE_VISIBILITY_HIDDEN_FLAG)
  set(GECODE_GCC_HAS_CLASS_VISIBILITY "/**/")
endif()
if(GECODE_ENABLE_THREAD)
  set(GECODE_HAS_THREADS 1)
endif()
if(APPLE AND GECODE_ENABLE_THREAD AND GECODE_ENABLE_OSX_UNFAIR_MUTEX)
  set(GECODE_USE_OSX_UNFAIR_MUTEX "/**/")
endif()
if(GECODE_HAS_QT_RUNTIME)
  set(GECODE_HAS_QT "/**/")
endif()
if(GECODE_BUILD_GIST_TARGET)
  set(GECODE_HAS_GIST "/**/")
endif()
if(GECODE_FREELIST32_SIZE_MAX)
  set(GECODE_FREELIST_SIZE_MAX32 ${GECODE_FREELIST32_SIZE_MAX})
endif()
if(GECODE_FREELIST64_SIZE_MAX)
  set(GECODE_FREELIST_SIZE_MAX64 ${GECODE_FREELIST64_SIZE_MAX})
endif()

include(CheckSymbolExists)
check_symbol_exists(mmap sys/mman.h HAVE_MMAP)

include(CheckCSourceCompiles)
check_c_source_compiles("inline __attribute__ ((__always_inline__)) void foo(void) {}\nint main() {}" HAVE_ALWAYS_INLINE)
check_c_source_compiles("__forceinline void foo(void) {}\nint main() {}" HAVE_FORCE_INLINE)
set(forceinline inline)
if(HAVE_ALWAYS_INLINE)
  set(forceinline "inline __attribute__ ((__always_inline__))")
endif()
if(HAVE_FORCE_INLINE)
  set(forceinline "__forceinline")
endif()

check_c_source_compiles("int main() { return __builtin_ffsll(0); }" HAVE_BUILTIN_FFSLL)
if(HAVE_BUILTIN_FFSLL)
  set(GECODE_HAS_BUILTIN_FFSLL "/**/")
endif()
check_c_source_compiles("int main() { return __builtin_popcountll(0); }" HAVE_BUILTIN_POPCOUNTLL)
if(HAVE_BUILTIN_POPCOUNTLL)
  set(GECODE_HAS_BUILTIN_POPCOUNTLL "/**/")
endif()

# Process config.hpp using autoconf-like undef handling.
list(LENGTH CONFIG length)
math(EXPR length "${length} - 1")
foreach(i RANGE ${length})
  list(GET CONFIG ${i} line)
  if(line MATCHES "^#( *)undef (.*)\n")
    set(space "${CMAKE_MATCH_1}")
    set(var ${CMAKE_MATCH_2})
    if(NOT DEFINED ${var} OR (var MATCHES "HAVE_.*" AND NOT ${var}))
      set(line "/* #${space}undef ${var} */\n")
    else()
      if("${${var}}" STREQUAL "/**/" OR "${var}" STREQUAL "GECODE_VERSION_NUMBER" OR
         "${var}" STREQUAL "forceinline" OR var MATCHES "SIZEOF_.*")
        set(value ${${var}})
      elseif(NOT (var MATCHES ^HAVE OR ${var} EQUAL 0 OR ${var} EQUAL 1))
        set(value \"${${var}}\")
      elseif(${var})
        set(value 1)
      else()
        set(value 0)
      endif()
      set(line "#${space}define ${var} ${value}\n")
    endif()
  endif()
  string(REPLACE "<semi>" ";" line "${line}")
  set(CONFIG_OUT "${CONFIG_OUT}${line}")
endforeach()

file(WRITE ${GECODE_BINARY_DIR}/gecode/support/config.hpp
"/* gecode/support/config.hpp.  Generated from config.hpp.in by configure.  */
/* gecode/support/config.hpp.in.  Generated from configure.ac by autoheader.  */

/* Disable MSVC pragma-based auto-linking: CMake wires dependencies explicitly. */
#define GECODE_NO_AUTOLINK 1

${CONFIG_OUT}")

# ---------------------------------------------------------------------------
# Native source inventory for CMake (decoupled from Makefile.in).
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GecodeSources.cmake)

# ---------------------------------------------------------------------------
# Generated variable implementations (in-source, matching Makefile behavior)
# ---------------------------------------------------------------------------
set(GECODE_VIS_FILES)
if(GECODE_ENABLE_INT_VARS)
  list(APPEND GECODE_VIS_FILES
    gecode/int/var-imp/int.vis
    gecode/int/var-imp/bool.vis)
endif()
if(GECODE_ENABLE_SET_VARS)
  list(APPEND GECODE_VIS_FILES
    gecode/set/var-imp/set.vis)
endif()
if(GECODE_ENABLE_FLOAT_VARS)
  list(APPEND GECODE_VIS_FILES
    gecode/float/var-imp/float.vis)
endif()
if(GECODE_WITH_VIS)
  string(REPLACE "," ";" _extra_vis "${GECODE_WITH_VIS}")
  foreach(vis ${_extra_vis})
    list(APPEND GECODE_VIS_FILES "${vis}")
  endforeach()
endif()
list(REMOVE_DUPLICATES GECODE_VIS_FILES)

set(GECODE_VIS_DEPENDS)
set(GECODE_VIS_FILES_FOR_GEN)
foreach(vis ${GECODE_VIS_FILES})
  if(IS_ABSOLUTE "${vis}")
    list(APPEND GECODE_VIS_DEPENDS "${vis}")
    list(APPEND GECODE_VIS_FILES_FOR_GEN "${vis}")
  else()
    list(APPEND GECODE_VIS_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${vis}")
    if(vis MATCHES "^\\./")
      list(APPEND GECODE_VIS_FILES_FOR_GEN "${vis}")
    else()
      list(APPEND GECODE_VIS_FILES_FOR_GEN "./${vis}")
    endif()
  endif()
endforeach()
string(JOIN "||" GECODE_VIS_FILES_SERIALIZED ${GECODE_VIS_FILES_FOR_GEN})

set(GECODE_VAR_TYPE_HPP ${CMAKE_CURRENT_SOURCE_DIR}/gecode/kernel/var-type.hpp)
set(GECODE_VAR_IMP_HPP ${CMAKE_CURRENT_SOURCE_DIR}/gecode/kernel/var-imp.hpp)

add_custom_command(
  OUTPUT ${GECODE_VAR_TYPE_HPP}
  COMMAND ${CMAKE_COMMAND}
    -DPERL_EXECUTABLE=${PERL_EXECUTABLE}
    -DGENVARIMP=${CMAKE_CURRENT_SOURCE_DIR}/misc/genvarimp.perl
    -DMODE=-typehpp
    -DOUT_FILE=${GECODE_VAR_TYPE_HPP}
    -DVIS_FILES_SERIALIZED=${GECODE_VIS_FILES_SERIALIZED}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVarImp.cmake
  DEPENDS ${GECODE_VIS_DEPENDS} ${CMAKE_CURRENT_SOURCE_DIR}/misc/genvarimp.perl ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt ${CMAKE_CURRENT_SOURCE_DIR}/configure.ac
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  VERBATIM
)
add_custom_command(
  OUTPUT ${GECODE_VAR_IMP_HPP}
  COMMAND ${CMAKE_COMMAND}
    -DPERL_EXECUTABLE=${PERL_EXECUTABLE}
    -DGENVARIMP=${CMAKE_CURRENT_SOURCE_DIR}/misc/genvarimp.perl
    -DMODE=-header
    -DOUT_FILE=${GECODE_VAR_IMP_HPP}
    -DVIS_FILES_SERIALIZED=${GECODE_VIS_FILES_SERIALIZED}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVarImp.cmake
  DEPENDS ${GECODE_VIS_DEPENDS} ${CMAKE_CURRENT_SOURCE_DIR}/misc/genvarimp.perl ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt ${CMAKE_CURRENT_SOURCE_DIR}/configure.ac
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  VERBATIM
)
add_custom_target(gecode-varimp-gen DEPENDS ${GECODE_VAR_TYPE_HPP} ${GECODE_VAR_IMP_HPP})

# ---------------------------------------------------------------------------
# Target creation helpers
# ---------------------------------------------------------------------------
set(GECODE_LIBRARY_COMPONENTS support kernel)
if(GECODE_ENABLE_SEARCH)
  list(APPEND GECODE_LIBRARY_COMPONENTS search)
endif()
if(GECODE_ENABLE_INT_VARS)
  list(APPEND GECODE_LIBRARY_COMPONENTS int)
endif()
if(GECODE_ENABLE_SET_VARS)
  list(APPEND GECODE_LIBRARY_COMPONENTS set)
endif()
if(GECODE_ENABLE_FLOAT_VARS)
  list(APPEND GECODE_LIBRARY_COMPONENTS float)
endif()
if(GECODE_ENABLE_MINIMODEL)
  list(APPEND GECODE_LIBRARY_COMPONENTS minimodel)
endif()
if(GECODE_ENABLE_DRIVER)
  list(APPEND GECODE_LIBRARY_COMPONENTS driver)
endif()
if(GECODE_ENABLE_FLATZINC)
  list(APPEND GECODE_LIBRARY_COMPONENTS flatzinc)
endif()
if(GECODE_BUILD_GIST_TARGET)
  list(APPEND GECODE_LIBRARY_COMPONENTS gist)
endif()

set(GECODE_DEFAULT_LINK_VARIANT)
if(GECODE_BUILD_SHARED)
  set(GECODE_DEFAULT_LINK_VARIANT shared)
else()
  set(GECODE_DEFAULT_LINK_VARIANT static)
endif()

function(add_gecode_component_library lib)
  string(TOUPPER ${lib} libupper)
  set(sources ${GECODE_${libupper}_SOURCES})
  if(lib STREQUAL "float" AND NOT (GECODE_ENABLE_MPFR AND MPFR_FOUND))
    # Keep in sync with Make behavior: skip empty MPFR-only source when MPFR is absent.
    list(REMOVE_ITEM sources "gecode/float/rounding.cpp")
  endif()

  if(NOT sources)
    return()
  endif()

  if(GECODE_BUILD_SHARED)
    add_library(gecode${lib}_shared SHARED ${sources})
    target_compile_definitions(gecode${lib}_shared PRIVATE GECODE_BUILD_${libupper})
    target_include_directories(gecode${lib}_shared
      PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    set_target_properties(gecode${lib}_shared PROPERTIES
      OUTPUT_NAME gecode${lib}
      VERSION ${GECODE_PROJECT_VERSION}
      SOVERSION ${GECODE_SOVERSION})
    add_dependencies(gecode${lib}_shared gecode-varimp-gen)
    list(APPEND GECODE_INSTALL_TARGETS gecode${lib}_shared)
    list(APPEND GECODE_EXPORT_TARGETS gecode${lib}_shared)
    set(GECODE_INSTALL_TARGETS ${GECODE_INSTALL_TARGETS} PARENT_SCOPE)
    set(GECODE_EXPORT_TARGETS ${GECODE_EXPORT_TARGETS} PARENT_SCOPE)
  endif()

  if(GECODE_BUILD_STATIC)
    add_library(gecode${lib}_static STATIC ${sources})
    target_compile_definitions(gecode${lib}_static PRIVATE GECODE_BUILD_${libupper})
    target_include_directories(gecode${lib}_static
      PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    set_target_properties(gecode${lib}_static PROPERTIES OUTPUT_NAME gecode${lib})
    add_dependencies(gecode${lib}_static gecode-varimp-gen)
    list(APPEND GECODE_INSTALL_TARGETS gecode${lib}_static)
    list(APPEND GECODE_EXPORT_TARGETS gecode${lib}_static)
    set(GECODE_INSTALL_TARGETS ${GECODE_INSTALL_TARGETS} PARENT_SCOPE)
    set(GECODE_EXPORT_TARGETS ${GECODE_EXPORT_TARGETS} PARENT_SCOPE)
  endif()

  add_library(gecode${lib} INTERFACE)
  target_link_libraries(gecode${lib} INTERFACE gecode${lib}_${GECODE_DEFAULT_LINK_VARIANT})
  list(APPEND GECODE_EXPORT_TARGETS gecode${lib})
  set(GECODE_EXPORT_TARGETS ${GECODE_EXPORT_TARGETS} PARENT_SCOPE)
endfunction()

function(gecode_link_component comp)
  foreach(kind shared static)
    if(TARGET gecode${comp}_${kind})
      foreach(dep ${ARGN})
        if(TARGET gecode${dep}_${kind})
          target_link_libraries(gecode${comp}_${kind} PUBLIC gecode${dep}_${kind})
        elseif(TARGET ${dep})
          target_link_libraries(gecode${comp}_${kind} PUBLIC ${dep})
        endif()
      endforeach()
    endif()
  endforeach()
endfunction()

foreach(component ${GECODE_LIBRARY_COMPONENTS})
  add_gecode_component_library(${component})
endforeach()

if(GECODE_ENABLE_THREAD)
  find_package(Threads REQUIRED)
  gecode_link_component(support Threads::Threads)
endif()

gecode_link_component(kernel support)
if(GECODE_ENABLE_SEARCH)
  gecode_link_component(search kernel)
endif()
if(GECODE_ENABLE_INT_VARS)
  gecode_link_component(int kernel)
endif()
if(GECODE_ENABLE_SET_VARS)
  gecode_link_component(set int)
endif()
if(GECODE_ENABLE_FLOAT_VARS)
  gecode_link_component(float int kernel)
  if(GECODE_ENABLE_MPFR AND MPFR_FOUND)
    foreach(kind shared static)
      if(TARGET gecodefloat_${kind})
        target_link_libraries(gecodefloat_${kind} PUBLIC ${MPFR_LIBRARIES})
        target_include_directories(gecodefloat_${kind} PRIVATE ${MPFR_INCLUDES})
      endif()
    endforeach()
  endif()
endif()
if(GECODE_ENABLE_MINIMODEL)
  set(mm_deps int set search)
  if(GECODE_ENABLE_FLOAT_VARS)
    list(APPEND mm_deps float)
  endif()
  gecode_link_component(minimodel ${mm_deps})
endif()
if(GECODE_ENABLE_DRIVER)
  set(driver_deps int search)
  if(GECODE_ENABLE_MINIMODEL)
    list(APPEND driver_deps minimodel)
  endif()
  if(GECODE_BUILD_GIST_TARGET)
    list(APPEND driver_deps gist)
  endif()
  gecode_link_component(driver ${driver_deps})
endif()
if(GECODE_BUILD_GIST_TARGET)
  foreach(kind shared static)
    if(TARGET gecodegist_${kind})
      target_link_libraries(gecodegist_${kind} PUBLIC ${GECODE_QT_WIDGETS})
    endif()
  endforeach()
  if(GECODE_ENABLE_SEARCH)
    gecode_link_component(gist search int)
  endif()
endif()
if(GECODE_ENABLE_FLATZINC)
  set(fzn_deps minimodel driver)
  if(GECODE_BUILD_GIST_TARGET)
    list(APPEND fzn_deps gist)
  endif()
  gecode_link_component(flatzinc ${fzn_deps})
  if(GECODE_HAS_QT_RUNTIME)
    foreach(kind shared static)
      if(TARGET gecodeflatzinc_${kind})
        target_link_libraries(gecodeflatzinc_${kind} PUBLIC ${GECODE_QT_CORE})
      endif()
    endforeach()
  endif()
endif()

# ---------------------------------------------------------------------------
# Executables / tests
# ---------------------------------------------------------------------------
if(GECODE_ENABLE_FLATZINC)
  add_executable(fzn-gecode ${GECODE_FLATZINC_EXE_SOURCE})
  target_link_libraries(fzn-gecode PRIVATE gecodeflatzinc)
  list(APPEND GECODE_INSTALL_TARGETS fzn-gecode)
endif()

set(GECODE_CAN_BUILD_TESTS TRUE)
foreach(required search int minimodel driver)
  if(NOT TARGET gecode${required})
    set(GECODE_CAN_BUILD_TESTS FALSE)
  endif()
endforeach()

if(GECODE_ENABLE_SET_VARS AND NOT TARGET gecodeset)
  set(GECODE_CAN_BUILD_TESTS FALSE)
endif()
if(GECODE_ENABLE_FLOAT_VARS AND NOT TARGET gecodefloat)
  set(GECODE_CAN_BUILD_TESTS FALSE)
endif()
if(GECODE_ENABLE_FLATZINC AND NOT TARGET gecodeflatzinc)
  set(GECODE_CAN_BUILD_TESTS FALSE)
endif()

set(GECODE_TEST_SOURCES_SELECTED ${GECODE_TEST_SOURCES})
if(NOT GECODE_ENABLE_SET_VARS)
  list(FILTER GECODE_TEST_SOURCES_SELECTED EXCLUDE REGEX "^test/set(/|\\.cpp)")
endif()
if(NOT GECODE_ENABLE_FLOAT_VARS)
  list(FILTER GECODE_TEST_SOURCES_SELECTED EXCLUDE REGEX "^test/float(/|\\.cpp)")
endif()
if(NOT GECODE_ENABLE_FLATZINC)
  list(FILTER GECODE_TEST_SOURCES_SELECTED EXCLUDE REGEX "^test/flatzinc(/|\\.cpp)")
endif()

if(GECODE_CAN_BUILD_TESTS)
  add_executable(gecode-test EXCLUDE_FROM_ALL ${GECODE_TEST_SOURCES_SELECTED})
  set(GECODE_TEST_LINK_LIBS gecodeminimodel)
  if(GECODE_ENABLE_FLATZINC)
    list(APPEND GECODE_TEST_LINK_LIBS gecodeflatzinc)
  endif()
  target_link_libraries(gecode-test PRIVATE ${GECODE_TEST_LINK_LIBS})

  enable_testing()
  set(GECODE_CHECK_TESTS
    Branch::Int::Dense::3
    Int::Arithmetic::Abs
    Int::Arithmetic::ArgMax
    Int::Arithmetic::Max::Nary
    Int::Cumulative::Man::Fix::0::4
    Int::Distinct::Random
    Int::Linear::Bool::Int::Lq
    Int::MiniModel::LinExpr::Bool::352
    NoGoods::Queens
    Search::DFS::Sol::Binary::Nary::Binary::1::1::1)
  if(GECODE_ENABLE_FLATZINC)
    list(INSERT GECODE_CHECK_TESTS 1 FlatZinc::magic_square)
  endif()
  if(GECODE_ENABLE_SET_VARS)
    list(APPEND GECODE_CHECK_TESTS
      Set::Dom::Dom::Gr
      Set::RelOp::ConstSSI::Union
      Set::Sequence::SeqU1
      Set::Wait)
  endif()

  set(GECODE_CHECK_ARGS -iter 2 -threads 0)
  foreach(gecode_check_test ${GECODE_CHECK_TESTS})
    list(APPEND GECODE_CHECK_ARGS -test ${gecode_check_test})
  endforeach()

  add_test(NAME test COMMAND gecode-test ${GECODE_CHECK_ARGS})
  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^test$"
    DEPENDS gecode-test
    USES_TERMINAL)
else()
  message(WARNING "Skipping gecode-test/check targets because required modules are disabled")
  add_custom_target(check
    COMMAND ${CMAKE_COMMAND} -E echo "Skipping check target because required modules are disabled")
endif()

# ---------------------------------------------------------------------------
# FlatZinc scripts and solver configs
# ---------------------------------------------------------------------------
set(prefix ${CMAKE_INSTALL_PREFIX})
set(datarootdir \${prefix}/share)
set(datadir \${datarootdir})
if(GECODE_ENABLE_FLATZINC)
  if(WIN32)
    configure_file(${PROJECT_SOURCE_DIR}/tools/flatzinc/mzn-gecode.bat.in
                   ${PROJECT_BINARY_DIR}/tools/flatzinc/mzn-gecode.bat @ONLY)
    set(MZN_SCRIPT ${PROJECT_BINARY_DIR}/tools/flatzinc/mzn-gecode.bat)
  else()
    configure_file(${PROJECT_SOURCE_DIR}/tools/flatzinc/mzn-gecode.in
                   ${PROJECT_BINARY_DIR}/tools/flatzinc/mzn-gecode @ONLY)
    set(MZN_SCRIPT ${PROJECT_BINARY_DIR}/tools/flatzinc/mzn-gecode)
  endif()
  configure_file(${PROJECT_SOURCE_DIR}/tools/flatzinc/gecode.msc.in
                 ${PROJECT_BINARY_DIR}/tools/flatzinc/gecode.msc @ONLY)
  if(GECODE_BUILD_GIST_TARGET)
    configure_file(${PROJECT_SOURCE_DIR}/tools/flatzinc/gecode-gist.msc.in
                   ${PROJECT_BINARY_DIR}/tools/flatzinc/gecode-gist.msc @ONLY)
  endif()
endif()

if(GECODE_ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif()

# ---------------------------------------------------------------------------
# Installation + CMake package export
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ${GECODE_INSTALL_TARGETS}
  EXPORT GecodeTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(GECODE_ENABLE_FLATZINC)
  install(FILES ${MZN_SCRIPT} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(DIRECTORY gecode
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
    PATTERN "**.hh"
    PATTERN "**.hpp"
    PATTERN "LICENSE_1_0.txt"
    PATTERN "mznlib" EXCLUDE
    PATTERN "exampleplugin" EXCLUDE
    PATTERN "standalone-example" EXCLUDE
    PATTERN "abi*" EXCLUDE)

install(FILES ${PROJECT_BINARY_DIR}/gecode/support/config.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gecode/support/)

if(GECODE_ENABLE_FLATZINC)
  install(DIRECTORY gecode/flatzinc/mznlib/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/minizinc/gecode)

  install(FILES ${PROJECT_BINARY_DIR}/tools/flatzinc/gecode.msc
    DESTINATION ${CMAKE_INSTALL_DATADIR}/minizinc/solvers)
  if(GECODE_BUILD_GIST_TARGET)
    install(FILES ${PROJECT_BINARY_DIR}/tools/flatzinc/gecode-gist.msc
      DESTINATION ${CMAKE_INSTALL_DATADIR}/minizinc/solvers)
  endif()
endif()

set(GECODE_PACKAGE_NEEDS_THREADS OFF)
if(GECODE_ENABLE_THREAD)
  set(GECODE_PACKAGE_NEEDS_THREADS ON)
endif()
set(GECODE_PACKAGE_QT_MAJOR "")
set(GECODE_PACKAGE_QT_COMPONENTS "")
if(GECODE_BUILD_GIST_TARGET)
  set(GECODE_PACKAGE_QT_COMPONENTS "Core;Gui;Widgets;PrintSupport")
elseif(GECODE_ENABLE_FLATZINC AND GECODE_HAS_QT_RUNTIME)
  set(GECODE_PACKAGE_QT_COMPONENTS "Core")
endif()
if(NOT GECODE_PACKAGE_QT_COMPONENTS STREQUAL "")
  if(Qt6_FOUND)
    set(GECODE_PACKAGE_QT_MAJOR "6")
  elseif(Qt5_FOUND)
    set(GECODE_PACKAGE_QT_MAJOR "5")
  endif()
endif()

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GecodeConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/GecodeConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Gecode)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/GecodeConfigVersion.cmake
  VERSION ${GECODE_PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(EXPORT GecodeTargets
  FILE GecodeTargets.cmake
  NAMESPACE Gecode::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Gecode)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/GecodeConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/GecodeConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Gecode)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
