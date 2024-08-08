#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <atomic>

#include "Agent.hpp"
#include "ThreadPool.hpp"
#include "SharedBuffer.hpp"
#include "Obstacle.hpp"
#include "Utilities.hpp"
#include "Logging.hpp"

/**************************************/
/********** SIMULATION CLASS **********/
/**************************************/

// Simulation class
class Simulation {
public:
    // Simulation(std::queue<std::vector<Agent>> (&buffer)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer);
    Simulation(SharedBuffer& buffer, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config);
    void run();
    void update();
    float getCurrentFrameRate();
    void initializeAgents();
    void loadConfiguration();
    void loadObstacles();

private:
    
    // Simulation parameters
    // ThreadPool threadPool;
    std::atomic<float>& currentSimulationTimeStep;
    std::atomic<bool>& stop;
    const YAML::Node& config;
    int numThreads;

    // Timing parameters
    float timeStep;
    size_t maxFrames;
    int targetSimulationTime;

    // Window parameters
    float simulationWidth; // Meters
    float simulationHeight; // Meters
    float scale;

    // Agent parameters
    int numAgents;
    std::vector<Agent> agents;
    std::atomic<int>& currentNumAgents;
    float waypointDistance;

    // Shared buffer reference
    SharedBuffer& buffer;
    size_t currentFrameIndex = 0;

    // Obstacles
    std::vector<Obstacle> obstacles;
};