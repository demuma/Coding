#include "Sensor.hpp"
#include <cmath>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <uuid/uuid.h>

// Base Sensor constructor
Sensor::Sensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor)
    : frameRate(frameRate), timeSinceLastUpdate(0.0f), detectionAreaColor(detectionAreaColor) {}

// Base Sensor velocity estimation
void Sensor::estimateVelocities(std::unordered_map<std::string, sf::Vector2f>& estimatedVelocities) {

    // Estimate the velocities of the agents
    for (const auto& kvp : previousPositions) {

        const std::string& agentUUID = kvp.first;
        const sf::Vector2f& previousPosition = kvp.second;

        // If the agent is still in the detection area
        if (currentPositions.find(agentUUID) != currentPositions.end()) {

            const sf::Vector2f& currentPosition = currentPositions[agentUUID];
            sf::Vector2f estimatedVelocity = (currentPosition - previousPosition) * frameRate;
            estimatedVelocities[agentUUID] = estimatedVelocity;
        }
    }
}

// Constructor
AgentBasedSensor::AgentBasedSensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor)
    : Sensor(frameRate, detectionArea, detectionAreaColor) {
        this->detectionArea = detectionArea;
        this->detectionAreaColor = detectionAreaColor;
        this->sensor_id = generateUUID();
    }

// Destructor
AgentBasedSensor::~AgentBasedSensor() {}

// Update method for agent-based sensor, taking snapshot of agents in detection area
void AgentBasedSensor::update(const std::vector<Agent>& agents, float deltaTime) {
    
    // Update the time since the last update
    timeSinceLastUpdate += deltaTime;

    // Update the estimated velocities at the specified frame rate
    if (timeSinceLastUpdate >= 1.0f / frameRate) {

        // Capture agent data (Modified)
        captureAgentData(agents);

        // Estimate the velocities of the agents
        estimateVelocities(estimatedVelocities);

        // Reset for next update
        previousPositions = currentPositions;
        currentPositions.clear();
        timeSinceLastUpdate = 0.0f;
    }
}

// Make a snapshot of the agents in the detection area
void AgentBasedSensor::captureAgentData(const std::vector<Agent>& agents) {

    // Capture the current positions of the agents
    for (const Agent& agent : agents) {

        // Check if the agent is within the detection area
        if (detectionArea.contains(agent.position)) {

            // Prepare SensorData object for the agent
            AgentBasedSensorData agentData;
            agentData.sensor_id = this->sensor_id;
            agentData.agent_id = agent.uuid;
            agentData.timestamp = generateISOTimestamp();
            agentData.type = agent.type;
            agentData.position = agent.position;

            // Estimate and store the velocity of the agent TODO: Only save with velocity
            if (previousPositions.find(agent.uuid) != previousPositions.end()) {
                sf::Vector2f prevPos = previousPositions[agent.uuid];
                agentData.estimatedVelocity = (agent.position - previousPositions[agent.uuid]) * frameRate;
            }

            // Print the agent data
            // std::cout << "Agent " << agent.uuid << " detected at " << agent.position.x << ", " << agent.position.y << std::endl;

            // Store the agent data
            dataStorage.push_back(agentData);

            // Store the current position of a specific agent using its UUID 
            currentPositions[agent.uuid] = agent.position;
        }
    }
}

// Save data method for agent-based sensor
void AgentBasedSensor::saveData() {

    // Now responsible only for printing/saving the stored data
    // std::cout << "Agent-Based Sensor Data:" << std::endl;

    // Print the stored data
    // for (const AgentBasedSensorData& data : dataStorage) {

    //     std::cout << "  Timestamp: " << data.timestamp << std::endl;
    //     std::cout << "  Sensor ID: " << data.sensor_id << std::endl;
    //     std::cout << "  Agent ID: " << data.agent_id << std::endl;
    //     std::cout << "  Agent Type: " << data.type << std::endl;
    //     std::cout << "  Position: (" << data.position.x << ", " << data.position.y << ")" << std::endl;
    //     std::cout << "  Estimated Velocity: (" << data.estimatedVelocity.x << ", " << data.estimatedVelocity.y << ")" << std::endl;
    //     std::cout << std::endl;
    // }

    // Optionally, save to MongoDB or another storage here
}

void AgentBasedSensor::printData() {

    // Print the stored data
    for (const AgentBasedSensorData& data : dataStorage) {

        std::cout << "  Timestamp: " << data.timestamp << std::endl;
        std::cout << "  Sensor ID: " << data.sensor_id << std::endl;
        std::cout << "  Agent ID: " << data.agent_id << std::endl;
        std::cout << "  Agent Type: " << data.type << std::endl;
        std::cout << "  Position: (" << data.position.x << ", " << data.position.y << ")" << std::endl;
        std::cout << "  Estimated Velocity: (" << data.estimatedVelocity.x << ", " << data.estimatedVelocity.y << ")" << std::endl;
        std::cout << std::endl;
    }
}

// Constructor
GridBasedSensor::GridBasedSensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor, float cellSize, bool showGrid)
    : Sensor(frameRate, detectionArea, detectionAreaColor), cellSize(cellSize) {
        this->detectionArea = detectionArea;
        this->detectionAreaColor = detectionAreaColor;
        this->showGrid = showGrid;
        this->sensor_id = generateUUID();
    }

// Destructor
GridBasedSensor::~GridBasedSensor() {}

// GridBasedSensor update method
void GridBasedSensor::update(const std::vector<Agent>& agents, float deltaTime) {

    timeSinceLastUpdate += deltaTime;
    if (timeSinceLastUpdate >= 1.0f / frameRate) {
        gridData.clear();
        for (const Agent& agent : agents) {
            if (detectionArea.contains(agent.position)) {
                sf::Vector2i cellIndex = getCellIndex(agent.position);
                gridData[cellIndex][agent.type]++;
            }
        }

        previousPositions.clear();
        timeSinceLastUpdate = 0.0f;
    }
}

// Save data method for grid-based sensor
void GridBasedSensor::saveData() {

    auto currentTime = std::chrono::system_clock::now();

    std::time_t now = std::chrono::system_clock::to_time_t(currentTime);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%FT%TZ");

    // Store grid data
    dataStorage.push_back({currentTime, gridData});
    //gridData.clear();
}

void GridBasedSensor::printData() {

    auto currentTime = std::chrono::system_clock::now();

    std::time_t now = std::chrono::system_clock::to_time_t(currentTime);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%FT%TZ");
    
    // Print grid data
    for (const auto& kvp : gridData) {

        const sf::Vector2i& cellIndex = kvp.first;
        const std::unordered_map<std::string, int>& cellData = kvp.second;

        std::cout << "Current time: " << ss.str() << " Cell (" << cellIndex.x << ", " << cellIndex.y << "): ";

        // Going through key-value pairs in the cell data
        for (const auto& kvp : cellData) {
            
            // Print agent type and count
            const std::string& agentType = kvp.first;
            int count = kvp.second;
            std::cout << agentType << ": " << count << ", ";
        }
        std::cout << std::endl;
    }
    gridData.clear();

}

// Helper function to get cell index based on position
sf::Vector2i GridBasedSensor::getCellIndex(const sf::Vector2f& position) const {

    int x = static_cast<int>((position.x - detectionArea.left) / cellSize);
    int y = static_cast<int>((position.y - detectionArea.top)/ cellSize);

    return sf::Vector2i(x, y);
}
