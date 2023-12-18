# Policy
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0048 NEW)

# Project setup
cmake_minimum_required(VERSION 3.10) # Set the minimum required version of CMake
project(libplacebo-jni-native)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Set the variable with a default value (windows default)
set(LIBPLACEBO_LIB_PATH "${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/lib/libplacebo.a" CACHE STRING "Path to libplacebo library")
option(IS_RUNNING_ON_WINDOWS "Variable indicating if this CMakeLists is executed on a Windows OS" ON)

# Check for host system
if(UNIX OR MINGW)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fstack-protector")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fstack-protector")
endif()

# Configure JNI
find_package(JNI REQUIRED)
if(NOT JNI_FOUND)
    message(FATAL_ERROR "JNI not found")
endif()

# Include JNI headers
include_directories(${JNI_INCLUDE_DIRS})

# Check for GENERATED_JNI_HEADERS_DIR
set(GENERATED_JNI_HEADERS_DIR "" CACHE STRING "Path to generated JNI headers")

# Collect all files in the directory
file(GLOB MY_DIRECTORY_CONTENTS "${GENERATED_JNI_HEADERS_DIR}")
if(NOT MY_DIRECTORY_CONTENTS)
    message(STATUS "The directory ${GENERATED_JNI_HEADERS_DIR} is empty.")
else()
    # Print each file in the directory
    foreach(FILE IN LISTS MY_DIRECTORY_CONTENTS)
        message(STATUS "File in directory: ${FILE}")
    endforeach()
endif()

if(GENERATED_JNI_HEADERS_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DGENERATED_JNI_HEADERS_DIR=/path/to/generated/headers or simply use the gradle build task")
endif()

include_directories(${GENERATED_JNI_HEADERS_DIR})
include_directories(3rdparty/libplacebo/install/include)

# Project sources
set(SOURCES src/placebo_jni.cpp)

# Add shared library target
add_library(libplacebo-jni-native SHARED ${SOURCES})
target_compile_definitions(libplacebo-jni-native PRIVATE PL_STATIC)

if(IS_RUNNING_ON_WINDOWS)
    # Find packages using PkgConfig with static linking preference
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SHADERC_COMBINED_STATIC REQUIRED shaderc_combined)
    pkg_check_modules(VULKAN_STATIC REQUIRED vulkan)

    # Include 3rd party directories and library paths
    include_directories(${SHADERC_COMBINED_STATIC_INCLUDE_DIRS} ${VULKAN_STATIC_INCLUDE_DIRS})
    link_directories(${SHADERC_COMBINED_STATIC_LIBRARY_DIRS} ${VULKAN_STATIC_LIBRARY_DIRS})

    # Link libraries - Static versions
    target_link_libraries(libplacebo-jni-native PRIVATE
        ${LIBPLACEBO_LIB_PATH}
        ${JNI_LIBRARIES}
        ${SHADERC_COMBINED_STATIC_LIBRARIES}
        ${VULKAN_STATIC_LIBRARIES}
        glslang
        SPIRV
        SPIRV-Tools-opt
        SPIRV-Tools
        SPIRV-Tools-link
        spirv-cross-glsl
        spirv-cross-c
        shlwapi)
else()
    target_link_libraries(libplacebo-jni-native PRIVATE
        ${LIBPLACEBO_LIB_PATH}
        ${JNI_LIBRARIES})
endif()

# Set the PREFIX property to empty
set_target_properties(libplacebo-jni-native PROPERTIES PREFIX "") # avoid the "liblib" naming issu

if(MSVC)
    set_property(
            TARGET libplacebo-jni-native
            PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
    )
endif()
