# See instructions in README.md for building and installing

### VERSION DEFINITIONS ###
cmake_minimum_required(VERSION 3.22)
project(evio VERSION 6.1.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
# C/C++ build options
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_DEBUG_POSTFIX -dbg)
add_compile_options(-Wall) # Enable all warnings

# Includes
include(GNUInstallDirs)
include(FindPackageHandleStandardArgs) # find_package_handle_standard_args()
include(CTest) 
include(CMakePackageConfigHelpers)

# Build option parameters (and defaults)
option(C_ONLY                "SKIP building C++ library, build C only" OFF)
option(MAKE_EXAMPLES         "Build example/test programs" OFF)
option(USE_FILESYSTEMLIB     "Use C++ <filesystem> instead of Boost" OFF)
option(DISRUPTOR_FETCH       "Allow CMake to download Disruptor if not found" ON)

# Add custom find_package for Disruptor
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Add a compile definition if USE_FILESYSTEMLIB set, for when creating your library or executable
if(USE_FILESYSTEMLIB)
    add_compile_definitions(USE_FILESYSTEMLIB=1)
endif()

# Place libs & binaries in build/lib and bin (this is not for installation)
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# Set architecture-related string for installation (consistent with scons)
set(ARCH ${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR})
message(STATUS "ARCH = " ${ARCH})

# Install into $CODA_INSTALL directory
if (DEFINED CODA_INSTALL)
    message(STATUS "CODA_INSTALL = " ${CODA_INSTALL})
    set(INSTALL_DIR_DEFINED 1)
    set(CMAKE_INSTALL_PREFIX ${CODA_INSTALL}/${ARCH})
    set(CMAKE_INSTALL_INCLUDEDIR ${CODA_INSTALL}/common/include)
    message(STATUS "CMAKE_INSTALL_PREFIX reset to " ${CMAKE_INSTALL_PREFIX})
    # Install into $CODA directory
elseif (DEFINED ENV{CODA})
    message(STATUS "CODA = " $ENV{CODA})
    set(INSTALL_DIR_DEFINED 1)
    set(CMAKE_INSTALL_PREFIX $ENV{CODA}/${ARCH})
    set(CMAKE_INSTALL_INCLUDEDIR $ENV{CODA}/common/include)
    message(STATUS "CMAKE_INSTALL_PREFIX reset to " ${CMAKE_INSTALL_PREFIX})
else()
    # Use default CMAKE_INSTALL_PREFIX
    set(INSTALL_DIR_DEFINED 1)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/${ARCH})
    set(CMAKE_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include)
    message(STATUS "Installing to default location: ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "Installing includes to default location: ${CMAKE_INSTALL_INCLUDEDIR}")
endif()

# Boost libs
find_package(Boost REQUIRED COMPONENTS system thread chrono filesystem)
if (Boost_FOUND)
    message(STATUS "Boost Found: libs = " ${Boost_LIBRARIES} ", include dirs = " ${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
    include_directories(${Boost_INCLUDE_DIRS})
elseif(NOT C_ONLY)
    message(FATAL_ERROR "Boost cannot be found, cmake will exit." )
endif()

# LZ4 library
find_path(LZ4_INCLUDE_DIR
        NAMES lz4.h
        PATHS /usr/local/include /usr/include /usr/include/linux
)
find_library(LZ4_LIBRARY
        NAMES lz4 liblz4
        PATHS
        /usr/local/lib
        /usr/lib64
        /usr/lib
        /lib/x86_64-linux-gnu
        /usr/lib/x86_64-linux-gnu
        /usr/lib/x86_64-linux-gnu/mesa
)
find_package_handle_standard_args(LZ4
        REQUIRED_VARS LZ4_LIBRARY LZ4_INCLUDE_DIR
        HANDLE_COMPONENTS
)
if(NOT LZ4_FOUND)
    message(STATUS "LZ4 NOT found with the initial search; trying alternative approach ...")
    # Fallback or manual search logic if you have other unique paths or want
    # to do something different. Or just fail here if that's your preference.
    # e.g. message(FATAL_ERROR "No LZ4 found!")
else()
    message(STATUS "LZ4 Found: library = ${LZ4_LIBRARY}, include = ${LZ4_INCLUDE_DIR}")
endif()

# C source files
file(GLOB C_HEADER_FILES "src/libsrc/*.h")
file(GLOB C_LIB_FILES "src/libsrc/*.c")
# C++ source files
file(GLOB CPP_LIB_FILES "src/libsrc++/*.cpp")
file(GLOB CPP_HEADER_FILES "src/libsrc++/*.h")
# C++ utility files
file(GLOB CPP_UTILS_FILES "src/utils/cpp/*.cpp")
# A few extras required for examples
if(MAKE_EXAMPLES)
    list(APPEND CPP_HEADER_FILES src/test/cpp/EvioTestHelper.h)
    file(GLOB TEST "src/test/cpp/*.cpp")
    file(GLOB TESTC "src/test/c/*.c")
endif()

# BUILD C++ LIBRARY (unless otherwise specified)
if(NOT C_ONLY)

    find_package(Disruptor REQUIRED)  # or find_package(Disruptor)

    # If found, it defines:
    #   DISRUPTOR_FOUND, DISRUPTOR_INCLUDE_DIR, DISRUPTOR_LIBRARIES
    # Possibly also the library target if you do so inside the subdirectory
    if(DISRUPTOR_FOUND)
        message(STATUS "Disruptor found: library=${DISRUPTOR_LIBRARIES}, include=${DISRUPTOR_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "Could not find Disruptor. Please set DISRUPTOR_CPP_DIR or DISRUPTOR_CPP_HOME, or enable fetch by DISRUPTOR_ALLOW_FETCH option")
    endif()

    # Shared evio C++ library
    add_library(eviocc SHARED ${CPP_LIB_FILES})
    set_target_properties(eviocc PROPERTIES
            DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
            EXPORT_NAME eviocc
    )

    target_link_libraries(eviocc PUBLIC
            ${LZ4_LIBRARY}
            ${Boost_LIBRARIES}
            ${DISRUPTOR_LIBRARY}
    )

    target_include_directories(eviocc PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/libsrc++>
            $<INSTALL_INTERFACE:include>
            /usr/local/include
            ${Boost_INCLUDE_DIRS}
            ${LZ4_INCLUDE_DIRS}
            ${DISRUPTOR_INCLUDE_DIR}
    )

    # Build utility programs
    foreach(fileName ${CPP_UTILS_FILES})
        # Get file name with no directory or extension as executable name
        get_filename_component(execName ${fileName} NAME_WE)
        # Create executable from file
        add_executable(${execName} ${fileName})
        # Put debug extension on if applicable
        set_target_properties(${execName} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
        # Needs these libs
        target_link_libraries(${execName} eviocc pthread ${Boost_LIBRARIES} ${LZ4_LIBRARY} expat dl z m)

        # Only install if installation directory has been defined
        if(DEFINED INSTALL_DIR_DEFINED)
            message(STATUS "Installing utility executable: ${execName}")
            # Install into bin/utils dir
            install(TARGETS ${execName} RUNTIME DESTINATION bin/utils)
        endif()
    endforeach()

    # Add the C++ tests/examples
    if(MAKE_EXAMPLES)
        foreach(fileName ${TEST})
            # Get file name with no directory or extension as executable name
            get_filename_component(execName ${fileName} NAME_WE)
            # Create executable from file
            add_executable(${execName} ${fileName})
            # Put debug extension on if applicable
            set_target_properties(${execName} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
            # Needs these libs
            target_link_libraries(${execName} eviocc pthread ${Boost_LIBRARIES} ${LZ4_LIBRARY} expat dl z m)

            # Only install if installation directory has been defined
            if(DEFINED INSTALL_DIR_DEFINED)
                # Install into bin/test dir
                install(TARGETS ${execName} RUNTIME DESTINATION bin/test)
            endif()
        endforeach()
    endif()
else()
    message(STATUS "NOT compiling C++ evio library (C_ONLY=ON)")
endif()

# Shared evio C library
add_library(evio SHARED ${C_LIB_FILES})
set_target_properties(evio PROPERTIES
        DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
        EXPORT_NAME evio
)
target_include_directories(evio PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/libsrc>
        $<INSTALL_INTERFACE:include>
        /usr/local/include
)

# C Test programs
if(MAKE_EXAMPLES)
    foreach(fileName ${TESTC})
        # Get file name with no directory or extension as executable name
        get_filename_component(execName ${fileName} NAME_WE)
        # Create executable from file
        add_executable(${execName} ${fileName})
        # Put debug extension on if applicable
        set_target_properties(${execName} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
        # Needs these libs
        target_link_libraries(${execName} evio pthread)

        # Only install if installation directory has been defined
        if(DEFINED INSTALL_DIR_DEFINED)
            # Install into bin/test dir
            install(TARGETS ${execName} RUNTIME DESTINATION bin/test)
        endif()
    endforeach()
endif()

# Find Doxygen for documentation (only required for C++ build)
find_package(Doxygen)

if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found")

    # Change these values from cmake defaults in the Doxyfile
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_REFERENCED_BY_RELATION YES)
    set(DOXYGEN_EXCLUDE_PATTERNS */.* */CMakeFiles/* */_CPack_Packages/* CMakeLists.txt CMakeCache.txt)
    set(DOXYGEN_EXCLUDE src/libsrc++/pugixml.cpp src/libsrc++/pugixml.hpp)
    set(DOXYGEN_FILE_PATTERNS *.c *.cpp *.h)
    set(DOXYGEN_SORT_MEMBERS_CTORS_1ST YES)
    set(DOXYGEN_SHOW_GROUPED_MEMB_INC YES)
    set(DOXYGEN_INLINE_IHERITED_MEMB YES)
    set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/CC)

    # Add target of "docCC"
    doxygen_add_docs(docCC src/libsrc++ COMMENT "Generating Doxygen documentation for C++")

    # Change these values from cmake defaults in the Doxyfile
    set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/C)

    # Add target of "docC"
    doxygen_add_docs(docC src/libsrc COMMENT "Generating Doxygen documentation for C")
else()
    message(WARNING "Doxygen NOT found, documentation will not be generated")
endif()



# Basic C library installation - minimal approach
install(TARGETS evio
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${C_HEADER_FILES}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# C++ components if enabled
if(NOT C_ONLY)
    install(TARGETS eviocc
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES ${CPP_HEADER_FILES}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# Install config files for find_package
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/evioConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/evioConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_PREFIX}/evio/cmake"
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/evioConfig.cmake"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/evio/cmake"
)

# Unit testing setup
add_test(NAME EvioWriteAndReadBack_builder COMMAND bin/EvioWriteAndReadBack_builder 10)

# Uninstall target
# Removed for now, not yet compatible with building disruptor-cpp internally
# if(NOT TARGET uninstall)
#     configure_file(
#             "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
#             "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
#             IMMEDIATE @ONLY
#     )

#     add_custom_target(uninstall
#             COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
#     )
# endif()

# Options printout
message(STATUS "Options table: ")
message(STATUS "   C_ONLY               : ${C_ONLY}")
message(STATUS "   MAKE_EXAMPLES        : ${MAKE_EXAMPLES}")
message(STATUS "   USE_FILESYSTEMLIB    : ${USE_FILESYSTEMLIB}")
message(STATUS "   DISRUPTOR_ALLOW_FETCH: ${DISRUPTOR_ALLOW_FETCH}")
