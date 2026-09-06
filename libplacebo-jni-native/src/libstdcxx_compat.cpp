// The prebuilt shaderc/glslang/SPIRV-Tools archives under libs/shaderc were
// compiled against libstdc++ 11 or newer, which is where
// std::__throw_bad_array_new_length() was introduced. Ubuntu 20.04 is the
// oldest distribution this library supports and ships GCC 9, so the symbol is
// absent there and the .so fails to load with an undefined symbol error.
//
// Defining it here satisfies the reference within this library at link time.
// The behaviour is the same as the libstdc++ implementation.
//
// Linux only: macOS builds against libc++, which has no such symbol, and the
// Windows build links -static-libstdc++ against a modern libstdc++ that
// already provides it, where a second definition would be a link error.

#include <new>

extern "C" {

[[noreturn]] void _ZSt28__throw_bad_array_new_lengthv()
{
    throw std::bad_array_new_length();
}

}
