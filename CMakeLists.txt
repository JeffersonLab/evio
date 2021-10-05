# To use this, do the following the first time:
#   % mkdir build
#   % cd build
#   % cmake .. -DCMAKE_BUILD_TYPE=release
#   % make (install)
#
# To only build C libraries and executables, replace the 3rd cammand above with
#   % cmake .. -DCMAKE_BUILD_TYPE=release -DC_ONLY=1
#
# After doing this once can do the following to recompile
#  % cd <top level evio dir>
#  % cmake --build build (--target install)
#
#
# The installation will default to the CODA env variable if it's defined
# This can be overridden on command line (in build dir) with:
#   % cmake -DCODA_INSTALL=<my_dir> ..
#   % make install


# This call must be placed BEFORE "project" command
cmake_minimum_required(VERSION 3.3)


project(evio VERSION 5.3 LANGUAGES C CXX)

option(C_ONLY "Build C-library only" OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_DEBUG_POSTFIX -dbg)


# Place libs & binaries in build/lib and bin (this is not for installation)
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)


# Set architecture-related string for installation consistent with scons file
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
    message(STATUS "Set the \"CODA\" env var or specify -DCODA_INSTALL=... on the command line or no installation possible!")
endif()


set(C_HEADER_FILES
        src/libsrc/evio.h
        src/libsrc/msinttypes.h
        )

set(C_LIB_FILES
        src/libsrc/evio.c
        src/libsrc/evioswap.c
        src/libsrc/eviofmt.c
        src/libsrc/eviofmtdump.c
        src/libsrc/eviofmtswap.c
        )

set(CPP_HEADER_FILES
        src/libsrc++/evioBankIndex.hxx
        src/libsrc++/evioBufferChannel.hxx
        src/libsrc++/evioChannel.hxx
        src/libsrc++/evioDictEntry.hxx
        src/libsrc++/evioDictionary.hxx
        src/libsrc++/evioException.hxx
        src/libsrc++/evioFileChannel.hxx
        src/libsrc++/evioSocketChannel.hxx
        src/libsrc++/evioTypedefs.hxx
        src/libsrc++/evioUtil.hxx
        src/libsrc++/evioUtilTemplates.hxx
        )

set(CPP_LIB_FILES
        src/libsrc++/evioBankIndex.cc
        src/libsrc++/evioBufferChannel.cc
        src/libsrc++/evioDictEntry.cc
        src/libsrc++/evioDictionary.cc
        src/libsrc++/evioException.cc
        src/libsrc++/evioFileChannel.cc
        src/libsrc++/evioSocketChannel.cc
        src/libsrc++/evioUtil.cc
        )


set(TESTCC
        src/test/elliottTest.cc
        )


set(TESTC
        src/test/evReadPipe.c
        src/test/evtest.c
        src/test/evtest2.c
        src/test/evtest3.c
        src/test/evtestAppend.c
        src/test/evtestBuf.c
        src/test/evtestBuf2.c
        src/test/evTestFile.c
        src/test/evTestFirstEvent.c
        src/test/evTestHandles.c
        src/test/evtestRead.c
        src/test/evtestRio.c
        src/test/evtestSock.c
        src/test/evWritePipe.c
        src/test/splitTest.c
        src/test/stringTest.c
        )


find_package(Doxygen)


if( DOXYGEN_FOUND )
    message(STATUS "Doxygen found")

    # Change these values from cmake defaults in the Doxyfile
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_REFERENCED_BY_RELATION YES)
    set(DOXYGEN_EXCLUDE_PATTERNS */.* */CMakeFiles/* */_CPack_Packages/* CMakeLists.txt CMakeCache.txt)
    set(DOXYGEN_FILE_PATTERNS *.c *.cpp *.h)
    #set(DOXYGEN_WARN_LOGFILE doxyerrors.log)
    set(DOXYGEN_SORT_MEMBERS_CTORS_1ST YES)
    set(DOXYGEN_SHOW_GROUPED_MEMB_INC YES)
    set(DOXYGEN_INLINE_IHERITED_MEMB YES)
    set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/CC)

    # Add target of "docCC"
    doxygen_add_docs(docCC src/libsrc++)

    # Change these values from cmake defaults in the Doxyfile
    set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/C)

    # Add target of "docC"
    doxygen_add_docs(docC src/libsrc )


elseif(NOT C_ONLY)
    message(FATAL_ERROR "Doxygen NOT found, cmake will exit." )
endif()


# Shared evio C library
add_library(evio SHARED ${C_LIB_FILES})
set_target_properties(evio PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
target_compile_options(evio PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Wall>
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall -Wextra -Wno-unused-parameter>)
include_directories(evio PUBLIC src/libsrc /usr/local/include)


if (NOT C_ONLY)
    # Shared evio C++ library
    add_library(eviocc SHARED ${CPP_LIB_FILES})
    set_target_properties(eviocc PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
    target_compile_options(eviocc PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Wall>
      $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall -Wextra -Wno-unused-parameter>)
    target_link_libraries(eviocc expat evio)
    include_directories(eviocc PUBLIC src/libsrc src/libsrc++ /usr/local/include)

    # Test programs
    if (DEFINED MAKE_EXAMPLES)
        foreach(fileName ${TESTCC})
            # Get file name with no directory or extension as executable name
            get_filename_component(execName ${fileName} NAME_WE)
            # Create executable from file
            add_executable(${execName} ${fileName})
            # Put debug extension on if applicable
            set_target_properties(${execName} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
            # Needs these libs
            target_link_libraries(${execName} evio eviocc)

            # Only install if installation directory has been defined
            if (DEFINED INSTALL_DIR_DEFINED)
                # Install into bin/testC dir
                install(TARGETS ${execName} RUNTIME DESTINATION bin/test)
            endif()
        endforeach(fileName)
    endif()

else()
    message(STATUS "NOT compiling C++ evio library!" )
endif()


# C Test programs
if (DEFINED MAKE_EXAMPLES)
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
        if (DEFINED INSTALL_DIR_DEFINED)
            # Install into bin/testC dir
            install(TARGETS ${execName} RUNTIME DESTINATION bin/test)
        endif()
    endforeach(fileName)
endif()


# Only install if installation directory has been defined
if (DEFINED INSTALL_DIR_DEFINED)
    # Installation defaulting to ${CMAKE_INSTALL_PREFIX}/lib or include
    install(TARGETS evio LIBRARY DESTINATION lib)
    install(FILES ${C_HEADER_FILES} DESTINATION include)

    if (NOT C_ONLY)
        install(TARGETS eviocc LIBRARY DESTINATION lib)
        install(FILES ${CPP_HEADER_FILES} DESTINATION include)
        install(FILES ${CPP_HEADER_FILES} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()
endif()


# Uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

