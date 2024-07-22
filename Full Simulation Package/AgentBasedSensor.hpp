#ifndef AGENTBASEDSENSOR_HPP
#define AGENTBASEDSENSOR_HPP

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

    AgentBasedSensor(
        float frameRate, 
        sf::FloatRect detectionArea, 
        sf::Color detectionAreaColor,
        const std::string& databaseName,
        const std::string& collectionName,
        std::shared_ptr<mongocxx::client> client);
    ~AgentBasedSensor();

    void update(const std::vector<Agent>& agents, float deltaTime) override;
    void captureAgentData(const std::vector<Agent>& agent);
    void postData() override;
    void printData() override;

private:
    mongocxx::database db;
    mongocxx::collection collection;
    std::string data = "Data from AgentBasedSensor";

    // Mapping of agent ID to estimated velocity
    std::unordered_map<std::string, sf::Vector2f> estimatedVelocities;

    // Data storage structure
    //std::vector<std::pair<std::chrono::system_clock::time_point, std::unordered_map<std::string, sf::Vector2f>>> dataStorage;
    std::vector<AgentBasedSensorData> dataStorage;
};

#endif // AGENTBASEDSENSOR_HPP