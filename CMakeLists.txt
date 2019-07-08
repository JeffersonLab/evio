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
include_directories(src/libsrc src/libsrc++ src/newlib++)

# Place libs & binaries in somewhere besides build/cmake - like in allBinaries subdirectory
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/allBinaries)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/allBinaries)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -lpthread -lexpat -ldl -lz -lm")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -lexpat -ldl -lz -lm")

set(C_HEADER_FILES
    src/libsrc/evio.h
    src/libsrc/msinttypes.h
)

set(C_LIB_FILES
    src/libsrc/evio.c
    src/libsrc/eviofmt.c
    src/libsrc/eviofmtdump.c
    src/libsrc/eviofmtswap.c
    src/libsrc/evioswap.c)

set(CPP_HEADER_FILES
    src/libsrc++/evioBankIndex.hxx
    src/libsrc++/evioBufferChannel.hxx
    src/libsrc++/evioFileChannel.hxx
    src/libsrc++/evioSocketChannel.hxx
    src/libsrc++/evioChannel.hxx
    src/libsrc++/evioDictionary.hxx
    src/libsrc++/evioException.hxx
    src/libsrc++/evioTypedefs.hxx
    src/libsrc++/evioUtil.hxx
    src/libsrc++/evioUtilTemplates.hxx
    src/libsrc++/evioDictEntry.hxx
)

set(CPP_LIB_FILES
    src/libsrc++/evioBankIndex.cc
    src/libsrc++/evioBufferChannel.cc
    src/libsrc++/evioFileChannel.cc
    src/libsrc++/evioSocketChannel.cc
    src/libsrc++/evioDictionary.cc
    src/libsrc++/evioException.cc
    src/libsrc++/evioUtil.cc
    src/libsrc++/evioDictEntry.cc
)

set(CPP_HEADER_FILES_NEW
        src/newlib++/FileEventIndex.h
        src/newlib++/HipoException.h
        src/newlib++/ByteOrder.h
        src/newlib++/ByteBuffer.h
        src/newlib++/HeaderType.h
        src/newlib++/Stoppable.h
        src/newlib++/ConcurrentFixedQueue.h
        src/newlib++/Compressor.h
        src/newlib++/CompressionType.h
        src/newlib++/FileHeader.h
        src/newlib++/RecordHeader.h
        src/newlib++/RecordInput.h
        src/newlib++/RecordOutput.h
        src/newlib++/Writer.h
        src/newlib++/WriterMT.h
        src/newlib++/ReadWriteTest.h
        )

set(CPP_LIB_FILES_NEW
        src/newlib++/FileEventIndex.cpp
        src/newlib++/ByteBuffer.cpp
        src/newlib++/Compressor.cpp
        src/newlib++/FileHeader.cpp
        src/newlib++/RecordHeader.cpp
        src/newlib++/RecordInput.cpp
        src/newlib++/RecordOutput.cpp
        src/newlib++/Writer.cpp
        src/newlib++/WriterMT.cpp
        src/newlib++/ReadWriteTest.cpp)


file(GLOB TEST_FILES_C src/test/*.c)
file(GLOB TEST_FILES_CC src/test/*.cc)

#file(GLOB EXAMPLE_FILES_C src/examples/*.c)
#file(GLOB EXAMPLE_FILES_CC src/examples/*.cc)


# Shared evio C library
add_library(evio SHARED ${C_LIB_FILES})
target_include_directories(evio PUBLIC src/libsrc)

# Shared evio C++ library
#add_library(evioxx SHARED ${CPP_LIB_FILES})
#target_include_directories(evioxx PUBLIC src/libsrc src/libsrc++)


# Shared hipo C++ library
add_library(hipo SHARED ${CPP_LIB_FILES_NEW})
target_include_directories(hipo PUBLIC src/newlib++)


# Main Executables
add_executable(evio2xml src/execsrc/evio2xml.c)
target_link_libraries(evio2xml pthread expat z evio)

add_executable(eviocopy src/execsrc/eviocopy.c)
target_link_libraries(eviocopy pthread evio)

#add_executable(evioCat  src/execsrc/evioCat.cc)
#target_link_libraries(evioCat evio evioxx)


# Test programs
foreach(fileName ${TEST_FILES_C})
    # Get file name with no directory or extension as executable name
    get_filename_component(execName ${fileName} NAME_WE)
    # Create executable from file
    add_executable(${execName} ${fileName})
    # Needs these libs
    target_link_libraries(${execName} evio)
    # Install into bin/test dir
    install(TARGETS ${execName} DESTINATION bin/test)
endforeach(fileName)


# Example programs
#foreach(fileName ${EXAMPLE_FILES_C})
#    get_filename_component(execName ${fileName} NAME_WE)
#    add_executable(${execName} ${fileName})
#    target_link_libraries(${execName} evio)
#    install(TARGETS ${execName} DESTINATION bin/examples)
#endforeach(fileName)


# Installation relative to build/cmake
#install(TARGETS evio evioxx DESTINATION lib)
install(TARGETS evio hipo DESTINATION lib)
#install(TARGETS evio DESTINATION lib)
#install(TARGETS evio2xml eviocopy evioCat DESTINATION bin)
install(TARGETS evio2xml eviocopy DESTINATION bin)
install(FILES ${C_HEADER_FILES} ${CPP_HEADER_FILES_NEW} DESTINATION inc)
