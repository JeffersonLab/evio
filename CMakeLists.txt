# To use this, do the following the first time:
#   % mkdir build
#   % mkdir build/cmake
#   % cd build/cmake
#   % cmake ../..
#   % make (install)
#
#
# After doing this once can do the following to recompile
#  % cd <top level evio dir>
#  % cmake --build build/cmake (--target install)
#
#
# Set the path in which to install.
# This can be overridden on command line (in build/cmake dir) with:
#   % cmake -DCMAKE_INSTALL_PREFIX=<my_dir> ../..
#   % make install
# (This call must be placed BEFORE "project" command).
set(CMAKE_INSTALL_PREFIX ./)

cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(evio VERSION 6.0)

# Place libs & binaries in somewhere besides build/cmake - like in allBinaries subdirectory
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/ccBuild)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/ccBuild)

message("CMAKE_BINARY_DIR = " ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(C_HEADER_FILES
        src/libCsrc/evio.h
        src/libCsrc/msinttypes.h
        )

set(C_LIB_FILES
        src/libCsrc/evio.c
        src/libCsrc/evioswap.c
        src/libCsrc/eviofmt.c
        src/libCsrc/eviofmtdump.c
        src/libCsrc/eviofmtswap.c
        src/libCsrc/eviocopy.c
        )

set(CPP_HEADER_FILES_NEW
        src/libsrc/FileEventIndex.h
        src/libsrc/EvioException.h
        src/libsrc/ByteOrder.h
        src/libsrc/ByteBuffer.h
        src/libsrc/HeaderType.h
        src/libsrc/Compressor.h
        src/libsrc/FileHeader.h
        src/libsrc/RecordHeader.h
        src/libsrc/RecordInput.h
        src/libsrc/RecordOutput.h
        src/libsrc/Writer.h
        src/libsrc/WriterMT.h
        src/libsrc/EvioNode.h
        src/libsrc/DataType.h
        src/libsrc/StructureType.h
        src/libsrc/RecordNode.h
        src/libsrc/Reader.h
        src/libsrc/RecordSupply.h
        src/libsrc/RecordRingItem.h
        src/libsrc/Util.h
        src/libsrc/EventWriter.h
        src/libsrc/RecordCompressor.h
        src/libsrc/BaseStructure.h
        src/libsrc/BaseStructureHeader.h
        src/libsrc/CompositeData.h
        src/libsrc/BankHeader.h
        src/libsrc/SegmentHeader.h
        src/libsrc/TagSegmentHeader.h
        src/libsrc/EvioBank.h
        src/libsrc/EvioSegment.h
        src/libsrc/EvioTagSegment.h
        src/libsrc/EvioEvent.h
        src/libsrc/EventParser.h
        src/libsrc/IEvioFilter.h
        src/libsrc/IEvioListener.h
        src/libsrc/EventParser.h
        src/libsrc/EventHeaderParser.h
        src/libsrc/StructureTransformer.h
        src/libsrc/IBlockHeader.h
        src/libsrc/IEvioReader.h
        src/libsrc/EvioReader.h
        src/libsrc/EvioReaderV4.h
        src/libsrc/BlockHeaderV2.h
        src/libsrc/BlockHeaderV4.h
        src/libsrc/EvioReaderV6.h
        src/libsrc/EvioCompactReader.h
        src/libsrc/IEvioCompactReader.h
        src/libsrc/EvioXMLDictionary.h
        src/libsrc/EvioDictionaryEntry.h
        src/libsrc/pugixml.hpp
        src/libsrc/pugiconfig.hpp
        src/libsrc/EvioCompactReaderV4.h
        src/libsrc/EvioCompactReaderV6.h
        src/libsrc/EventBuilder.h
        src/libsrc/CompactEventBuilder.h
        src/libsrc/EvioSwap.h
        src/libsrc/StructureFinder.h
        src/libsrc/eviocc.h
        )

set(CPP_LIB_FILES_NEW
        src/libsrc/FileEventIndex.cpp
        src/libsrc/ByteOrder.cpp
        src/libsrc/ByteBuffer.cpp
        src/libsrc/HeaderType.cpp
        src/libsrc/Compressor.cpp
        src/libsrc/FileHeader.cpp
        src/libsrc/RecordHeader.cpp
        src/libsrc/RecordInput.cpp
        src/libsrc/RecordOutput.cpp
        src/libsrc/Writer.cpp
        src/libsrc/WriterMT.cpp
        src/libsrc/EvioNode.cpp
        src/libsrc/DataType.cpp
        src/libsrc/StructureType.cpp
        src/libsrc/Reader.cpp
        src/libsrc/RecordSupply.cpp
        src/libsrc/RecordRingItem.cpp
        src/libsrc/EventWriter.cpp
        src/libsrc/BaseStructure.cpp
        src/libsrc/BaseStructureHeader.cpp
        src/libsrc/CompositeData.cpp
        src/libsrc/BankHeader.cpp
        src/libsrc/SegmentHeader.cpp
        src/libsrc/TagSegmentHeader.cpp
        src/libsrc/EventParser.cpp
        src/libsrc/EvioReader.cpp
        src/libsrc/EvioReaderV4.cpp
        src/libsrc/EvioReaderV6.cpp
        src/libsrc/EvioCompactReader.cpp
        src/libsrc/EvioXMLDictionary.cpp
        src/libsrc/pugixml.cpp
        src/libsrc/EvioCompactReaderV4.cpp
        src/libsrc/EvioCompactReaderV6.cpp
        src/libsrc/EventBuilder.cpp
        src/libsrc/CompactEventBuilder.cpp)


set(TEST
        src/test/CompactBuilder_Test.cpp
        src/test/Dict_FirstEv_Test.cpp
        src/test/Hipo_Test.cpp
        src/test/ReadWriteTest.cpp
        src/test/RecordSupplyTest.cpp
        src/test/RingBufferTest.cpp
        src/test/Tree_Buf_Composite_Builder_Test.cpp
        src/test/Writer_Reader_Compression_Test.cpp
        src/test/Writer_Reader_Test.cpp
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
        )


include(FindPackageHandleStandardArgs)

# search for boost libs
find_package(Boost REQUIRED system thread chrono)

if (Boost_FOUND)
    message("Boost Found: libs = " ${Boost_LIBRARIES} ", include dirs = " ${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
elseif(NOT Boost_FOUND)
    error("Boost Not Found")
endif()

# search for lz4 libs
find_package_handle_standard_args( LZ4 REQUIRED_VARS LZ4_LIBRARY LZ4_INCLUDE_DIR HANDLE_COMPONENTS)

if( LZ4_FOUND )
  message("LZ4 Found")
  message("LZ4 include directory = ${LZ4_INCLUDE_DIR}")
  message("LZ4 library = ${LZ4_LIBRARY}")
  set( CMAKE_REQUIRED_INCLUDES ${LZ4_INCLUDE_DIRS} )
  set( CMAKE_REQUIRED_LIBRARIES ${LZ4_LIBRARY})
else()
    message("LZ4 NOT Found, try again ...")

    # On Linux lz4 cannot be found for some reason.
    # This is where it is on my system.
    find_path(LZ4_INCLUDE_DIR
            NAMES lz4.h
            PATHS /usr/include
            )

    find_library(LZ4_LIBRARY
            NAMES lz4
            PATHS /usr/lib64
            )

    if(LZ4_INCLUDE_DIR-NOTFOUND)
        error("lz4.h cannot be found")
    endif()

    if (LZ4_LIBRARY-NOTFOUND)
        error("lz4 library cannot be found")
    endif()

    message("Found LZ4 include directory = ${LZ4_INCLUDE_DIR}")
    message("Found LZ4 library = ${LZ4_LIBRARY}")
endif()

# search for disruptor lib
find_path(DISRUPTOR_INCLUDE_DIR
           NAMES lz4.h
           PATHS $ENV{DISRUPTOR_CPP_HOME}/Disruptor
         )

find_library(DISRUPTOR_LIBRARY
             NAMES Disruptor
             PATHS $ENV{DISRUPTOR_CPP_HOME}/build/Disruptor
            )


# Find include files here
#include_directories(src/libsrc src/libCsrc /usr/local/include ${Boost_INCLUDE_DIRS} ${LZ4_INCLUDE_DIR} $ENV{DISRUPTOR_CPP_HOME}/Disruptor)

# Shared evio C library
add_library(evio SHARED ${C_LIB_FILES})
include_directories(evio PUBLIC src/libCsrc /usr/local/include)


# Shared evio C++ library
add_library(eviocc SHARED ${CPP_LIB_FILES_NEW})
target_link_libraries(eviocc ${LZ4_LIBRARY} ${Boost_LIBRARIES} ${DISRUPTOR_LIBRARY})
include_directories(eviocc PUBLIC src/libsrc /usr/local/include
                    ${Boost_INCLUDE_DIRS} ${LZ4_INCLUDE_DIRS} $ENV{DISRUPTOR_CPP_HOME}/Disruptor)


# Main Executables
add_executable(ReadWriteTest src/test/ReadWriteTest.cpp)
target_link_libraries(ReadWriteTest pthread ${Boost_LIBRARIES} expat dl z m ${LZ4_LIBRARY} eviocc)


add_executable(RingBufferTest src/test/RingBufferTest.cpp)
target_link_libraries(RingBufferTest pthread ${Boost_LIBRARIES}  expat dl z m ${LZ4_LIBRARY} eviocc)


# Test programs
foreach(fileName ${TESTC})
    # Get file name with no directory or extension as executable name
    get_filename_component(execName ${fileName} NAME_WE)
    # Create executable from file
    add_executable(${execName} ${fileName})
    # Needs these libs
    target_link_libraries(${execName} evio)
    # Install into bin/test dir
    install(TARGETS ${execName} DESTINATION bin/testC)
endforeach(fileName)


# Installation relative to build/cmake
install(TARGETS evio eviocc DESTINATION lib)
install(FILES ${CPP_HEADER_FILES_NEW} DESTINATION inc)
