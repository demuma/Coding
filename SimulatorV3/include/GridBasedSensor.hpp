#pragma once

#include "Sensor.hpp"
#include "CollisionGrid.hpp"

class GridBasedSensor : public Sensor {

public:

    struct GridBasedSensorData {

        // Sensor data structure
        std::string sensorId;
        std::string timestamp;
        sf::Vector2i cellIndex;
        int count;
    };

    // Base constructor for simulation
    GridBasedSensor(
        float frameRate, 
        sf::FloatRect detectionArea, 
        float cellSize, 
        const std::string& databaseName, 
        const std::string& collectionName, 
        std::shared_ptr<mongocxx::client> client
    );

    // Alternative constructor for rendering
    GridBasedSensor(
        sf::FloatRect detectionArea, 
        sf::Color detectionAreaColor, 
        float cellSize, 
        bool showGrid
    );

    ~GridBasedSensor();
    float cellSize;
    bool showGrid = false;
    Grid currentGrid;
    Grid previousGrid;
    sf::Vector2f position = sf::Vector2f(detectionArea.position.x, detectionArea.position.y);

    void update(std::vector<Agent>& agents, float timeStep, sf::Time simulationTime, std::string date) override;
    void postData() override;
    void postMetadata() override;
    void printData() override;
    void clearDatabase() override;
    void calculateCellDensity();

private:
    mongocxx::database db;
    mongocxx::collection collection;

    std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash> gridData; // Grid Data: map(cell index, map(agent type, count) + hash function for sf::Vector2i)
    // std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash>>> dataStorage;
    std::vector<std::pair<std::string, std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash>>> dataStorage; // Data Storage: timestamp, map(cell index, map(agent type, count) + hash function for sf::Vector2i)
    
    sf::Vector2i getCellIndex(const sf::Vector2f& position) const;
    sf::Vector2f getCellPosition(const sf::Vector2i& cellIndex) const;
};