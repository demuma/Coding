#include "Sensor.hpp"
#include <cmath>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

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
    }

// Destructor
AgentBasedSensor::~AgentBasedSensor() {}

// Update method for agent-based sensor
void AgentBasedSensor::update(const std::vector<Agent>& agents, float deltaTime) {
    
    // Update the time since the last update
    timeSinceLastUpdate += deltaTime;

    // Update the estimated velocities at the specified frame rate
    if (timeSinceLastUpdate >= 1.0f / frameRate) {

        // Clear the current positions
        for (const Agent& agent : agents) {

            // Check if the agent is within the detection area
            if (detectionArea.contains(agent.position)) {

                // Store the current position of a specific agent using its UUID 
                currentPositions[agent.uuid] = agent.position;
            }
        }

        estimateVelocities(estimatedVelocities);
        previousPositions = currentPositions;
        currentPositions.clear();
        timeSinceLastUpdate = 0.0f;
    }
}

// Save data method for agent-based sensor
void AgentBasedSensor::saveData() {

    auto currentTime = std::chrono::system_clock::now();
    dataStorage.push_back({currentTime, estimatedVelocities});
    estimatedVelocities.clear();
}

// Constructor
GridBasedSensor::GridBasedSensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor, float cellSize, bool showGrid)
    : Sensor(frameRate, detectionArea, detectionAreaColor), cellSize(cellSize) {
        this->detectionArea = detectionArea;
        this->detectionAreaColor = detectionAreaColor;
        this->showGrid = showGrid;
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
    dataStorage.push_back({currentTime, gridData});
    gridData.clear();
}

// Helper function to get cell index based on position
sf::Vector2i GridBasedSensor::getCellIndex(const sf::Vector2f& position) const {

    int x = static_cast<int>((position.x - detectionArea.left) / cellSize);
    int y = static_cast<int>((position.y - detectionArea.top)/ cellSize);

    return sf::Vector2i(x, y);
}
