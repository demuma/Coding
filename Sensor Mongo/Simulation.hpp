#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <memory>
#include <vector>
#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <yaml-cpp/yaml.h>
#include "Sensor.hpp"
#include "AgentBasedSensor.hpp"

class Simulation {
public:
    Simulation();
    void run();
private:
    void loadConfiguration();
    void initializeMongoDB();
    void initializeSensors();
    void updateSensors();

    std::string dbUri;
    std::string databaseName;
    YAML::Node config;
    mongocxx::instance instance; // Ensure instance is kept alive for the duration of Simulation
    std::shared_ptr<mongocxx::client> client;
    std::vector<std::unique_ptr<Sensor>> sensors;
};

#endif // SIMULATION_HPP
