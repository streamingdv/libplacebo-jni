# Policy
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0048 NEW)

# Project setup
cmake_minimum_required(VERSION 3.10) # Set the minimum required version of CMake
project(libplacebo-jni-native)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

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
if(GENERATED_JNI_HEADERS_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DGENERATED_JNI_HEADERS_DIR=/path/to/generated/headers or simply use the gradle build task")
endif()

# Collect all files in the directory
file(GLOB MY_DIRECTORY_CONTENTS "${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/lib/*")
if(NOT MY_DIRECTORY_CONTENTS)
    message(STATUS "The directory ${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/lib is empty.")
else()
    # Print each file in the directory
    foreach(FILE IN LISTS MY_DIRECTORY_CONTENTS)
        message(STATUS "File in directory: ${FILE}")
    endforeach()
endif()

include_directories(${GENERATED_JNI_HEADERS_DIR})
include_directories(3rdparty/libplacebo/install/include)

# Project sources
set(SOURCES src/placebo_jni.cpp)

# Add shared library target
add_library(libplacebo-jni-native SHARED ${SOURCES})
target_compile_definitions(libplacebo-jni-native PRIVATE PL_STATIC)
target_link_libraries(libplacebo-jni-native PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/lib/libplacebo.a
    ${JNI_LIBRARIES})

if(MSVC)
    set_property(
            TARGET libplacebo-jni-native
            PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
    )
endif()
