#pragma once

#include "Sensor.hpp"

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

    // Base constructor for simulation
    AgentBasedSensor(
        float frameRate, 
        sf::FloatRect detectionArea, 
        const std::string& databaseName,
        const std::string& collectionName,
        std::shared_ptr<mongocxx::client> client);

    // Alternative constructor for rendering
    AgentBasedSensor( 
        sf::FloatRect detectionArea, 
        sf::Color detectionAreaColor);

    ~AgentBasedSensor();
    sf::Vector2f position = sf::Vector2f(detectionArea.left, detectionArea.top);

    void update(std::vector<Agent>& agents, float timeStep, sf::Time simulationTime, std::string datetime) override;
    void captureAgentData(std::vector<Agent>& agents);
    void postData() override;
    void postMetadata() override;
    void printData() override;
    void clearDatabase() override;

private:
    mongocxx::database db;
    mongocxx::collection collection;

    // Mapping of agent ID to estimated velocity
    std::unordered_map<std::string, sf::Vector2f> estimatedVelocities;

    // Data storage structure
    //std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<std::string, sf::Vector2f>>> dataStorage;
    std::vector<AgentBasedSensorData> dataStorage;
};