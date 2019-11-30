cmake_minimum_required (VERSION 2.6)

project (acg_localizer)

# add our macro directory to cmake search path
if (EXISTS "${CMAKE_SOURCE_DIR}/cmake")
  set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
endif ()

if ( NOT ENABLE_TESTS )
    set (ENABLE_TESTS OFF CACHE BOOL "Turn on testing framework.")
endif()

if ( NOT STL_VECTOR_CHECKS )
    set ( STL_VECTOR_CHECKS false CACHE BOOL "Include full stl vector checks in debug mode (This option is only used in debug Mode!)" )
endif ( NOT STL_VECTOR_CHECKS )

if ( STL_VECTOR_CHECKS )
    add_definitions (
        -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
    )
endif ()

# use sse4_2
if (CMAKE_C_COMPILER MATCHES ".*gcc.*$")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2")
endif()


if (CMAKE_C_COMPILER MATCHES ".*icc.*$")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ip -axSSE4.2,SSSE3,SSE2 -fp-model fast=2")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ip -axSSE4.2,SSSE3,SSE2 -fp-model fast=2")
endif()


set( CMAKE_DEBUG_POSTFIX "d" )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wall  -O3 -march=native ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall   -O3 -march=native")

# Check C++11 or C++0x support
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
   add_definitions(-DCOMPILEDWITHC11)
   message(STATUS "Using flag -std=c++11.")
elseif(COMPILER_SUPPORTS_CXX0X)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
   add_definitions(-DCOMPILEDWITHC0X)
   message(STATUS "Using flag -std=c++0x.")
else()
   message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()


add_subdirectory (src)

# testing
if( ENABLE_TESTS )
    add_subdirectory (test)
endif()

