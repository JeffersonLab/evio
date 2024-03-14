# -- This was mostly written by chatGPT piecewise in response to several prompts.

cmake_minimum_required(VERSION 3.16)
project(EvioProject)

# Add the evio library
set(EVIO_SRC
    src/libsrc/evio.c
    src/libsrc/eviofmt.c
    src/libsrc/eviofmtswap.c
    src/libsrc/evioswap.c
)
add_library(evio SHARED ${EVIO_SRC})
# target_include_directories(evio PUBLIC src/libsrc)
target_include_directories(evio PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/libsrc>
                           $<INSTALL_INTERFACE:include/evio>)
target_link_libraries(evio PUBLIC expat)


# Add the evio++ library
set(EVIO_PP_SRC
    src/libsrc++/evioBankIndex.cc
    src/libsrc++/evioBufferChannel.cc
    src/libsrc++/evioDictionary.cc
    src/libsrc++/evioException.cc
    src/libsrc++/evioFileChannel.cc
    src/libsrc++/evioSocketChannel.cc
    src/libsrc++/evioUtil.cc
)
add_library(evio++ SHARED ${EVIO_PP_SRC})
# target_include_directories(evio++ PUBLIC src/libsrc++ src/libsrc)
target_include_directories(evio++ PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/libsrc++>
                           $<INSTALL_INTERFACE:include/evio>)
target_link_libraries(evio++ PUBLIC evio expat)

# Optional: Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Install the evio library
install(TARGETS evio
        EXPORT evioTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

# Install the evio headers
file(GLOB LIBSRC_HEADERS "src/libsrc/*.h")
install(FILES ${LIBSRC_HEADERS} DESTINATION include/evio)

# Install the evio++ library
install(TARGETS evio++
        EXPORT evioTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

# Install the evio++ headers
file(GLOB LIBSRCPP_HEADERS "src/libsrc++/*.hxx" "src/libsrc++/*.hpp")
install(FILES ${LIBSRCPP_HEADERS} DESTINATION include/evio)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "evioConfigVersion.cmake"
    VERSION 4.3
    COMPATIBILITY AnyNewerVersion
)

export(EXPORT evioTargets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/evioTargets.cmake"
)

configure_file(cmake/evioConfig.cmake
               "${CMAKE_CURRENT_BINARY_DIR}/evioConfig.cmake"
               COPYONLY)

install(EXPORT evioTargets
        FILE evioTargets.cmake
        DESTINATION lib/cmake/evio)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/evioConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/evioConfigVersion.cmake"
        DESTINATION lib/cmake/evio)