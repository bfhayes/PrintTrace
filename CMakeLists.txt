cmake_minimum_required(VERSION 3.16)
project(PrintTrace VERSION 1.0.0 LANGUAGES CXX)

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

# Build libdxfrw as static library from submodule
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libdxfrw/CMakeLists.txt")
    message(STATUS "Building libdxfrw from submodule")
    
    # Configure libdxfrw to build as static library
    set(BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    
    # Disable building dwg2dxf tool from libdxfrw
    set(BUILD_DWG2DXF OFF CACHE BOOL "" FORCE)
    
    # Add libdxfrw subdirectory
    add_subdirectory(third_party/libdxfrw EXCLUDE_FROM_ALL)
    
    # Restore BUILD_SHARED_LIBS setting
    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVED} CACHE BOOL "" FORCE)
    
    # Set variables for linking
    set(DXFRW_LIBRARY dxfrw)
    set(DXFRW_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libdxfrw/src")
    
else()
    # Fallback to system-installed libdxfrw if submodule not available
    message(STATUS "Submodule not found, looking for system libdxfrw")
    
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
            "libdxfrw not found. Please:\n"
            "  1. Initialize submodule: git submodule update --init --recursive\n"
            "  OR\n"
            "  2. Install libdxfrw system-wide and set:\n"
            "     -DDXFRW_LIBRARY=/path/to/libdxfrw.so\n"
            "     -DDXFRW_INCLUDE_DIR=/path/to/dxfrw/headers")
    endif()
    
    if(NOT DXFRW_INCLUDE_DIR)
        message(FATAL_ERROR 
            "libdxfrw headers not found. Please:\n"
            "  1. Initialize submodule: git submodule update --init --recursive\n"
            "  OR\n"
            "  2. Set: -DDXFRW_INCLUDE_DIR=/path/to/dxfrw/headers")
    endif()
endif()

# Status messages
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV includes: ${OpenCV_INCLUDE_DIRS}")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libdxfrw/CMakeLists.txt")
    message(STATUS "DXFRW: Building from submodule (static)")
else()
    message(STATUS "DXFRW library: ${DXFRW_LIBRARY}")
endif()
message(STATUS "DXFRW includes: ${DXFRW_INCLUDE_DIR}")

# Core library source files (shared between executable and library)
set(CORE_SOURCES
    src/ImageProcessor.cpp
    src/DXFWriter.cpp
    src/PrintTraceAPI.cpp
)

# Executable source files (old monolithic approach)
set(EXECUTABLE_SOURCES
    src/main.cpp
)

# CLI tool source files (uses shared library)
set(CLI_SOURCES
    src/printtrace_cli.cpp
)

# Build options
option(BUILD_SHARED_LIB "Build shared library (.dylib/.so)" ON)
option(BUILD_EXECUTABLE "Build command-line executable" ON)
option(BUILD_CLI_TOOL "Build CLI tool that uses shared library" ON)

# Create shared library
if(BUILD_SHARED_LIB)
    add_library(${PROJECT_NAME}Lib SHARED ${CORE_SOURCES})
    
    # Set library properties
    set_target_properties(${PROJECT_NAME}Lib PROPERTIES
        OUTPUT_NAME "printtrace"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "include/PrintTraceAPI.h"
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
        target_compile_definitions(${PROJECT_NAME}Lib PRIVATE PRINT_TRACE_EXPORTS)
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
    
    # Create and install pkg-config file
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/printtrace.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/printtrace.pc
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/printtrace.pc
        DESTINATION lib/pkgconfig
    )
endif()

# Create executable (monolithic - includes all source directly)
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

# Create CLI tool (uses shared library)
if(BUILD_CLI_TOOL AND BUILD_SHARED_LIB)
    add_executable(${PROJECT_NAME}CLI ${CLI_SOURCES})
    
    # Set executable name
    set_target_properties(${PROJECT_NAME}CLI PROPERTIES
        OUTPUT_NAME "printtrace"
    )
    
    # Include directories for CLI tool
    target_include_directories(${PROJECT_NAME}CLI
        PRIVATE
            include
    )
    
    # Link to shared library
    target_link_libraries(${PROJECT_NAME}CLI
        PRIVATE
            ${PROJECT_NAME}Lib
    )
    
    # Set rpath for finding the shared library
    if(APPLE)
        set_target_properties(${PROJECT_NAME}CLI PROPERTIES
            INSTALL_RPATH "@loader_path/../lib"
            BUILD_RPATH "@loader_path"
            BUILD_WITH_INSTALL_RPATH FALSE
        )
    else()
        set_target_properties(${PROJECT_NAME}CLI PROPERTIES
            INSTALL_RPATH "$ORIGIN/../lib"
            BUILD_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH FALSE
        )
    endif()
    
    # Installation for CLI tool
    install(TARGETS ${PROJECT_NAME}CLI
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
if(BUILD_SHARED_LIB)
    message(STATUS "  Building: Shared library (libprinttrace.dylib)")
endif()
if(BUILD_CLI_TOOL AND BUILD_SHARED_LIB)
    message(STATUS "  Building: CLI tool (printtrace) using shared library")
endif()
if(BUILD_EXECUTABLE)
    message(STATUS "  Building: Monolithic executable (PrintTrace)")
endif()
message(STATUS "")