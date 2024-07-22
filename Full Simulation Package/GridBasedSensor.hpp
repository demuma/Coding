#ifndef GRIDGBASEDSENSOR_HPP
#define GRIDGBASEDSENSOR_HPP

#include "Sensor.hpp"

class GridBasedSensor : public Sensor {

public:

    struct GridBasedSensorData {

        // Sensor data structure
        std::string sensor_id;
        std::string timestamp;
        sf::Vector2i cellIndex;
        int count;
    };

    GridBasedSensor(
        float frameRate, sf::FloatRect detectionArea, 
        sf::Color detectionAreaColor, float cellSize, 
        bool showGrid, const std::string& databaseName, 
        const std::string& collectionName, 
        std::shared_ptr<mongocxx::client> client);
    ~GridBasedSensor();
    float cellSize;
    bool showGrid = false;

    void update(const std::vector<Agent>& agents, float deltaTime) override;
    void postData() override;
    void printData() override;

private:
    mongocxx::database db;
    mongocxx::collection collection;
    std::string data = "Data from GridBasedSensor";
    std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash> gridData;
    std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<sf::Vector2i, std::unordered_map<std::string, int>, Vector2iHash>>> dataStorage;

    sf::Vector2i getCellIndex(const sf::Vector2f& position) const;
};

#endif // GRIDGBASEDSENSOR_HPP