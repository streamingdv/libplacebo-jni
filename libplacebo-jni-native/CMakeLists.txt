# Policy
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0048 NEW)

# Project setup
cmake_minimum_required(VERSION 3.10) # Set the minimum required version of CMake
project(libplacebo-jni-native)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(NOT APPLE)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
endif()

# Set macOS-specific flags
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") # Ensure compatibility with macOS 11
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fstack-protector -mmacosx-version-min=11.0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fstack-protector -mmacosx-version-min=11.0")
endif()

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
if(GENERATED_JNI_HEADERS_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DGENERATED_JNI_HEADERS_DIR=/path/to/generated/headers")
endif()

# Include JNI headers
include_directories(${GENERATED_JNI_HEADERS_DIR})
# Include libplacebo headers
set(LIBPLACEBO_HEADERS_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/include" CACHE STRING "Path to libplacebo headers")
include_directories(${LIBPLACEBO_HEADERS_DIR})

if(APPLE)
   # Include MoltenVK headers (for macOS)
   set(MOLTENVK_INCLUDE_DIR "${MOLTENVK_INCLUDE}" CACHE STRING "MoltenVK include directory")
   include_directories(${MOLTENVK_INCLUDE_DIR})
else()
   # Check for VULKAN_LIB_DIR
   set(VULKAN_LIB_DIR "" CACHE STRING "Path to vulkan headers")
   if(VULKAN_LIB_DIR STREQUAL "")
       message(FATAL_ERROR "Please run CMake with -DVULKAN_LIB_DIR=/path/to/vulkan/include")
   endif()
   include_directories(${VULKAN_LIB_DIR}/include)
endif()

# Check for Shaderc
set(SHADERC_HEADERS_DIR "" CACHE STRING "Path to shaderc headers")
if(SHADERC_HEADERS_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DSHADERC_HEADERS_DIR=/path/to/shaderc/include")
endif()
include_directories(${SHADERC_HEADERS_DIR})

set(SHADERC_LIB_DIR "" CACHE STRING "Path to shaderc lib")
if(SHADERC_LIB_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DSHADERC_LIB_DIR=/path/to/shaderc/lib")
endif()

# Check for FFMPEG
set(FFMPEG_DIR "" CACHE STRING "Path to fffmpeg dir")
if(FFMPEG_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DFFMPEG_DIR=/path/to/ffmpeg")
endif()
include_directories(${FFMPEG_DIR})

set(LCMS2_DIR "" CACHE STRING "Path to lcms2 dir")
if(LCMS2_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DLCMS2_DIR=/path/to/lcms2")
endif()
include_directories(${LCMS2_DIR}/include)

# Check for NUKLEAR_DIR
set(NUKLEAR_DIR "" CACHE STRING "Path to nuklear headers")
if(NUKLEAR_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DNUKLEAR_DIR=/path/to/nuklear/include")
endif()
include_directories(${NUKLEAR_DIR})

# Check for Vulkan-Memory-Allocator header
set(VKMEMALLOC_HEADERS "" CACHE STRING "Path to shaderc headers")
if(VKMEMALLOC_HEADERS STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DVKMEMALLOC_HEADERS=/path/to/vulkanMemAllocator/include")
endif()
include_directories(${VKMEMALLOC_HEADERS})

# Project sources
include_directories(src)
set(SOURCES
    src/libav_impl.c
    src/placebo_jni.cpp
)

# Add shared library target
add_library(libplacebo-jni-native SHARED ${SOURCES})
target_compile_definitions(libplacebo-jni-native PRIVATE PL_STATIC)

# Set link options for Windows
if(IS_RUNNING_ON_WINDOWS)
    target_link_options(libplacebo-jni-native PRIVATE
        -static-libgcc
        -static-libstdc++
        -static
    )
endif()

if(IS_RUNNING_ON_WINDOWS)
    # Link libraries - Static versions
    target_link_libraries(libplacebo-jni-native PRIVATE
        ${LIBPLACEBO_LIB_PATH}
        ${JNI_LIBRARIES}
        "${LCMS2_DIR}/lib/liblcms2.a"
        "${SHADERC_LIB_DIR}/libshaderc.a"
        "${SHADERC_LIB_DIR}/libshaderc_util.a"
        "${FFMPEG_DIR}/libavcodec/libavcodec.a"
        "${FFMPEG_DIR}/libavformat/libavformat.a"
        "${FFMPEG_DIR}/libavutil/libavutil.a"
        "${VULKAN_LIB_DIR}/lib/libloader-opt.a"
        "${VULKAN_LIB_DIR}/lib/libvulkan-1.dll.a"
        "${SHADERC_LIB_DIR}/libMachineIndependent.a"
        "${SHADERC_LIB_DIR}/libOSDependent.a"
        "${SHADERC_LIB_DIR}/libglslang.a"
        "${SHADERC_LIB_DIR}/libSPIRV.a"
        "${SHADERC_LIB_DIR}/libSPVRemapper.a"
        "${SHADERC_LIB_DIR}/libglslang-default-resource-limits.a"
        "${SHADERC_LIB_DIR}/libGenericCodeGen.a"
        "${SHADERC_LIB_DIR}/libSPIRV-Tools-link.a"
        "${SHADERC_LIB_DIR}/libSPIRV-Tools-lint.a"
        "${SHADERC_LIB_DIR}/libSPIRV-Tools-reduce.a"
        "${SHADERC_LIB_DIR}/libSPIRV-Tools-opt.a"
        "${SHADERC_LIB_DIR}/libSPIRV-Tools.a"
        "${SHADERC_LIB_DIR}/libSPIRV-Tools-diff.a"
        shlwapi
        bcrypt
    )
else()
    # Find packages using PkgConfig with static linking preference
    find_package(PkgConfig REQUIRED)

    #pkg_check_modules(XCB REQUIRED xcb)
    #include_directories(${XCB_INCLUDE_DIRS})
    #link_directories(${XCB_LIBRARY_DIRS})
    #add_definitions(${XCB_CFLAGS_OTHER})

    if(APPLE)
        set(MOLTENVK_LIB "${MOLTENVK_LIB_PATH}" CACHE STRING "MoltenVK static library path")
        target_link_libraries(libplacebo-jni-native PRIVATE
            ${LIBPLACEBO_LIB_PATH}
            ${JNI_LIBRARIES}
            ${MOLTENVK_LIB}
            "${LCMS2_DIR}/lib/liblcms2.a"
            "${SHADERC_LIB_DIR}/libshaderc.a"
            "${SHADERC_LIB_DIR}/libshaderc_util.a"
            "${FFMPEG_DIR}/libavcodec/libavcodec.a"
            "${FFMPEG_DIR}/libavformat/libavformat.a"
            "${FFMPEG_DIR}/libavutil/libavutil.a"
            "${SHADERC_LIB_DIR}/libMachineIndependent.a"
            "${SHADERC_LIB_DIR}/libOSDependent.a"
            "${SHADERC_LIB_DIR}/libglslang.a"
            "${SHADERC_LIB_DIR}/libSPIRV.a"
            "${SHADERC_LIB_DIR}/libSPVRemapper.a"
            "${SHADERC_LIB_DIR}/libglslang-default-resource-limits.a"
            "${SHADERC_LIB_DIR}/libGenericCodeGen.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-link.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-lint.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-reduce.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-opt.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-diff.a"
            "-framework CoreFoundation"
            "-framework CoreMedia"
            "-framework CoreVideo"
            "-framework VideoToolbox"
            "-framework CoreGraphics"
            "-framework Metal"
            "-framework IOSurface"
            "-framework Foundation"
            "-framework AppKit"
            "-framework IOKit"
            "-framework QuartzCore"
        )
    else()
        pkg_check_modules(VULKAN REQUIRED vulkan)
        link_directories(${VULKAN_LIBRARY_DIRS})

        target_link_libraries(libplacebo-jni-native PRIVATE
            ${LIBPLACEBO_LIB_PATH}
            ${VULKAN_LIBRARIES}
            ${XCB_LIBRARIES}
            "${LCMS2_DIR}/lib/liblcms2.a"
            "${SHADERC_LIB_DIR}/libshaderc.a"
            "${SHADERC_LIB_DIR}/libshaderc_util.a"
            "${FFMPEG_DIR}/libavcodec/libavcodec.a"
            "${FFMPEG_DIR}/libavformat/libavformat.a"
            "${FFMPEG_DIR}/libavutil/libavutil.a"
            "${SHADERC_LIB_DIR}/libMachineIndependent.a"
            "${SHADERC_LIB_DIR}/libOSDependent.a"
            "${SHADERC_LIB_DIR}/libglslang.a"
            "${SHADERC_LIB_DIR}/libSPIRV.a"
            "${SHADERC_LIB_DIR}/libSPVRemapper.a"
            "${SHADERC_LIB_DIR}/libglslang-default-resource-limits.a"
            "${SHADERC_LIB_DIR}/libGenericCodeGen.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-link.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-lint.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-reduce.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-opt.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools.a"
            "${SHADERC_LIB_DIR}/libSPIRV-Tools-diff.a"
        )
    endif()
endif()

# Set the PREFIX property to empty
set_target_properties(libplacebo-jni-native PROPERTIES PREFIX "") # avoid the "liblib" naming issu

if(MSVC)
    set_property(
            TARGET libplacebo-jni-native
            PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded"
    )
endif()
