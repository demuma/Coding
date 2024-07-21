#include "AgentBasedSensor.hpp"
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>

AgentBasedSensor::AgentBasedSensor( 
    const std::string& databaseName,
    const std::string& collectionName, 
    std::shared_ptr<mongocxx::client> client
) : Sensor(client),
    db(client->database(databaseName)), // Initialize db in initializer list
    collection(db[collectionName]) {     // Initialize collection in initializer list
}

void AgentBasedSensor::getData() {
    // Simulate getting data for this example
    data += "_updated"; 
}

void AgentBasedSensor::postData() {
    // Create a document
    bsoncxx::builder::stream::document document{};
    document << "sensor_id" << sensorId << "data" << data;

    try {
        // Use the member variables db and collection
        collection.insert_one(document.view());
        std::cout << "Sensor " << sensorId << " posted data: " << bsoncxx::to_json(document.view()) << std::endl;
    } catch (const mongocxx::exception& e) {
        std::cerr << "Error posting data: " << e.what() << std::endl;
    }
}