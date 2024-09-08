#ifndef SENSOR_HPP
#define SENSOR_HPP

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
#include "Utilities.hpp" // Include the common utilities

class Sensor {
public:

    Sensor(
        float frameRate, 
        sf::FloatRect detectionArea, 
        sf::Color detectionAreaColor, 
        std::shared_ptr<mongocxx::client> client);
    virtual void update(const std::vector<Agent>& agents, float deltaTime, int frameCount, sf::Time totalElapsedTime, std::string date) = 0;
    virtual void printData() = 0;
    virtual void postData() = 0;
    virtual ~Sensor() = default;
    sf::Color detectionAreaColor;
    sf::FloatRect detectionArea;
    float frameRate;
    int scale;
    std::unordered_map<std::string, sf::Vector2f> previousPositions;
    std::unordered_map<std::string, sf::Vector2f> currentPositions;

protected:
    std::shared_ptr<mongocxx::client> client;
    std::string sensor_id;
    float timeSinceLastUpdate = 0.0f;

    void estimateVelocities(std::unordered_map<std::string, sf::Vector2f>& estimatedVelocities);
};

#endif // SENSOR_HPP