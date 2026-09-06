# libplacebo-jni

This repository provides an example of how to integrate the [libplacebo](https://github.com/haasn/libplacebo) library with Java using JNI (Java Native Interface). It is primarily intended to be used in conjunction with FFmpeg for rendering video streams via Vulkan. This project can be utilized alongside the FFmpeg JavaCV build.

## Disclaimer

This project serves as a demonstration and example of how to achieve JNI integration with libplacebo. It is tailored to fit the needs of a specific private project and may require modifications to suit your requirements. While it aims to be generally applicable, it's recommended to adapt the code to your specific use case.

## Features

- Demonstrates integration of libplacebo with Java using JNI.
- Meant to be used in combination with FFmpeg for rendering video streams via Vulkan.
- Compatible with FFmpeg's LGPL license when used via dynamic linking.
- Supports basic rendering alongside the Vulkan stream using the [Nuklear GUI library](https://github.com/Immediate-Mode-UI/Nuklear).
- Native libraries are built for Linux x64, Windows x64/x86 and macOS x86_64/arm64.
- GitHub Actions scripts are provided for building the native libraries.
- A demo application showcasing rendering of H.264 or H.265 streams is planned and will be added soon.

## Usage

1. Clone the repository:

   ```bash
   git clone --recurse-submodules https://github.com/streamingdv/libplacebo-jni.git
   ```

2. Build the native libraries using the provided GitHub Actions scripts.

3. Assemble and publish the Java library — see
   [Building and publishing the desktop jar](#building-and-publishing-the-desktop-jar).

4. Use the provided JNI functions to interact with libplacebo from your Java code.

## Building and publishing the desktop jar

The desktop jar is a *fat* jar: it embeds the natives for every supported
platform under `native-binaries/` and extracts the matching one at runtime via
`PlaceboManager.setupWithTemporaryFolder()`.

### 1. Drop the CI artifacts into the resources directory

The natives are git-tracked in
[`libplacebo-jni-java/src/main/resources/native-binaries/`](./libplacebo-jni-java/src/main/resources/native-binaries).
Download the artifacts produced by the per-platform workflows and overwrite the
files there. The five names are fixed, because
`PlaceboManager.getNativeLibraryName()` derives them from `os.name` and
`os.arch`:

| Workflow artifact | File in `native-binaries/` | Resolved on |
| --- | --- | --- |
| `windows-libjerasure-jni-artifact-64bit` | `libplacebo-jni-native-64.dll` | Windows x64 |
| `windows-libjerasure-jni-artifact-32bit` | `libplacebo-jni-native-32.dll` | Windows x86 |
| `linux-libplacebo-jni-artifact-64bit` | `libplacebo-jni-native-64.so` | Linux x64 |
| `macos-libplacebo-jni-artifact-x86_64` | `libplacebo-jni-native-64.dylib` | macOS Intel |
| `macos-libplacebo-jni-artifact-arm64` | `libplacebo-jni-native-arm64.dylib` | macOS Apple Silicon |

The loader only prefixes `arm` when `os.arch` starts with `aarch`, so Intel
macOS looks for the bare `-64.dylib` name rather than `-x86_64.dylib`. All
workflows already emit these exact filenames, so the artifacts can be unzipped
straight over the existing ones.

### 2. Build the jar and publish it to the local Maven repository

```bash
./gradlew :libplacebo-jni-java:clean :libplacebo-jni-java:publishToMavenLocal
```

On Windows use `gradlew.bat` instead of `./gradlew`. This requires a JDK 25
toolchain and produces
`~/.m2/repository/com/grill/placebo/libplacebo-jni-java/1.0/libplacebo-jni-java-1.0.jar`.

Do **not** pass `-PnativeBinaryExternalDir=...` for this step. That property is
meant for single-platform CI builds that produce a native outside the source
tree; combined with the checked-in resources it makes `processResources` fail
with a duplicate-entry error under Gradle 9.

### 3. Consume it

Add the local Maven repository and the dependency to the consuming project
(this is how PXPlay pulls it in):

```groovy
repositories {
    mavenLocal()
}

dependencies {
    implementation 'com.grill.placebo:libplacebo-jni-java:1.0'
}
```

Gradle reads artifacts from `mavenLocal()` in place rather than copying them
into its module cache, so re-running `publishToMavenLocal` is picked up without
`--refresh-dependencies`.

To swap the jar in an already installed PXPlay (Windows, Linux, macOS) instead
of rebuilding the app, see [Replacing the library in PXPlay (LGPL)](./LGPL.md).

## License

This project is licensed under the GNU Lesser General Public License (LGPL), which allows for the use of the code in proprietary software as long as any modifications to the LGPL-licensed code are released under LGPL. It is compatible with other licenses when used via dynamic linking.

PXPlay ships the desktop jar on the class path and loads the native from it at
runtime, so a rebuilt `libplacebo-jni-java-1.0.jar` can replace the one in the
installation folder. Paths and the macOS re-sign step are in
[LGPL.md](./LGPL.md).

## Contributions

Feedback is welcome. If you have any suggestions for improvements or ideas for new features, feel free to let me know. I value your input and will consider all feedback to enhance the project. Additionally, if you find this project helpful, consider forking it to customize it for your own needs.

## Credits

This project was created by streamingdv as a solution for integrating libplacebo into a Java project. It is heavily tailored to meet the needs of a specific private project but can serve as a useful starting point for similar integrations.

Special thanks to the original author of libplacebo, [Haasn](https://github.com/haasn), for developing such a powerful library that enables advanced video rendering capabilities.

Credit also goes to the author of the [Nuklear GUI library](https://github.com/Immediate-Mode-UI/Nuklear) for providing a flexible and lightweight GUI solution that complements the video rendering capabilities of libplacebo.
