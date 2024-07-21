#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

class Sensor {
public:
    Sensor(std::shared_ptr<mongocxx::client> client);
    virtual ~Sensor() = default;
    virtual void getData() = 0;
    virtual void postData() = 0;
    std::string generateUUID();
protected:
    std::string sensorId;
    std::shared_ptr<mongocxx::client> client;
};

#endif // SENSOR_HPP
