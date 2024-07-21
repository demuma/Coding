#include <iostream>
#include "Simulation.hpp"

int main() {
    std::string dbUri = "mongodb://localhost:27017";
    std::string database = "Simulation";

    Simulation simulation(dbUri, database);
    simulation.initializeMongoDB();
    simulation.initializeSensors();
    simulation.updateSensors();

    return 0;
}
