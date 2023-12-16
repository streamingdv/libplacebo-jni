# Project setup
project('libplacebo-jni-native', ['c', 'cpp', 'java'],
        default_options : ['buildtype=release', 'b_ndebug=if-release', 'c_std=c11', 'cpp_std=c++20'],
        version : '1.0.0',
        meson_version : '>=0.50.0')

# Set global compile options
if host_machine.system() == 'darwin'
    add_global_arguments('-fPIC -fstack-protector', language : 'c')
endif

# Requires JNI
jni_dep = dependency('jni', required : true)

# Configure project
if get_option('GENERATED_JNI_HEADERS_DIR') == ''
    error('Please run meson with -DGENERATED_JNI_HEADERS_DIR=/path/to/generated/headers or simply use the gradle build task')
endif

# Project sources
sources = ['src/placebo_jni.c']

# Path to the custom library directory
lib_dir = meson.current_source_dir() / '3rdparty/libplacebo/install/lib'
libplacebo = find_library('placebo', dirs: [lib_dir], static: true)
libplacebo_dep = declare_dependency(link_with: libplacebo,
                                    include_directories: include_directories('3rdparty/libplacebo/install/lib'))

# Add target
shared_library('libplacebo-jni-native',
               sources,
               dependencies : [jni_dep, libplacebo_dep],
               include_directories: [
                   include_directories(get_option('GENERATED_JNI_HEADERS_DIR')),
                   include_directories('3rdparty/libplacebo/install/include/libplacebo')
               ],
               install : true)

# MSVC specific options
if meson.get_compiler('c').get_id() == 'msvc'
    add_project_arguments('/MTd', language : 'c')
endif
