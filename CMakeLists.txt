# To use this, do the following the first time:
#   % mkdir build
#   % cd build
#   % cmake .. -DCMAKE_BUILD_TYPE=release -DMAKE_EXAMPLES=1
#   % make (install)
#
# To only build C libraries and executables, replace the 3rd command above with
#   % cmake .. -DCMAKE_BUILD_TYPE=release -DC_ONLY=1
#
# After doing this once can do the following to recompile
#  % cd <top level evio dir>
#  % cmake --build build (--target install)
#
# The installation will default to the CODA env variable if it's defined
# This can be overridden on command line (in build dir) with:
#   % cmake -DCODA_INSTALL=<my_dir> ..
#   % make install

# This call must be placed BEFORE "project" command
cmake_minimum_required(VERSION 3.13)

# in cmake v 3.30 and later, new way to look for boost
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(evio VERSION 6.0)

# This is better to be on the top of CMake!
include(GNUInstallDirs)

# Allow users to set C++ standard via -DCMAKE_CXX_STANDARD
# Only default to C++11 if not already set
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_DEBUG_POSTFIX -dbg)
add_compile_options(-Wall)

# Add custom find_package for Disruptor
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Build examples by default, but allow disabling
option(MAKE_EXAMPLES "Build examples and tests" ON)
option(C_ONLY "Build only C library (no C++)" OFF)

# Option to enable/disable usage of <filesystem>
option(USE_FILESYSTEMLIB "Use C++17 <filesystem> instead of Boost" OFF)

# Later, when creating your library or executable, add a compile definition if it's ON
if(USE_FILESYSTEMLIB)
    add_compile_definitions(USE_FILESYSTEMLIB=1)
endif()

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
    # Always define installation, even without CODA, using the default CMAKE_INSTALL_PREFIX
    set(INSTALL_DIR_DEFINED 1)
    message(STATUS "No CODA environment - will install to default location: ${CMAKE_INSTALL_PREFIX}")
    # Don't set CMAKE_INSTALL_PREFIX here - use the CMake default
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
        src/libsrc/eviocopy.c
)

set(CPP_HEADER_FILES
        src/libsrc++/FileWritingSupport.h
        src/libsrc++/FileEventIndex.h
        src/libsrc++/EvioException.h
        src/libsrc++/ByteOrder.h
        src/libsrc++/ByteBuffer.h
        src/libsrc++/HeaderType.h
        src/libsrc++/Compressor.h
        src/libsrc++/FileHeader.h
        src/libsrc++/RecordHeader.h
        src/libsrc++/RecordInput.h
        src/libsrc++/RecordOutput.h
        src/libsrc++/Writer.h
        src/libsrc++/WriterMT.h
        src/libsrc++/EvioNode.h
        src/libsrc++/DataType.h
        src/libsrc++/StructureType.h
        src/libsrc++/RecordNode.h
        src/libsrc++/Reader.h
        src/libsrc++/RecordSupply.h
        src/libsrc++/RecordRingItem.h
        src/libsrc++/Util.h
        src/libsrc++/EventWriter.h
        src/libsrc++/EventWriterV4.h
        src/libsrc++/RecordCompressor.h
        src/libsrc++/BaseStructure.h
        src/libsrc++/BaseStructureHeader.h
        src/libsrc++/CompositeData.h
        src/libsrc++/BankHeader.h
        src/libsrc++/SegmentHeader.h
        src/libsrc++/TagSegmentHeader.h
        src/libsrc++/EvioBank.h
        src/libsrc++/EvioSegment.h
        src/libsrc++/EvioTagSegment.h
        src/libsrc++/EvioEvent.h
        src/libsrc++/EventParser.h
        src/libsrc++/IEvioFilter.h
        src/libsrc++/IEvioListener.h
        src/libsrc++/EventParser.h
        src/libsrc++/EventHeaderParser.h
        src/libsrc++/StructureTransformer.h
        src/libsrc++/IBlockHeader.h
        src/libsrc++/IEvioReader.h
        src/libsrc++/EvioReader.h
        src/libsrc++/EvioReaderV4.h
        src/libsrc++/BlockHeaderV2.h
        src/libsrc++/BlockHeaderV4.h
        src/libsrc++/EvioReaderV6.h
        src/libsrc++/EvioCompactReader.h
        src/libsrc++/IEvioCompactReader.h
        src/libsrc++/EvioXMLDictionary.h
        src/libsrc++/EvioDictionaryEntry.h
        src/libsrc++/pugixml.hpp
        src/libsrc++/pugiconfig.hpp
        src/libsrc++/EvioCompactReaderV4.h
        src/libsrc++/EvioCompactReaderV6.h
        src/libsrc++/EvioCompactStructureHandler.h
        src/libsrc++/EventBuilder.h
        src/libsrc++/CompactEventBuilder.h
        src/libsrc++/EvioSwap.h
        src/libsrc++/StructureFinder.h
        src/libsrc++/eviocc.h
        src/test/TestBase.h
)

set(CPP_LIB_FILES
        src/libsrc++/FileEventIndex.cpp
        src/libsrc++/ByteOrder.cpp
        src/libsrc++/ByteBuffer.cpp
        src/libsrc++/HeaderType.cpp
        src/libsrc++/Compressor.cpp
        src/libsrc++/FileHeader.cpp
        src/libsrc++/RecordHeader.cpp
        src/libsrc++/RecordInput.cpp
        src/libsrc++/RecordOutput.cpp
        src/libsrc++/Writer.cpp
        src/libsrc++/WriterMT.cpp
        src/libsrc++/EvioNode.cpp
        src/libsrc++/DataType.cpp
        src/libsrc++/StructureType.cpp
        src/libsrc++/Reader.cpp
        src/libsrc++/RecordSupply.cpp
        src/libsrc++/RecordRingItem.cpp
        src/libsrc++/EventWriter.cpp
        src/libsrc++/EventWriterV4.cpp
        src/libsrc++/BaseStructure.cpp
        src/libsrc++/BaseStructureHeader.cpp
        src/libsrc++/CompositeData.cpp
        src/libsrc++/BankHeader.cpp
        src/libsrc++/SegmentHeader.cpp
        src/libsrc++/TagSegmentHeader.cpp
        src/libsrc++/EventParser.cpp
        src/libsrc++/EvioReader.cpp
        src/libsrc++/EvioReaderV4.cpp
        src/libsrc++/EvioReaderV6.cpp
        src/libsrc++/EvioCompactReader.cpp
        src/libsrc++/EvioXMLDictionary.cpp
        src/libsrc++/pugixml.cpp
        src/libsrc++/EvioCompactReaderV4.cpp
        src/libsrc++/EvioCompactReaderV6.cpp
        src/libsrc++/EvioCompactStructureHandler.cpp
        src/libsrc++/EventBuilder.cpp
        src/libsrc++/CompactEventBuilder.cpp
        src/test/TestBase.cpp
)

set(TEST
        src/test/CompactBuilder_Test.cpp
        src/test/CompactReaderAddSubtractTest.cpp
        src/test/CompactReaderBugTest.cpp
        src/test/CompositeTester.cpp
        src/test/EvioCompStructHandlerTest.cpp
        src/test/DictTest.cpp
        src/test/Hipo_Test.cpp
        src/test/IndexArrayLenTest.cpp
        src/test/ReadWriteTest.cpp
        src/test/ReadWriteV4Test.cpp
        src/test/RecordSupplyTest.cpp
        src/test/RingBufferTest.cpp
        src/test/Tree_Buf_Composite_Builder_Test.cpp
        src/test/hallDEventFiles.cpp
        src/test/FileWriteTest.cpp
        src/test/SwapTest.cpp
        src/test/ReadSequenceTest.cpp
        src/test/SequentialReaderTest.cpp
        src/test/SplitTest2.cpp
        )

set(TESTC
        src/testC/evReadPipe.c
        src/testC/evtest.c
        src/testC/evtest2.c
        src/testC/evtest3.c
        src/testC/evtestAppend.c
        src/testC/evtestBuf.c
        src/testC/evtestBuf2.c
        src/testC/evTestFile.c
        src/testC/evtestWriteFile.c
        src/testC/evtestRead.c
        src/testC/evtestRio.c
        src/testC/evtestSock.c
        src/testC/evWritePipe.c
        src/testC/splitTest.c
        src/testC/splitNameTest.c
)

include(FindPackageHandleStandardArgs)

# search for boost libs
find_package(Boost REQUIRED COMPONENTS system thread chrono filesystem)

if (Boost_FOUND)
    message(STATUS "Boost Found: libs = " ${Boost_LIBRARIES} ", include dirs = " ${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
    include_directories(${Boost_INCLUDE_DIRS})
elseif(NOT C_ONLY)
    message(FATAL_ERROR "Boost cannot be found, cmake will exit." )
endif()

# --------------------------------------------------
# LZ4 detection
# --------------------------------------------------
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

include(FindPackageHandleStandardArgs)
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


# If building C++ version, handle Disruptor dependency
if(NOT C_ONLY)

    find_package(Disruptor REQUIRED)  # or find_package(Disruptor)

    # If found, it defines:
    #   DISRUPTOR_FOUND, DISRUPTOR_INCLUDE_DIR, DISRUPTOR_LIBRARIES
    # Possibly also the library target if you do so inside the subdirectory
    if(DISRUPTOR_FOUND)
        message(STATUS "Disruptor found: library=${DISRUPTOR_LIBRARIES}, include=${DISRUPTOR_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "Could not find Disruptor. Please set DISRUPTOR_DIR or DISRUPTOR_CPP_HOME, or enable fetch by DISRUPTOR_ALLOW_FETCH option")
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
elseif(NOT C_ONLY)
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


# Uninstall target
if(NOT TARGET uninstall)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()

# Options printout
message(STATUS "Options table: ")
message(STATUS "   C_ONLY               : ${C_ONLY}")
message(STATUS "   MAKE_EXAMPLES        : ${MAKE_EXAMPLES}")
message(STATUS "   USE_FILESYSTEMLIB    : ${USE_FILESYSTEMLIB}")
message(STATUS "   DISRUPTOR_ALLOW_FETCH: ${DISRUPTOR_ALLOW_FETCH}")
