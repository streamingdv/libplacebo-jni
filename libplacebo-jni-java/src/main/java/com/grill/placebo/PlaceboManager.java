package com.grill.placebo;

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

    public static final int API_VERSION = 342;

    /**
     * Creates a logger with the log callback
     * @param apiVersion the API version
     * @param logLevel the log level
     * @param callback the callback
     * @return the log handle
     */
    public native long createLog(int apiVersion, int logLevel, LogCallback callback);

    /**
     * Deletes a logger with the log callback
     * @param logHandle the log handle to be deleted
     */
    public native void cleanup(long logHandle);
}
