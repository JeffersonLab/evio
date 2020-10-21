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

project(evio_6_0)

# Find include files here
include_directories(src/libsrc src/libsrc++ src/libsrc /usr/include/boost /usr/local/include $ENV{DISRUPTOR_CPP_HOME})

# Place libs & binaries in somewhere besides build/cmake - like in allBinaries subdirectory
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/allBinaries)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/allBinaries)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -lexpat -ldl -lz -lm")


set(CPP_HEADER_FILES_NEW
        src/libsrc/FileEventIndex.h
        src/libsrc/HipoException.h
        src/libsrc/EvioException.h
        src/libsrc/ByteOrder.h
        src/libsrc/ByteBuffer.h
        src/libsrc/ByteBufferVector.h
        src/libsrc/HeaderType.h
        src/libsrc/Stoppable.h
        src/libsrc/ConcurrentFixedQueue.h
        src/libsrc/Compressor.h
        src/libsrc/CompressionType.h
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
        src/libsrc/EvioNodeSource.h
        src/libsrc/TestWriter.h
        src/libsrc/ReadWriteTest.h
        src/libsrc/Reader.h
        src/libsrc/RecordSupply.h
        src/libsrc/RecordRingItem.h
        src/libsrc/Util.h
        src/libsrc/EventWriter.h
        src/libsrc/RecordWriter.h
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
        src/libsrc/INameProvider.h
        src/libsrc/EvioCompactReaderV4.h
        src/libsrc/BlockNode.h
        src/libsrc/EvioCompactReaderV6.h
        src/libsrc/EventBuilder.h
        src/libsrc/CompactEventBuilder.h
        src/libsrc/EvioSwap.h
        src/libsrc/StructureFinder.h
        src/libsrc/evio.h
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


#set(CPP_LIB_FILES_DISRUPTOR
#        src/disruptorTest/claim_strategy_test.cc
#        src/disruptorTest/ring_buffer_test.cc
#        src/disruptorTest/sequence_barrier_test.cc
#        src/disruptorTest/sequence_test.cc
#        src/disruptorTest/sequencer_test.cc
#        src/disruptorTest/wait_strategy_test.cc
#        src/disruptorTest/benchmark/one_publisher_to_one_unicast_throughput_test.cc
#        src/disruptorTest/benchmark/one_publisher_to_three_pipeline_throughput_test.cc
#        )



#file(GLOB TEST_FILES_CC src/test/*.cc)


# Shared evio C++ library
add_library(evio SHARED ${CPP_LIB_FILES_NEW})
target_include_directories(evio PUBLIC src/libsrc /usr/local/include)


# Main Executables
add_executable(ReadWriteTest src/test/ReadWriteTest.cpp)
target_link_libraries(ReadWriteTest pthread expat z hipo)

add_executable(RingBufferTest src/test/RingBufferTest.cpp)
target_link_libraries(RingBufferTest pthread expat z hipo)


## Test programs
#foreach(fileName ${TEST_FILES_C})
#    # Get file name with no directory or extension as executable name
#    get_filename_component(execName ${fileName} NAME_WE)
#    # Create executable from file
#    add_executable(${execName} ${fileName})
#    # Needs these libs
#    target_link_libraries(${execName} evio)
#    # Install into bin/test dir
#    install(TARGETS ${execName} DESTINATION bin/test)
#endforeach(fileName)


# Installation relative to build/cmake
install(TARGETS evio DESTINATION lib)
install(FILES ${CPP_HEADER_FILES_NEW} DESTINATION inc)
