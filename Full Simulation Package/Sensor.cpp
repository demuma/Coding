#include "Sensor.hpp"
#include <cmath>
#include <iostream>

// Base Sensor constructor
Sensor::Sensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor)
    : frameRate(frameRate), timeSinceLastUpdate(0.0f), detectionAreaColor(detectionAreaColor) {}

// Base Sensor velocity estimation
void Sensor::estimateVelocities(std::unordered_map<std::string, sf::Vector2f>& estimatedVelocities) {

    for (const auto& kvp : previousPositions) {

        const std::string& agentUUID = kvp.first;
        const sf::Vector2f& previousPosition = kvp.second;
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

    timeSinceLastUpdate += deltaTime;
    if (timeSinceLastUpdate >= 1.0f / frameRate) {
        for (const Agent& agent : agents) {
            if (detectionArea.contains(agent.position)) {
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
GridBasedSensor::GridBasedSensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor, float cellSize)
    : Sensor(frameRate, detectionArea, detectionAreaColor), cellSize(cellSize) {
        this->detectionArea = detectionArea;
        this->detectionAreaColor = detectionAreaColor;
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

    // Print grid data
    for (const auto& kvp : gridData) {

        const sf::Vector2i& cellIndex = kvp.first;
        const std::unordered_map<std::string, int>& cellData = kvp.second;
        //std::cout << "Cell (" << cellIndex.x << ", " << cellIndex.y << "): ";

        for (const auto& kvp : cellData) {

            const std::string& agentType = kvp.first;
            int count = kvp.second;
            //std::cout << agentType << ": " << count << ", ";
        }
        //std::cout << std::endl;
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
