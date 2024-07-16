#include "Utilities.hpp"
#include <uuid/uuid.h>

// Generate a unique identifier for the agent
std::string generateUUID() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuidStr[37];
    uuid_unparse(uuid, uuidStr);

    return std::string(uuidStr);
}

std::string generateISOTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%FT%TZ");
    return ss.str();
}
