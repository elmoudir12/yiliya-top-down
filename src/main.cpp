#include "Engine.h"
#include <iostream>

int main() {
    try {
        Engine engine;
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
