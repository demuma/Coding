#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <vector>
#include <chrono>
#include "Agent.hpp"
#include "Utilities.hpp" // Include the common utilities

class Sensor {
public:
    Sensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor);
    virtual void update(const std::vector<Agent>& agents, float deltaTime) = 0;
    virtual void saveData() = 0;
    virtual ~Sensor() = default;
    sf::Color detectionAreaColor;
    sf::FloatRect detectionArea;
    float frameRate;
    bool showGrid;

protected:
    
    float timeSinceLastUpdate;
    std::string sensor_id;
    std::unordered_map<std::string, sf::Vector2f> previousPositions;
    std::unordered_map<std::string, sf::Vector2f> currentPositions;

    void estimateVelocities(std::unordered_map<std::string, sf::Vector2f>& estimatedVelocities);
};

class AgentBasedSensor : public Sensor {
public:
    AgentBasedSensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor);
    ~AgentBasedSensor();

    void update(const std::vector<Agent>& agents, float deltaTime) override;
    void saveData() override;

private:
    std::unordered_map<std::string, sf::Vector2f> estimatedVelocities;
    std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<std::string, sf::Vector2f>>> dataStorage;
};

class GridBasedSensor : public Sensor {
public:
    GridBasedSensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor, float cellSize);
    ~GridBasedSensor();

    void update(const std::vector<Agent>& agents, float deltaTime) override;
    void saveData() override;

private:
    float cellSize;
    std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash> gridData;
    std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash>>> dataStorage;

    sf::Vector2i getCellIndex(const sf::Vector2f& position) const;
};

#endif // SENSOR_HPP
