namespace Config {

    struct Options {
        // Set to true for LSP/Library builds
        bool errorRecoveryEnabled = false;

        // Set to true to enable logging
        bool loggingEnabled = true;

        int maxErrorCount = 100;

        int threadCount         = 12;
        int threadWorkQueueSize = 32;

        // Starting file count to pre-allocate space for
        int initFileCount = 128;

        // expected namespace count per file used
        // ex. to pre-allocate sets
        int expectedNamespaceCount = 32;
    };

    constexpr int LINEAR_SEARCH_THRESHOLD = 16;

    // TODO: For now global, but there may be need to apply different options
    //       for different compilations within the very same program...
    extern Options opt;

}
