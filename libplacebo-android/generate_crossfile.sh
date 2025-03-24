#!/bin/bash

# Define target prefix
prefix_dir=$(pwd)/android_build

# Ensure the prefix_dir exists
mkdir -p "$prefix_dir"

# Resolve ANDROID_NDK_HOME at runtime
ndk_path="$ANDROID_NDK_HOME"

# Create crossfile dynamically
generate_crossfile() {
    local cpu_family=$1
    local ndk_triple=$2
    local cpu_name=$3
    local arch_name=$4
    local api_level=21

    local crossfile_path="$prefix_dir/crossfile_${arch_name}.txt"

    cat >"$crossfile_path" <<CROSSFILE
[built-in options]
buildtype = 'release'
default_library = 'static'
wrap_mode = 'nodownload'
prefix = '/usr/local'

[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$ndk_path/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar'
nm = '$ndk_path/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm'
strip = '$ndk_path/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip'
pkgconfig = 'pkg-config'
pkg-config = 'pkg-config'

[host_machine]
system = 'android'
cpu_family = '$cpu_family'
cpu = '$cpu_name'
endian = 'little'

[cflags]
c_args = ['-nostdinc++', '-isystem', '$ndk_path/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1']

[cppflags]
cpp_args = ['-nostdinc++', '-isystem', '$ndk_path/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1', '-std=c++20']
CROSSFILE

    echo "Generated crossfile: $crossfile_path"
}

# Set environment variables for compilers
dynamic_env_setup() {
    local arch=$1
    local ndk_triple=$2
    local api_level=21

    export PATH=$ndk_path/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH
    export CC=$ndk_path/toolchains/llvm/prebuilt/linux-x86_64/bin/$ndk_triple$api_level-clang
    export CXX=$ndk_path/toolchains/llvm/prebuilt/linux-x86_64/bin/$ndk_triple$api_level-clang++
}

# Generate crossfiles for all architectures
dynamic_env_setup "arm64" "aarch64-linux-android"
generate_crossfile "aarch64" "aarch64-linux-android" "aarch64" "arm64-v8a"

dynamic_env_setup "armeabi-v7a" "armv7a-linux-androideabi"
generate_crossfile "arm" "armv7a-linux-androideabi" "arm" "armeabi-v7a"

dynamic_env_setup "x86" "i686-linux-android"
generate_crossfile "x86" "i686-linux-android" "i686" "x86"

dynamic_env_setup "x86_64" "x86_64-linux-android"
generate_crossfile "x86_64" "x86_64-linux-android" "x86_64" "x86_64"

# List generated crossfiles for verification
echo "All crossfiles generated successfully in $prefix_dir"
ls -l "$prefix_dir"