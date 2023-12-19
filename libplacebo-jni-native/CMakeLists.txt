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
if(GENERATED_JNI_HEADERS_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DGENERATED_JNI_HEADERS_DIR=/path/to/generated/headers")
endif()

# Check for GLSLANG_LIBS_DIR
set(GLSLANG_LIBS_DIR "" CACHE STRING "Path to glslang static libraries")
if(GLSLANG_LIBS_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DGLSLANG_LIBS_DIR=/path/to/glslang/static/libs")
endif()

# Check for GLSLANG_HEADERS_DIR
set(GLSLANG_HEADERS_DIR "" CACHE STRING "Path to glslang headers")
if(GLSLANG_HEADERS_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DGLSLANG_HEADERS_DIR=/path/to/glslang/include")
endif()
include_directories(${GLSLANG_HEADERS_DIR})

# Inlcude JNI headers
include_directories(${GENERATED_JNI_HEADERS_DIR})
# Include libplacebo headers
set(LIBPLACEBO_HEADERS_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libplacebo/install/include" CACHE STRING "Path to libplacebo headers")
include_directories(${LIBPLACEBO_HEADERS_DIR})

# Check for VULKAN_LIB_DIR
set(VULKAN_LIB_DIR "" CACHE STRING "Path to vulkan headers")
if(VULKAN_LIB_DIR STREQUAL "")
    message(FATAL_ERROR "Please run CMake with -DVULKAN_LIB_DIR=/path/to/glslang/include")
endif()
include_directories(${VULKAN_LIB_DIR}/include)

# Project sources
set(SOURCES src/placebo_jni.cpp)

# Add shared library target
add_library(libplacebo-jni-native SHARED ${SOURCES})
target_compile_definitions(libplacebo-jni-native PRIVATE PL_STATIC)

# Find packages using PkgConfig with static linking preference
find_package(PkgConfig REQUIRED)
#pkg_check_modules(VULKAN_STATIC REQUIRED vulkan)
#link_directories(${VULKAN_STATIC_LIBRARY_DIRS})


if(IS_RUNNING_ON_WINDOWS)
    pkg_check_modules(SHADERC_COMBINED_STATIC REQUIRED shaderc_combined)
    message(STATUS "SHADERC_COMBINED_STATIC_INCLUDE_DIRS: ${SHADERC_COMBINED_STATIC_INCLUDE_DIRS}")

    foreach(DIR ${SHADERC_COMBINED_STATIC_INCLUDE_DIRS})
        file(GLOB FILES "${DIR}/*")
        message(STATUS "Files in ${DIR}:")
        foreach(FILE ${FILES})
            message(STATUS "  ${FILE}")
        endforeach()
    endforeach()

    include_directories(
        ${SHADERC_COMBINED_STATIC_INCLUDE_DIRS}
    )
    link_directories(
        ${SHADERC_COMBINED_STATIC_LIBRARY_DIRS}
    )

    # Link libraries - Static versions
    target_link_libraries(libplacebo-jni-native PRIVATE
        ${LIBPLACEBO_LIB_PATH}
        ${JNI_LIBRARIES}
        ${SHADERC_COMBINED_STATIC_LIBRARIES}
        #${VULKAN_STATIC_LIBRARIES}
        "${VULKAN_LIB_DIR}/lib/libloader-opt.a"
        "${VULKAN_LIB_DIR}/lib/libvulkan-1.dll.a"
        "${GLSLANG_LIBS_DIR}/libMachineIndependent.a"
        "${GLSLANG_LIBS_DIR}/libOSDependent.a"
        "${GLSLANG_LIBS_DIR}/libglslang.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV.a"
        "${GLSLANG_LIBS_DIR}/libSPVRemapper.a"
        "${GLSLANG_LIBS_DIR}/libglslang-default-resource-limits.a"
        "${GLSLANG_LIBS_DIR}/libGenericCodeGen.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-link.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-lint.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-reduce.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-opt.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-diff.a"
        shlwapi
    )
else()
    target_link_libraries(libplacebo-jni-native PRIVATE
        ${LIBPLACEBO_LIB_PATH}
        ${JNI_LIBRARIES}
        ${VULKAN_STATIC_LIBRARIES}
        "${GLSLANG_LIBS_DIR}/libMachineIndependent.a"
        "${GLSLANG_LIBS_DIR}/libOSDependent.a"
        "${GLSLANG_LIBS_DIR}/libglslang.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV.a"
        "${GLSLANG_LIBS_DIR}/libSPVRemapper.a"
        "${GLSLANG_LIBS_DIR}/libglslang-default-resource-limits.a"
        "${GLSLANG_LIBS_DIR}/libGenericCodeGen.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-link.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-lint.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-reduce.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-opt.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools.a"
        "${GLSLANG_LIBS_DIR}/libSPIRV-Tools-diff.a"
    )
endif()

# Set the PREFIX property to empty
set_target_properties(libplacebo-jni-native PROPERTIES PREFIX "") # avoid the "liblib" naming issu

if(MSVC)
    set_property(
            TARGET libplacebo-jni-native
            PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug"
    )
endif()
