#include "Utilities.hpp"
#include <uuid/uuid.h>
#include <random>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

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

sf::Vector2f generateRandomVelocityVector(float mu, float sigma, float min, float max) {
    
    // Generate distribution for angle
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> disAngle(0.0, 2 * M_PI);
    sf::Vector2f velocity;

    // Generate random velocity magnitude
    float velocityMagnitude = generateRandomNumberFromTND(mu, sigma, min, max);
    float angle = disAngle(gen);
    velocity = sf::Vector2f(velocityMagnitude * std::cos(angle), velocityMagnitude * std::sin(angle));
    
    return velocity;
}

// Generate velocity from truncated normal distribution
float generateRandomNumberFromTND(float mean, float stddev, float min, float max) {

    // Generate normal distribution
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::normal_distribution<float> generateNormal(mean, stddev);

    // Generate random number until it falls within the specified range
    float value;
    do {

        value = generateNormal(gen);

    } while (value < min || value > max);

    return value;
}

// Generate an ISO 8601 timestamp from total elapsed time
std::string generateISOTimestamp(sf::Time totalElapsedTime) {

    // Convert totalElapsedTime to seconds
    auto totalSeconds = totalElapsedTime.asSeconds();
    
    // Convert seconds to a time_point since epoch
    auto tp = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(totalSeconds));

    // Get time_t from time_point
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);

    // Format into ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&tt), "%FT%TZ"); // Use gmtime for UTC timezone

    return ss.str();
}

std::string generateTimestamp(sf::Time simulationWallTime) {

    // Convert simulationWallTime (milliseconds) into time_t
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds(simulationWallTime.asMicroseconds()));
    auto tp = std::chrono::system_clock::time_point(duration);

    // Convert to string
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&tt), "%FT%T.%3NZ"); // Include milliseconds

    std::cout << "Timestamp: " << ss.str() << std::endl;

    return ss.str();
}


// std::string generateISOTimestamp(sf::Time totalElapsedTime, const std::string& dateTimeString = "") {
//     std::tm startTime{}; // Initialize to all zeros
//     std::time_t start_time_t = 0;
//     bool useCurrentTime = false;

//     // Parse the start date string
//     if (!dateTimeString.empty()) {
//         std::istringstream ss(dateTimeString);
//         ss >> std::get_time(&startTime, "%Y-%m-%dT%H:%M:%SZ"); // Parse with Z for UTC

//         if (ss.fail()) {
//             std::cerr << "Error parsing datetime string from config.yaml: '" << dateTimeString << "'. Using current time instead!" << std::endl;
//             useCurrentTime = true;
//         } else {
//             start_time_t = timegm(&startTime); // Convert to time_t as UTC time
//         }
//     } else {
//         useCurrentTime = true;
//     }

//     // Get current time if parsing failed or no date string provided
//     if (useCurrentTime) {
//         auto now = std::chrono::system_clock::now();
//         start_time_t = std::chrono::system_clock::to_time_t(now);
//     }

//     // Convert totalElapsedTime to duration since epoch
//     auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::microseconds(totalElapsedTime.asMicroseconds()));

//     // Convert start time to time_point
//     auto startTimePoint = std::chrono::system_clock::from_time_t(start_time_t);

//     // Add the duration to the start time_point
//     auto timestampTp = startTimePoint + duration;

//     // Convert the new time_point back to time_t
//     std::time_t tt = std::chrono::system_clock::to_time_t(timestampTp);

//     // Format into ISO 8601 string
//     std::stringstream outputSS;
//     std::tm localTime; // Local time structure

//     // Get local time with DST information
//     localtime_r(&tt, &localTime);

//     if (!dateTimeString.empty() && dateTimeString.back() == 'Z') { // Check if original timestamp was in UTC
//         outputSS << std::put_time(std::gmtime(&tt), "%FT%TZ"); // UTC timezone
//     } else {
//         outputSS << std::put_time(&localTime, "%FT%T%z"); // Local timezone with offset (using localTime)
//     }

//     return outputSS.str();
// }

// Generate an ISO 8601 timestamp from a start date string and total elapsed time
std::string generateISOTimestamp(sf::Time totalElapsedTime, const std::string& dateTimeString = "") {

    std::chrono::system_clock::time_point timestampTp;
    bool useCurrentTime = dateTimeString.empty(); // Simplified check

    // Parse start date or use current time (if empty)
    if (!useCurrentTime) {
        std::istringstream ss(dateTimeString);
        std::tm startTime{};
        ss >> std::get_time(&startTime, "%Y-%m-%dT%H:%M:%S"); 

        if (ss.fail()) {
            std::cerr << "Error parsing datetime string: '" << dateTimeString << "'. Using current time instead!" << std::endl;
            useCurrentTime = true;
        } else {
            timestampTp = std::chrono::system_clock::from_time_t(std::mktime(&startTime));

            // Handle fractional seconds (if present)
            if (ss.peek() == ':') {
                char dot;
                int milliseconds;
                ss >> dot >> milliseconds;
                timestampTp += std::chrono::milliseconds(milliseconds);
            } 
        }
    }

    if (useCurrentTime) {
        timestampTp = std::chrono::system_clock::now();
    }

    // Calculate new timestamp
    timestampTp += std::chrono::microseconds(totalElapsedTime.asMicroseconds());

    // Format into ISO 8601 string (with milliseconds)
    std::time_t tt = std::chrono::system_clock::to_time_t(timestampTp);
    std::tm* gmtTime = std::gmtime(&tt);
    char buffer[32]; // Enough space for ISO 8601 with milliseconds
    strftime(buffer, sizeof(buffer), "%FT%T", gmtTime);

    // Get milliseconds
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timestampTp.time_since_epoch()) % 1000;

    // Append milliseconds and timezone (optional)
    std::stringstream outputSS;
    outputSS << buffer << ":" << std::setfill('0') << std::setw(3) << milliseconds.count() << "Z"; // Always add milliseconds

    return outputSS.str();
}