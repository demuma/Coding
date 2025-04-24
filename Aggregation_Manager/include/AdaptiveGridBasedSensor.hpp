#pragma once

#include "Sensor.hpp"
#include "CollisionGrid.hpp"
#include "Quadtree.hpp"
#include "AggregationManager.hpp"

class AdaptiveGridBasedSensor : public Sensor {

public:

    // Base constructor for simulation
    AdaptiveGridBasedSensor(
        float frameRate, 
        sf::FloatRect detectionArea, 
        float cellSize, 
        int maxDepth, 
        const std::string& databaseName, 
        const std::string& collectionName, 
        std::shared_ptr<mongocxx::client> client
    );

    // Alternative constructor for rendering
    AdaptiveGridBasedSensor(
        sf::FloatRect detectionArea, 
        sf::Color detectionAreaColor, 
        float cellSize,
        int maxDepth,
        bool showGrid
    );

    ~AdaptiveGridBasedSensor();
    float cellSize;
    bool showGrid = false;
    int maxDepth;
    Quadtree adaptiveGrid;
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

    std::unordered_map<int, std::unordered_map<std::string, int>> adaptiveGridData; // Adaptive Grid Data: map(cell id, map(agent type, count)
    // std::vector<std::pair<std::string, std::unordered_map<int, std::unordered_map<std::string, int>>>> dataStorage; // Data Storage: timestamp, map(cell id, map(agent type, count)
    std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<int, std::unordered_map<std::string, int>>>> dataStorage; // Data Storage: timestamp, map(cell id, map(agent type, count)
    
    sf::Vector2i getCellIndex(const sf::Vector2f& position) const;
    sf::Vector2f getCellPosition(const sf::Vector2i& cellIndex) const;
};