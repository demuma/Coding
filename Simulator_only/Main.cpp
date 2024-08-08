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

// #define DEBUG
// #define STATS
// #define ERROR

#ifdef DEBUG
#define DEBUG_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define DEBUG_MSG(str) do { } while ( false )
#endif

#ifdef STATS
#define STATS_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define STATS_MSG(str) do { } while ( false )
#endif

#ifdef ERROR
#define ERROR_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define ERROR_MSG(str) do { } while ( false )
#endif

#ifdef TIMING
#define TIMING_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define TIMING_MSG(str) do { } while ( false )
#endif


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
    void updatePosition(float deltaTime);

    sf::FloatRect rect;
    sf::Vector2f velocity;
    float velocityMagnitude;
    sf::Vector2f position;
    sf::Vector2f initialPosition;
    sf::Vector2f targetPosition;
    sf::Vector2f heading;
    float waypointDistance;
    float radius;
    std::vector<sf::Vector2f>(trajectory);
};

Agent::Agent(){};

Agent::~Agent(){
    trajectory.clear();
};

void Agent::calculateVelocity(sf::Vector2f waypoint) {
    
    // Calculate velocity based on heading to the next waypoint
    float angle = std::atan2(waypoint.y - position.y, waypoint.x - position.x);
    
    // Calculate the heading vector
    sf::Vector2f heading;
    heading.x = std::cos(angle);
    heading.y = std::sin(angle);

    // Set the velocity vector based on the heading
    velocity = heading * velocityMagnitude;
}

void Agent::updatePosition(float timeStep) {

    // Update the position based on the velocity
    position.x += velocity.x * timeStep;
    position.y += velocity.y * timeStep;
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
    void cleanupBuffers();

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

    // Window parameters
    int windowWidth;
    int windowHeight;

    // Agent parameters
    int numAgents;
    std::vector<Agent> agents;
    float waypointDistance;
    int currentNumAgents;

    // Shared buffer -> TODO: Make SharedBuffer class
    std::vector<std::vector<Agent>> buffer; // Reference to the queue

    // Current write buffer
    std::vector<std::vector<Agent>>* currentWriteBuffer;

    // Buffers
    size_t currentFrameIndex = 0;
};

Simulation::Simulation() {
    
    // Set the initial write buffer (second queue buffer) -> TODO: Make SharedBuffer class and move this logic there
    currentWriteBuffer = &buffer;
    DEBUG_MSG("Simulation: write buffer: " << (currentWriteBuffer - buffers));
    
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
    currentWriteBuffer->clear();
}

void Simulation::cleanupBuffers() {
    for (auto& frame : *currentWriteBuffer) {
        frame.clear();
    }
    currentWriteBuffer->clear();
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
        agent.radius = agentRadius;
        agent.waypointDistance = waypointDistance; // -> TODO: Use taxonomy for waypoint distance
        agent.calculateTrajectory(agent.waypointDistance);
        agent.calculateVelocity(agent.trajectory[1]);
        agents.push_back(agent);
    }
}

void Simulation::run() {

    // Initialize the clock
    sf::Clock simulationClock;
    simulationClock.restart();
    
    // Initialize the simulation time step
    sf::Time simulationTime = sf::Time::Zero;
    float writeBufferTime = 0.f;

    // Set simulation time step
    sf::Time simulationStepTime = sf::Time::Zero;
    simulationStepTime = sf::seconds(timeStep);

    // Set the initial time step
    currentSimulationTimeStep = timeStep;

    // Get indices for both buffers
    int writeBufferIndex = 0;
    simulationTime += simulationClock.getElapsedTime();

    // Main simulation loop
    while ((currentFrameIndex < maxFrames)) {

        // Restart the clock
        simulationClock.restart();

        // Update the agents
        update();

        // Write to the current write buffer
        DEBUG_MSG("Simulation: writing frame: " << currentFrameIndex << "to buffer: " << writeBufferIndex);
        currentWriteBuffer->push_back(agents);

        writeBufferTime += simulationClock.getElapsedTime().asSeconds();
        
        // Increment the frame index
        ++currentFrameIndex;

        // Calculate the time step for the simulation
        simulationStepTime = simulationClock.getElapsedTime();

        // Calculate the total simulation time
        simulationTime += simulationStepTime;

        // Set atomic time step
        currentSimulationTimeStep = simulationStepTime.asSeconds();
    }

    // Signalize the simulation has finished so that the renderer can swap buffers if needed
    stop = true;

    // Cleanup buffers
    cleanupBuffers();
    DEBUG_MSG("Simulation: finished");

    // Print simulation statistics
    STATS_MSG("Total simulation wall time: " << simulationTime.asSeconds() << " seconds for " << currentFrameIndex << " frames");
    STATS_MSG("Total simulation time: " << maxFrames * timeStep << " seconds" << " for " << numAgents << " agents");
    STATS_MSG("Simulation speedup: " << (maxFrames * timeStep) / simulationTime.asSeconds());
    STATS_MSG("Frame rate: " << 1/(simulationTime.asSeconds() / (currentFrameIndex + 1)));
    STATS_MSG("Average simulation time step: " << simulationTime.asSeconds() / (currentFrameIndex + 1));
    STATS_MSG("Average write buffer time: " << writeBufferTime / (currentFrameIndex + 1));

    return;
}

void Simulation::update() {

    // Loop through all agents and update their positions
    for (auto& agent : agents) {

        // Add agent updates as tasks to the thread pool
        agent.updatePosition(timeStep);
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

    simulation.run();

    return 0;
}