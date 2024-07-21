#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <memory>
#include <vector>
#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "Sensor.hpp"
#include "AgentBasedSensor.hpp"

class Simulation {
public:
    Simulation(const std::string& dbUri, const std::string& databaseName);
    void initializeMongoDB();
    void initializeSensors();
    void updateSensors();
private:
    std::string dbUri;
    std::string databaseName;
    std::shared_ptr<mongocxx::client> client;
    std::vector<std::unique_ptr<Sensor>> sensors;
    mongocxx::instance instance;
};

#endif // SIMULATION_HPP
