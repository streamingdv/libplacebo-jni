# libplacebo-jni

This repository provides an example of how to integrate the [libplacebo](https://github.com/haasn/libplacebo) library with Java using JNI (Java Native Interface). It is primarily intended to be used in conjunction with FFmpeg for rendering video streams via Vulkan. This project can be utilized alongside the FFmpeg JavaCV build.

## Disclaimer

This project serves as a demonstration and example of how to achieve JNI integration with libplacebo. It is tailored to fit the needs of a specific private project and may require modifications to suit your requirements. While it aims to be generally applicable, it's recommended to adapt the code to your specific use case.

## Features

- Demonstrates integration of libplacebo with Java using JNI.
- Meant to be used in combination with FFmpeg for rendering video streams via Vulkan.
- Compatible with FFmpeg's LGPL license when used via dynamic linking.
- Supports basic rendering alongside the Vulkan stream using the [Nuklear GUI library](https://github.com/Immediate-Mode-UI/Nuklear).
- Native libraries are built for Linux x64 and Windows x64/x86 bit platforms.
- GitHub Actions scripts are provided for building the native libraries.
- A demo application showcasing rendering of H.264 or H.265 streams is planned and will be added soon.

## Usage

1. Clone the repository:

   ```bash
   git clone https://github.com/your-username/libplacebo-jni.git

2. Build the native libraries using the provided GitHub Actions scripts.

3. Integrate the generated native libraries with your Java project.

4. Use the provided JNI functions to interact with libplacebo from your Java code.

## License

This project is licensed under the GNU Lesser General Public License (LGPL), which allows for the use of the code in proprietary software as long as any modifications to the LGPL-licensed code are released under LGPL. It is compatible with other licenses when used via dynamic linking.

## Contributions

Feedback is welcome. If you have any suggestions for improvements or ideas for new features, feel free to let me know. I value your input and will consider all feedback to enhance the project. Additionally, if you find this project helpful, consider forking it to customize it for your own needs.

## Credits

This project was created by streamingdv as a solution for integrating libplacebo into a Java project. It is heavily tailored to meet the needs of a specific private project but can serve as a useful starting point for similar integrations.

Special thanks to the original author of libplacebo, [Haasn](https://github.com/haasn), for developing such a powerful library that enables advanced video rendering capabilities.

Credit also goes to the author of the [Nuklear GUI library](https://github.com/Immediate-Mode-UI/Nuklear) for providing a flexible and lightweight GUI solution that complements the video rendering capabilities of libplacebo.
