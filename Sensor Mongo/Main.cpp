#include <iostream>
#include "Simulation.hpp"

int main() {
    try {
        Simulation simulation;
        simulation.run();
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
