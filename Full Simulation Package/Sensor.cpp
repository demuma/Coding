#include "Sensor.hpp"
#include <cmath>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <uuid/uuid.h>

// Base Sensor constructor
Sensor::Sensor(
    float frameRate, sf::FloatRect detectionArea, 
    sf::Color detectionAreaColor, std::shared_ptr<mongocxx::client> client)
    : frameRate(frameRate), timeSinceLastUpdate(0.0f), 
    detectionAreaColor(detectionAreaColor), client(std::move(client)), 
    sensor_id(generateUUID()) {}

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