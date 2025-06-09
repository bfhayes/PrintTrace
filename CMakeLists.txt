cmake_minimum_required(VERSION 3.16)
project(OutlineTool VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found. Please install OpenCV or set OpenCV_DIR.")
endif()

# Find libdxfrw
find_library(DXFRW_LIBRARY
    NAMES dxfrw libdxfrw
    PATHS
        /usr/local/lib
        /usr/lib
        /opt/homebrew/lib
        ${CMAKE_PREFIX_PATH}/lib
    DOC "Path to the dxfrw library"
)

find_path(DXFRW_INCLUDE_DIR
    NAMES libdxfrw.h drw_interface.h
    PATHS
        /usr/local/include/libdxfrw
        /usr/include/libdxfrw
        /opt/homebrew/include/libdxfrw
        ${CMAKE_PREFIX_PATH}/include/libdxfrw
        /usr/local/include
        /usr/include
        /opt/homebrew/include
        ${CMAKE_PREFIX_PATH}/include
    DOC "Path to the dxfrw headers"
)

if(NOT DXFRW_LIBRARY)
    message(FATAL_ERROR 
        "libdxfrw library not found. Please install libdxfrw or set:\n"
        "  -DDXFRW_LIBRARY=/path/to/libdxfrw.so\n"
        "  -DDXFRW_INCLUDE_DIR=/path/to/dxfrw/headers")
endif()

if(NOT DXFRW_INCLUDE_DIR)
    message(FATAL_ERROR 
        "libdxfrw headers not found. Please install libdxfrw or set:\n"
        "  -DDXFRW_INCLUDE_DIR=/path/to/dxfrw/headers")
endif()

# Status messages
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV includes: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "DXFRW library: ${DXFRW_LIBRARY}")
message(STATUS "DXFRW includes: ${DXFRW_INCLUDE_DIR}")

# Core library source files (shared between executable and library)
set(CORE_SOURCES
    src/ImageProcessor.cpp
    src/DXFWriter.cpp
    src/OutlineToolAPI.cpp
)

# Executable source files
set(EXECUTABLE_SOURCES
    src/main.cpp
)

# Build options
option(BUILD_SHARED_LIB "Build shared library (.dylib/.so)" ON)
option(BUILD_EXECUTABLE "Build command-line executable" ON)

# Create shared library
if(BUILD_SHARED_LIB)
    add_library(${PROJECT_NAME}Lib SHARED ${CORE_SOURCES})
    
    # Set library properties
    set_target_properties(${PROJECT_NAME}Lib PROPERTIES
        OUTPUT_NAME "outlinetool"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "include/OutlineToolAPI.h"
        MACOSX_RPATH TRUE
        INSTALL_RPATH "@loader_path"
    )
    
    # Include directories for library
    target_include_directories(${PROJECT_NAME}Lib
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${OpenCV_INCLUDE_DIRS}
            ${DXFRW_INCLUDE_DIR}
    )
    
    # Link libraries for shared lib
    target_link_libraries(${PROJECT_NAME}Lib
        PRIVATE
            ${OpenCV_LIBS}
            ${DXFRW_LIBRARY}
    )
    
    # Export symbols for C API
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME}Lib PRIVATE OUTLINE_TOOL_EXPORTS)
    endif()
    
    # Installation for library
    install(TARGETS ${PROJECT_NAME}Lib
        EXPORT ${PROJECT_NAME}Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        PUBLIC_HEADER DESTINATION include
    )
    
    # Install export configuration
    install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Config.cmake
        DESTINATION lib/cmake/${PROJECT_NAME}
    )
endif()

# Create executable
if(BUILD_EXECUTABLE)
    add_executable(${PROJECT_NAME} ${EXECUTABLE_SOURCES} ${CORE_SOURCES})
    
    # Include directories for executable
    target_include_directories(${PROJECT_NAME}
        PRIVATE
            include
            ${OpenCV_INCLUDE_DIRS}
            ${DXFRW_INCLUDE_DIR}
    )
    
    # Link libraries for executable
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            ${OpenCV_LIBS}
            ${DXFRW_LIBRARY}
    )
    
    # Installation for executable
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )
endif()

# Print build summary
message(STATUS "")
message(STATUS "Build Summary:")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")