#include <SFML/Graphics.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <queue>
#include <functional>
#include <condition_variable>
#include <type_traits>
#include <random>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <iomanip>
#include <sstream>

#include "../include/Utilities.hpp"
#include "../include/PerlinNoise.hpp"
#include "../include/Logging.hpp"

// #define DEBUG
// #define STATS
// #define ERROR


/*********************************/
/********** AGENT CLASS **********/
/*********************************/

// Agent class
class Agent {
public:
    Agent();
    ~Agent();

    void calculateTrajectory(float waypointDistance);
    void calculateVelocity(sf::Vector2f waypoint);
    void updatePosition(float timeStep);
    void updateVelocity(float timeStep, sf::Time simulationTime);

    // Positional features
    sf::Vector2f initialPosition;
    sf::Vector2f targetPosition;
    float theta; // Orientation angle in radians
    float waypointDistance;
    float velocityMagnitude;
    sf::Vector2f initialVelocity;
    std::vector<sf::Vector2f>(trajectory);
    
    // Main features
    std::string timestamp;
    std::string uuid;
    std::string sensor_id;
    std::string type;
    sf::Vector2f position;
    sf::Vector2f velocity;

    // Visual
    sf::Color initialColor;
    sf::Color color;
    float bodyRadius;

    // Status
    int priority;

    // Perlin noise
    unsigned int noiseSeed;
    PerlinNoise perlinNoise;
    float noiseScale = 0.05;
    float noiseFactor = 0.5;


    // Use struct to reduce memory usage
    struct AgentFeatureData {
        
        std::string timestamp;
        std::string uuid;
        std::string sensor_id;
        std::string type;
        sf::Vector2f position;
        sf::Vector2f velocity;
    };
};

Agent::Agent(){};

Agent::~Agent(){

    trajectory.clear();
};

void Agent::calculateVelocity(sf::Vector2f waypoint) {
    
    // Calculate velocity based on heading to the next waypoint
    theta = std::atan2(waypoint.y - position.y, waypoint.x - position.x);
    
    // Calculate the heading vector
    sf::Vector2f heading;
    heading.x = std::cos(theta);
    heading.y = std::sin(theta);

    // Set the velocity vector based on the heading
    velocity = heading * velocityMagnitude;
}

void Agent::updatePosition(float timeStep) {

    // Update the position based on the velocity
    position.x += velocity.x * timeStep;
    position.y += velocity.y * timeStep;
}

// Update the agent's velocity based on Perlin noise
void Agent::updateVelocity(float deltaTime, sf::Time totalElapsedTime) {
    
    // Use the agent's own Perlin noise generator to fluctuate velocity
    float noiseX = perlinNoise.noise(position.x * noiseScale, position.y * noiseScale, totalElapsedTime.asSeconds()) * 2.0f - 1.0f;
    float noiseY = perlinNoise.noise(position.x * noiseScale, position.y * noiseScale, totalElapsedTime.asSeconds() + 1000.0f) * 2.0f - 1.0f;

    // Apply noise to velocity
    velocity.x = initialVelocity.x + noiseX / 3.6 * noiseFactor;
    velocity.y = initialVelocity.y + noiseY / 3.6 * noiseFactor;
}

void Agent::calculateTrajectory(float waypointDistance) {
    
    // Clear existing trajectory data
    trajectory.clear();

    // Add the initial position as the starting waypoint
    trajectory.push_back(initialPosition);

    // Save the initial position
    sf::Vector2f currentPosition = initialPosition;

    // Calculate total distance to target
    double totalDistance = sqrt(pow((targetPosition.x - initialPosition.x), 2) + pow((targetPosition.y - initialPosition.y), 2));
    
    // Round down to the nearest integer
    int numWaypoints = floor(totalDistance / waypointDistance);
    
    // If there are no waypoints, go directly to the target
    if (numWaypoints < 1) {
        trajectory.push_back(targetPosition);
        return;
    }
    
    // Calculate angle to the target
    double angle = atan2(targetPosition.y - initialPosition.y, targetPosition.x - initialPosition.x); // in radians
    
    // Compute delta positions
    double deltaX = waypointDistance * cos(angle);
    double deltaY = waypointDistance * sin(angle);

    // Add intermediate waypoints
    for (int i = 0; i < numWaypoints; ++i) {
        currentPosition.x += deltaX;
        currentPosition.y += deltaY;
        trajectory.push_back(currentPosition);
    }

    // Add the final target position as the last waypoint
    trajectory.push_back(targetPosition);
}    

/**************************************/
/********** SIMULATION CLASS **********/
/**************************************/

// Simulation class
class Simulation {
public:
    Simulation();
    ~Simulation();
    void run();
    void update();
    float getCurrentFrameRate();
    void initializeAgents();
    void loadConfiguration();

private:
    
    // Simulation parameters
    // ThreadPool threadPool;
    float currentSimulationTimeStep;
    bool stop = false;
    YAML::Node config;
    int numThreads;

    // Timing parameters
    float timeStep;
    size_t maxFrames;
    int targetSimulationTime;
    sf::Time simulationTime;

    // Window parameters
    int windowWidth;
    int windowHeight;

    // Agent parameters
    int numAgents;
    std::vector<Agent> agents;
    float waypointDistance;
    int currentNumAgents;

public:

    // Buffers
    std::vector<std::vector<Agent>> buffer; // Reference to the queue
    int writeBufferIndex = 0;
    size_t currentFrameIndex = 0;
};

Simulation::Simulation() {
    
    // Set the initial write buffer (second queue buffer) -> TODO: Make SharedBuffer class and move this logic there
    DEBUG_MSG("Simulation: write buffer: " << buffer);
    
    loadConfiguration();

    // Reserve space for the buffer to store simulation frames
    buffer.reserve(maxFrames);

    // ThreadPool threadPool(numThreads); instead of threadPool(std::thread::hardware_concurrency())
    // TODO: set number of threads for thread pool (no lazy initialization) -> initializeThreadPool();
    initializeAgents();
}

Simulation::~Simulation() {
    agents.clear();
    buffer.clear();
}

void Simulation::loadConfiguration() {

    // Configuration Loading
    try {
        config = YAML::LoadFile("config.yaml");
    } catch (const YAML::Exception& e) {
        ERROR_MSG("Error loading config file: " << e.what());
        return;
    }

    // Load simulation parameters
    timeStep = config["simulation"]["time_step"].as<float>();

    // Set the maximum number of frames if the duration is specified
    if(config["simulation"]["duration_seconds"]) {
        maxFrames = config["simulation"]["duration_seconds"].as<int>() / timeStep;
        targetSimulationTime = config["simulation"]["duration_seconds"].as<int>();
    } else {
        maxFrames = config["simulation"]["maximum_frames"].as<int>();
        targetSimulationTime = maxFrames * timeStep;
    }

    // Load display parameters
    windowHeight = config["display"]["height"].as<int>();
    windowWidth = config["display"]["width"].as<int>();

    // Agent parameters
    waypointDistance = config["agents"]["waypoint_distance"].as<float>();
    numAgents = config["agents"]["num_agents"].as<int>();

    // Load number of threads -> TODO: Use this to initialize the thread pool
    if(config["simulation"]["num_threads"].as<int>()) {
        numThreads = config["simulation"]["num_threads"].as<int>();
    } else {
        numThreads = std::thread::hardware_concurrency();
    }
}

void Simulation::initializeAgents() {

    // Preallocate space for agents to improve performance
    agents.reserve(numAgents);
    
    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disPos(0, windowHeight); // Position
    std::uniform_real_distribution<> disVel(10, 50); // Velocity
    float agentRadius = 5.f;
    
    // Initialize agent vector with random velocity, start and target positions
    for (int i = 0; i < numAgents; ++i) {
        Agent agent;
        agent.velocityMagnitude = static_cast<float>(disVel(gen));
        agent.initialPosition = sf::Vector2f(0.f, static_cast<float>(disPos(gen)));
        agent.targetPosition = sf::Vector2f(windowWidth, static_cast<float>(disPos(gen)));
        agent.position = agent.initialPosition;
        agent.bodyRadius = agentRadius;
        agent.waypointDistance = waypointDistance; // -> TODO: Use taxonomy for waypoint distance
        agent.calculateTrajectory(agent.waypointDistance);
        agent.calculateVelocity(agent.trajectory[1]);
        agent.initialVelocity = agent.velocity;
        agents.push_back(agent);
    }
}

void Simulation::run() {

    // Initialize the clock
    sf::Clock simulationClock;
    simulationClock.restart();
    
    // Initialize the simulation time step
    simulationTime = sf::Time::Zero;
    float writeBufferTime = 0.f;
    float simulationUpdateTime = 0.0f;

    // Set simulation time step
    sf::Time simulationStepTime = sf::Time::Zero;
    simulationStepTime = sf::seconds(timeStep);

    // Set the initial time step
    currentSimulationTimeStep = timeStep;

    // Get indices for both buffers
    writeBufferIndex = 0;
    simulationTime += simulationClock.getElapsedTime();

    // Main simulation loop
    while ((currentFrameIndex < maxFrames)) {

        // Restart the clock
        simulationClock.restart();

        // Update the agents
        update();

        // Update the simulation update time
        simulationUpdateTime += simulationClock.getElapsedTime().asSeconds();

        // Write to the current write buffer
        DEBUG_MSG("Simulation: writing frame: " << currentFrameIndex << "to buffer: " << writeBufferIndex);
        buffer.push_back(agents);

        writeBufferTime += simulationClock.getElapsedTime().asSeconds();
        
        // Increment the frame index
        ++currentFrameIndex;

        // Calculate the time step for the simulation
        simulationStepTime = simulationClock.getElapsedTime();

        // Update the total simulation time
        simulationTime += simulationStepTime;

        // Set atomic time step
        currentSimulationTimeStep = simulationStepTime.asSeconds();
    }

    // Signalize the simulation has finished so that the renderer can swap buffers if needed
    DEBUG_MSG("Simulation: finished");
    stop = true;

    // Total buffer ram size
    size_t bufferRamSize = buffer.size() * sizeof(Agent) * numAgents;

    // Print simulation statistics
    STATS_MSG("Total simulation wall time: " << simulationTime.asSeconds() << " seconds for " << currentFrameIndex << " frames");
    STATS_MSG("Total simulation time: " << maxFrames * timeStep << " seconds" << " for " << numAgents << " agents");
    STATS_MSG("Simulation speedup: " << (maxFrames * timeStep) / simulationTime.asSeconds());
    STATS_MSG("Frame rate: " << 1/(simulationTime.asSeconds() / (currentFrameIndex + 1)));
    STATS_MSG("Average simulation update time: " << simulationUpdateTime / (currentFrameIndex + 1));
    STATS_MSG("Average simulation time step: " << simulationTime.asSeconds() / (currentFrameIndex + 1));
    STATS_MSG("Average write buffer time: " << writeBufferTime / (currentFrameIndex + 1));
    STATS_MSG("Total write buffer size: " << bufferRamSize / std::pow(1024, 2) << " MB");

    return;
}

void Simulation::update() {

    // Loop through all agents and update their positions
    for (auto& agent : agents) {

        // Add agent updates as tasks to the thread pool
        agent.updatePosition(timeStep);
        agent.updateVelocity(timeStep, simulationTime);
    }

    // Update the current number of agents
    currentNumAgents = agents.size();
}

/**************************/
/********** MAIN **********/
/**************************/

// Main function
int main() {

    Simulation simulation;

    // std::thread simulationThread(&Simulation::run, &simulation);

    // simulationThread.join();

    simulation.run();

    return 0;
}