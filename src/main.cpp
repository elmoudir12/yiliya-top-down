#include "Engine.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <climits>

int main(int argc, char* argv[]) {
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        char* lastSlash = strrchr(exePath, '/');
        if (lastSlash) {
            *lastSlash = '\0';
            char* secondLast = strrchr(exePath, '/');
            if (secondLast) *secondLast = '\0';
            chdir(exePath);
        }
    }
    try {
        Engine engine;
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
