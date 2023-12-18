package com.grill.placebo;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.AccessDeniedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.NoSuchElementException;

public class PlaceboManager {
    public interface LogCallback {
        void onLog(int level, String message);
    }

    public static final class LogLevel {
        public final static int PL_LOG_NONE = 0;
        public final static int PL_LOG_FATAL = 1;   // results in total loss of function of a major component
        public final static int PL_LOG_ERR = 2;     // serious error; may result in degraded function
        public final static int PL_LOG_WARN = 3;    // warning; potentially bad, probably user-relevant
        public final static int PL_LOG_INFO = 4;    // informational message, also potentially harmless errors
        public final static int PL_LOG_DEBUG = 5;   // verbose debug message, informational
        public final static int PL_LOG_TRACE = 6;   // very noisy trace of activity,, usually benign
        public final static int PL_LOG_ALL = PL_LOG_TRACE;
    }

    public static class VulkanExtensions {
        public static final String VK_KHR_SURFACE_EXTENSION = "VK_KHR_SURFACE_EXTENSION_NAME";
        public static final String VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION = "VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME";
        public static final String VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION = "VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME";
        public static final String VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION = "VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME";
        public static final String VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION = "VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME";

        public static String[] getExtensions() {
            return new String[] {
                    VK_KHR_SURFACE_EXTENSION,
                    VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION,
                    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION,
                    VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION,
                    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION
            };
        }
    }

    // Class to represent pl_vk_inst_params structure
    public static class PlVkInstParams {
        public boolean debug;
        public boolean debugExtra;
        public int maxApiVersion;
        public String[] extensions;
        public String[] optExtensions;
        public String[] layers;
        public String[] optLayers;
    }

    public static final int API_VERSION = 342;

    /**
     * Creates a logger with the log callback
     * @param apiVersion the API version
     * @param logLevel the log level
     * @param callback the callback
     * @return the log handle, pl_log
     */
    public native long createLog(int apiVersion, int logLevel, LogCallback callback);

    /**
     * Deletes a logger with the log callback
     * @param logHandle the log handle to be deleted, pl_log
     */
    public native void destroyLog(long logHandle);

    /**
     * Get the underlying windowing system. Please note: this is only supported on Unix systems
     * @return the underlying windowing system or "unknown"
     */
    public static native String getWindowingSystem();

    /**
     * Creates a Vulkan instance
     * @param plLog the logHandle
     * @param params the params
     * @return the handle to the vk instance, placebo_vk_inst
     */
    public native long plVkInstCreate(long plLog, PlVkInstParams params);

    /**
     * Deletes a vl inst
     * @param placebo_vk_inst the vk inst to be deleted, placebo_vk_inst
     */
    public native void plVkInstDestroy(long placebo_vk_inst);

    /************************/
    /*** load lib methods ***/
    /************************/

    public static void loadNative(File directory) throws IOException {
        loadNative(directory, true);
    }

    public static void loadNative(File directory, boolean allowArm) throws IOException {
        String nativeLibraryName = getNativeLibraryName(allowArm);
        InputStream source = PlaceboManager.class.getResourceAsStream("/native-binaries/" + nativeLibraryName);
        if (source == null) {
            throw new IOException("Could not find native library " + nativeLibraryName);
        }

        Path destination = directory.toPath().resolve(nativeLibraryName);
        try {
            Files.copy(source, destination, StandardCopyOption.REPLACE_EXISTING);
        } catch (AccessDeniedException ignored) {
            // The file already exists, or we don't have permission to write to the directory
        }
        System.load(new File(directory, nativeLibraryName).getAbsolutePath());
    }

    /**
     * Extract the native library and load it
     *
     * @throws IOException          In case an error occurs while extracting the native library
     * @throws UnsatisfiedLinkError In case the native libraries fail to load
     */
    public static void setupWithTemporaryFolder() throws IOException {
        File temporaryDir = Files.createTempDirectory("placebo-jni").toFile();
        temporaryDir.deleteOnExit();

        try {
            loadNative(temporaryDir);
        } catch (UnsatisfiedLinkError e) {
            e.printStackTrace();

            // Try without ARM support
            loadNative(temporaryDir, false);
        }
    }

    /***********************/
    /*** private methods ***/
    /***********************/

    private static String getNativeLibraryName(boolean allowArm) {
        String bitnessArch = System.getProperty("os.arch").toLowerCase();
        String bitnessDataModel = System.getProperty("sun.arch.data.model", null);

        boolean is64bit = bitnessArch.contains("64") || (bitnessDataModel != null && bitnessDataModel.contains("64"));
        String arch = bitnessArch.startsWith("aarch") && allowArm ? "arm" : "";

        if (is64bit) {
            String library64 = processLibraryName("libplacebo-jni-native-" + arch + "64");
            if (hasResource("/native-binaries/" + library64)) {
                return library64;
            }
        } else {
            String library32 = processLibraryName("libplacebo-jni-native-" + arch + "32");
            if (hasResource("/native-binaries/" + library32)) {
                return library32;
            }
        }

        String library = processLibraryName("libplacebo-jni-native");
        if (!hasResource("/native-binaries/" + library)) {
            throw new NoSuchElementException("No binary for the current system found, even after trying bit neutral names");
        } else {
            return library;
        }
    }

    private static String processLibraryName(String library) {
        String systemName = System.getProperty("os.name", "bare-metal?").toLowerCase();

        if (systemName.contains("nux") || systemName.contains("nix")) {
            return "lib" + library + ".so";
        } else if (systemName.contains("mac")) {
            return "lib" + library + ".dylib";
        } else if (systemName.contains("windows")) {
            return library + ".dll";
        } else {
            throw new NoSuchElementException("No native library for system " + systemName);
        }
    }

    private static boolean hasResource(String resource) {
        return PlaceboManager.class.getResource(resource) != null;
    }
}
