#include "Simulation.hpp"
#include <mongocxx/uri.hpp>
#include <iostream>

Simulation::Simulation(const std::string& dbUri, const std::string& databaseName)
    : dbUri(dbUri), databaseName(databaseName), instance{} {}

void Simulation::initializeMongoDB() {
    mongocxx::uri uri(dbUri);
    client = std::make_shared<mongocxx::client>(uri);
    std::cout << "MongoDB initialized with URI: " << dbUri << std::endl;
}

void Simulation::initializeSensors() {
    std::string collection1 = "AB_Sensor_Data";
    std::string collection2 = "GB_Sensor_Data";

    sensors.push_back(std::make_unique<AgentBasedSensor>(databaseName, collection1, client));
    sensors.push_back(std::make_unique<AgentBasedSensor>(databaseName, collection2, client));

    std::cout << "Sensors initialized." << std::endl;
}

void Simulation::updateSensors() {
    for (auto& sensor : sensors) {
        sensor->getData();
        sensor->postData();
    }
    std::cout << "Sensors updated." << std::endl;
}