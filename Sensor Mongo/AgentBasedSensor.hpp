#ifndef AGENTBASEDSENSOR_HPP
#define AGENTBASEDSENSOR_HPP

#include "Sensor.hpp"
#include <string>

class AgentBasedSensor : public Sensor {
public:
    AgentBasedSensor(
        const std::string& databaseName, 
        const std::string& collectionName, 
        std::shared_ptr<mongocxx::client> client);
    void getData() override;
    void postData() override;
private:
    // Example data to store (replace with your actual AgentBasedSensorData structure)
    std::string data = "Data from AgentBasedSensor";
    mongocxx::database db;
    mongocxx::collection collection;
};

#endif // AGENTBASEDSENSOR_HPP
