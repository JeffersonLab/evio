# FindDisruptor.cmake
#
# This module defines the following (on success):
#   DISRUPTOR_FOUND           - Boolean indicating if Disruptor is found
#   DISRUPTOR_INCLUDE_DIR     - Top-level include directory (so #include "Disruptor/Sequence.h" works)
#   DISRUPTOR_LIBRARIES       - Library or target(s) to link against

include(FetchContent)

option(DISRUPTOR_ALLOW_FETCH "Allow FetchContent fallback if Disruptor is not found locally" ON)

# 1. Check if user explicitly provided DISRUPTOR_DIR or DISRUPTOR_CPP_HOME on command line
if(DISRUPTOR_DIR)
    set(_DISRUPTOR_HINTS "${DISRUPTOR_DIR}")
elseif(DISRUPTOR_CPP_HOME)
    set(_DISRUPTOR_HINTS "${DISRUPTOR_CPP_HOME}")
endif()

# 2. If no hints set, check environment
if(NOT _DISRUPTOR_HINTS AND DEFINED ENV{DISRUPTOR_CPP_HOME})
    set(_DISRUPTOR_HINTS "$ENV{DISRUPTOR_CPP_HOME}")
endif()

# Provide typical subpaths to search
set(_DISRUPTOR_SEARCH_PATHS
        "${_DISRUPTOR_HINTS}"
        "${_DISRUPTOR_HINTS}/include"
        "${_DISRUPTOR_HINTS}/include/Disruptor"
        "${_DISRUPTOR_HINTS}/Disruptor"
)

# Look for a file that definitely lives inside "Disruptor/" subdirectory
# so we get the PARENT folder as DISRUPTOR_INCLUDE_DIR
find_path(
        DISRUPTOR_INCLUDE_DIR
        NAMES Disruptor/Sequence.h
        PATHS ${_DISRUPTOR_SEARCH_PATHS}
        NO_DEFAULT_PATH
)

find_library(
        DISRUPTOR_LIBRARY
        NAMES Disruptor
        PATHS
        "${_DISRUPTOR_HINTS}"
        "${_DISRUPTOR_HINTS}/lib"
        "${_DISRUPTOR_HINTS}/build/Disruptor"
)

set(DISRUPTOR_FOUND OFF)
if(DISRUPTOR_INCLUDE_DIR AND DISRUPTOR_LIBRARY)
    set(DISRUPTOR_FOUND ON)
endif()

# 3. If not found locally, optionally FetchContent
if(NOT DISRUPTOR_FOUND AND DISRUPTOR_ALLOW_FETCH)
    message(STATUS "[FindDisruptor.cmake] Falling back to FetchContent for Disruptor...")

    FetchContent_Declare(
            disruptor
            GIT_REPOSITORY https://github.com/JeffersonLab/disruptor-cpp.git
            GIT_TAG master
    )
    FetchContent_GetProperties(disruptor)
    if(NOT disruptor_POPULATED)
        FetchContent_Populate(disruptor)

        # Turn off tests from disruptor-cpp
        set(DISRUPTOR_BUILD_TESTS OFF CACHE BOOL "Don't build Disruptor tests" FORCE)

        add_subdirectory(${disruptor_SOURCE_DIR} ${disruptor_BINARY_DIR})
    else()
        # Already populated in a previous configure
        add_subdirectory(${disruptor_SOURCE_DIR} ${disruptor_BINARY_DIR})
    endif()

    # The disruptor-cpp project creates e.g. 'DisruptorShared'
    # We'll assume that's the main library target
    set(DISRUPTOR_LIBRARY DisruptorShared)
    # The top-level directory is the parent of Disruptor/...
    set(DISRUPTOR_INCLUDE_DIR "${disruptor_SOURCE_DIR}")

    # If you want to install the fetched library and headers:
    if(TARGET DisruptorShared)
        install(TARGETS DisruptorShared
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
        install(DIRECTORY ${disruptor_SOURCE_DIR}/Disruptor/
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Disruptor
                FILES_MATCHING PATTERN "*.h"
        )
    endif()

    set(DISRUPTOR_FOUND ON)
endif()

# If found or fetched, set DISRUPTOR_LIBRARIES
if(DISRUPTOR_FOUND)
    set(DISRUPTOR_LIBRARIES "${DISRUPTOR_LIBRARY}")
endif()

# Provide standard find_package_handle_standard_args
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Disruptor
        REQUIRED_VARS DISRUPTOR_FOUND DISRUPTOR_INCLUDE_DIR DISRUPTOR_LIBRARIES
)

mark_as_advanced(DISRUPTOR_INCLUDE_DIR DISRUPTOR_LIBRARY)
