#include <SFML/Graphics.hpp>
#include <thread>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <atomic>

#include "../include/SharedBuffer.hpp"
#include "../include/Simulation.hpp"
#include "../include/Renderer.hpp"
#include "../include/Logging.hpp"

/**************************/
/********** MAIN **********/
/**************************/

// Main function
int main() {

    // Configuration Loading
    YAML::Node config;
    try {
        config = YAML::LoadFile("config.yaml");
    } catch (const YAML::Exception& e) {
        ERROR_MSG("Error loading config file: " << e.what());
        return 1;
    }

    // Buffer for shared data
    SharedBuffer agentBuffer;

    // Load global configuration data
    float timeStep = config["simulation"]["time_step"].as<float>();
    bool enableRendering = config["renderer"]["show_renderer"].as<bool>();

    // Shared variabes
    std::atomic<float> currentSimulationTimeStep{timeStep};

    Simulation simulation(agentBuffer, currentSimulationTimeStep, config);
    std::thread simulationThread(&Simulation::run, &simulation);

    // Run the renderer if not in headless mode
    if (enableRendering) {
        Renderer renderer(agentBuffer, currentSimulationTimeStep, config);
        renderer.run();
    }

    simulationThread.join();

    return 0;
}