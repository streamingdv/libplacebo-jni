cmake_minimum_required(VERSION 3.10)

# Project setup
project(libplacebo-jni-native)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Android-specific flags
if(ANDROID)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# Include JNI headers
find_package(JNI REQUIRED)
if(NOT JNI_FOUND)
    message(FATAL_ERROR "JNI not found")
endif()

# Include JNI headers
include_directories(${JNI_INCLUDE_DIRS})

# Include libplacebo headers
set(LIBPLACEBO_HEADERS_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/include" CACHE STRING "Path to libplacebo headers")
include_directories(${LIBPLACEBO_HEADERS_DIR})

# Check for FFMPEG
set(FFMPEG_DIR "" CACHE STRING "Path to fffmpeg dir")
if(FFMPEG_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DFFMPEG_DIR=/path/to/ffmpeg")
endif()
include_directories(${FFMPEG_DIR}/include)

# Project sources
set(SOURCES
    native/libav_impl.c
    native/placebo_jni.cpp
    native/ffmpeg_jni.cpp
)

# Create shared library for JNI
add_library(libplacebo-jni-native SHARED ${SOURCES})

# Rename output to libplacebo.so
set_target_properties(libplacebo-jni-native PROPERTIES OUTPUT_NAME "placebo")

# Enable EGL/GLES extension prototypes
target_compile_definitions(libplacebo-jni-native PRIVATE EGL_EGLEXT_PROTOTYPES GL_GLEXT_PROTOTYPES)

# Link dependencies
target_link_libraries(libplacebo-jni-native
    ${LIBPLACEBO_HEADERS_DIR}/../lib/libplacebo.a
    ${FFMPEG_DIR}/lib/libavcodec.a
    ${FFMPEG_DIR}/lib/libavformat.a
    ${FFMPEG_DIR}/lib/libavutil.a
    EGL
    GLESv3
    log    # Android logging
    android # Android native APIs
    mediandk # Needed for av_mediacodec_* symbols
)

# Set the PREFIX property to empty to avoid "liblib" prefix issues
set_target_properties(libplacebo-jni-native PROPERTIES PREFIX "")

# Generate Android-specific libraries
if(ANDROID)
    set_target_properties(libplacebo-jni-native PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/libs/${ANDROID_ABI}"
    )
endif()