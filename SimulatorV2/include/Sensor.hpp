#pragma once

#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <string>
#include <iostream>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include "Agent.hpp"
#include "Utilities.hpp"

class Sensor {
public:

    Sensor(float frameRate, sf::FloatRect detectionArea, std::shared_ptr<mongocxx::client> client);
    Sensor(const sf::FloatRect& detectionArea, const sf::Color& detectionAreaColor);
    virtual void update(std::vector<Agent>& agents, float timeStep, sf::Time simulationTime, std::string datetime) = 0;
    virtual void printData() = 0;
    virtual void postData() = 0;
    virtual void postMetadata() = 0;
    virtual void clearDatabase() = 0;
    virtual ~Sensor() = default;

    sf::Color detectionAreaColor;
    sf::FloatRect detectionArea;
    float frameRate;
    int scale;
    std::string timestamp;
    std::unordered_map<std::string, sf::Vector2f> previousPositions;
    std::unordered_map<std::string, sf::Vector2f> currentPositions;

protected:
    std::shared_ptr<mongocxx::client> client;
    std::string sensor_id;
    float timeSinceLastUpdate = 0.0f;

    void estimateVelocities(std::unordered_map<std::string, sf::Vector2f>& estimatedVelocities);
};