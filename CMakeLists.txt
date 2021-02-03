cmake_minimum_required(VERSION 3.17)
project(evio VERSION 5.2)
set(CMAKE_CXX_STANDARD 11)


# Place libs & binaries in somewhere besides build/cmake - like in allBinaries subdirectory
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/ccBuild)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/ccBuild)

message("CMAKE_BINARY_DIR = " ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

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


# Shared evio C library
add_library(evio SHARED ${C_LIB_FILES})
include_directories(evio PUBLIC src/libsrc /usr/local/include)


# Shared evio C++ library
add_library(eviocc SHARED ${CPP_LIB_FILES})
target_link_libraries(eviocc expat evio)
include_directories(eviocc PUBLIC src/libsrc src/libsrc++ /usr/local/include)


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


# Test programs
foreach(fileName ${TESTCC})
    # Get file name with no directory or extension as executable name
    get_filename_component(execName ${fileName} NAME_WE)
    # Create executable from file
    add_executable(${execName} ${fileName})
    # Needs these libs
    target_link_libraries(${execName} evio eviocc)
    # Install into bin/test dir
    install(TARGETS ${execName} DESTINATION bin/testC)
endforeach(fileName)


# Installation relative to build/cmake
install(TARGETS evio eviocc DESTINATION lib)
install(FILES ${C_HEADER_FILES} ${CPP_HEADER_FILES} DESTINATION inc)
