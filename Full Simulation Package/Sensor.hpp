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
    virtual void printData() = 0;
    virtual ~Sensor() = default;
    sf::Color detectionAreaColor;
    sf::FloatRect detectionArea;
    float frameRate;

protected:
    
    float timeSinceLastUpdate;
    std::string sensor_id;
    std::unordered_map<std::string, sf::Vector2f> previousPositions;
    std::unordered_map<std::string, sf::Vector2f> currentPositions;

    void estimateVelocities(std::unordered_map<std::string, sf::Vector2f>& estimatedVelocities);
};

class AgentBasedSensor : public Sensor {
public:

    struct AgentBasedSensorData {

        // Sensor data structure
        std::string sensor_id;
        std::string agent_id;
        std::string timestamp;
        std::string type;
        sf::Vector2f position;
        sf::Vector2f estimatedVelocity;
    };

    AgentBasedSensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor);
    ~AgentBasedSensor();

    void update(const std::vector<Agent>& agents, float deltaTime) override;
    void captureAgentData(const std::vector<Agent>& agent);
    void saveData() override;
    void printData() override;

private:
    // Mapping of agent ID to estimated velocity
    std::unordered_map<std::string, sf::Vector2f> estimatedVelocities;

    // Data storage structure
    //std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<std::string, sf::Vector2f>>> dataStorage;
    std::vector<AgentBasedSensorData> dataStorage;
};

class GridBasedSensor : public Sensor {

public:

    struct GridBasedSensorData {

        // Sensor data structure
        std::string sensor_id;
        std::string timestamp;
        sf::Vector2i cellIndex;
        int count;
    };

    GridBasedSensor(float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor, float cellSize, bool showGrid);
    ~GridBasedSensor();
    float cellSize;
    bool showGrid = false;

    void update(const std::vector<Agent>& agents, float deltaTime) override;
    void saveData() override;
    void printData() override;

private:
    std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash> gridData;
    std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash>>> dataStorage;

    sf::Vector2i getCellIndex(const sf::Vector2f& position) const;
};

#endif // SENSOR_HPP
