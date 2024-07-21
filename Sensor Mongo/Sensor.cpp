#include "Sensor.hpp"
#include <iostream>
#include <uuid/uuid.h>

Sensor::Sensor(std::shared_ptr<mongocxx::client> client)
    : client(std::move(client)), sensorId(generateUUID()) {}

std::string Sensor::generateUUID() {
    // Create a UUID (Universally Unique Identifier)
    uuid_t uuid;
    uuid_generate(uuid);

    // Convert UUID to a readable string
    char uuidString[37]; // 36 characters for UUID + 1 for null terminator
    uuid_unparse(uuid, uuidString);

    return std::string(uuidString);
}