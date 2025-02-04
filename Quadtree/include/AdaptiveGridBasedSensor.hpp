#pragma once

#include "Sensor.hpp"
#include "Quadtree.hpp"

class AdaptiveGridBasedSensor : public Sensor {

public:

    struct AdaptiveGridBasedSensorData {

        // Sensor data structure
        std::string sensor_id;
        std::string timestamp;
        sf::Vector2i cellId;
        int count;
    };

    // Base constructor for simulation
    AdaptiveGridBasedSensor(
        float frameRate, sf::FloatRect detectionArea, 
        int maxDepth, const std::string& databaseName, 
        const std::string& collectionName, 
        std::shared_ptr<mongocxx::client> client);

    // Alternative constructor for rendering
    AdaptiveGridBasedSensor(
        sf::FloatRect detectionArea, 
        sf::Color detectionAreaColor, 
        int maxDepth, 
        bool showGrid);

    ~AdaptiveGridBasedSensor();
    float cellSize;
    bool showGrid = false;
    Quadtree adaptiveGrid;
    sf::Vector2f position = sf::Vector2f(detectionArea.left, detectionArea.top);

    void update(std::vector<Agent>& agents, float timeStep, sf::Time simulationTime, std::string date) override;
    void postData() override;
    void postMetadata() override;
    void printData() override;
    void clearDatabase() override;
    void calculateCellDensity();

private:
    mongocxx::database db;
    mongocxx::collection collection;

    std::unordered_map<int, std::unordered_map<std::string, int>> adaptiveGridData; // Grid Data: map(cell id, map(agent type, count) + hash function for sf::Vector2f)
    // std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash>>> dataStorage;
    std::vector<std::pair<std::string, std::unordered_map<int, std::unordered_map<std::string, int>>>> dataStorage; // Data Storage: timestamp, map(cell id, map(agent type, count) + hash function for sf::Vector2f)
};