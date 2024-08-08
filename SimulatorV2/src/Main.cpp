#include <SFML/Graphics.hpp>
#include <thread>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <atomic>

#include "../include/SharedBuffer.hpp"
#include "../include/Simulation.hpp"
#include "../include/Renderer.hpp"
#include "../include/Logging.hpp"

// #define DEBUG
// #define STATS
// #define ERROR



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
    SharedBuffer buffer;

    // Load global configuration data
    float timeStep = config["simulation"]["time_step"].as<float>();
    int maxFrames = config["simulation"]["maximum_frames"].as<int>();
    int numAgents = config["agents"]["num_agents"].as<int>();

    // Shared variabes
    std::atomic<float> currentSimulationTimeStep{timeStep};
    std::atomic<int> numSimulationFrames{maxFrames};
    std::atomic<bool> stop{false};
    std::atomic<int> currentNumAgents{numAgents};

    Simulation simulation(buffer, currentSimulationTimeStep, stop, currentNumAgents, config);
    Renderer renderer(buffer, currentSimulationTimeStep, stop, currentNumAgents, config);

    std::thread simulationThread(&Simulation::run, &simulation);

    simulationThread.join();
    renderer.run();

    return 0;
}