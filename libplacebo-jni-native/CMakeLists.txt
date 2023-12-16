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

# Include directories
include_directories(${GENERATED_JNI_HEADERS_DIR})
include_directories(3rdparty/libplacebo/install/include/libplacebo)

# Project sources
set(SOURCES src/placebo_jni.c)

# Add shared library target
add_library(libplacebo-jni-native SHARED ${SOURCES})
find_library(LIBPLACEBO
    NAMES placebo
    PATHS ${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/lib
    PATH_SUFFIXES lib)
if(NOT LIBPLACEBO)
    message(FATAL_ERROR "libplacebo not found in ${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/lib. Abort build")
endif()
target_link_libraries(libplacebo-jni-native PRIVATE ${LIBPLACEBO} ${JNI_LIBRARIES})
# D:/a/libplacebo-jni/libplacebo-jni/libplacebo-jni-native/3rdparty/libplacebo/install/lib
if(MSVC)
    set_property(
            TARGET opus
            PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
    )

    set_property(
            TARGET opus-jni-native
            PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
    )
endif()
