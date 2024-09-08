#include "../include/Utilities.hpp"

// Generate a unique identifier for the agent
std::string generateUUID() {

    uuid_t uuid;
    uuid_generate(uuid);
    char uuidStr[37];
    uuid_unparse(uuid, uuidStr);

    return std::string(uuidStr);
}

// Generate a unique identifier for the agent
std::string generateISOTimestamp() {

    auto now = std::chrono::system_clock::now();
    std::time_t time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%FT%TZ");

    return ss.str();
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

// Generate a random velocity vector from a truncated normal distribution
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

// Generate an ISO 8601 timestamp from a start date string and total elapsed time
std::string generateISOTimestamp(sf::Time totalElapsedTime, const std::string& dateTimeString = "") {
    std::tm startTime{}; // Initialize to all zeros
    std::time_t start_time_t = 0;
    bool useCurrentTime = false;

    // Parse the start date string
    if (!dateTimeString.empty()) {
        std::istringstream ss(dateTimeString);
        ss >> std::get_time(&startTime, "%Y-%m-%dT%H:%M:%SZ"); // Parse with Z for UTC

        if (ss.fail()) {
            std::cerr << "Error parsing datetime string from config.yaml: '" << dateTimeString << "'. Using current time instead!" << std::endl;
            useCurrentTime = true;
        } else {
            start_time_t = timegm(&startTime); // Convert to time_t as UTC time
        }
    } else {
        useCurrentTime = true;
    }

    // Get current time if parsing failed or no date string provided
    if (useCurrentTime) {
        auto now = std::chrono::system_clock::now();
        start_time_t = std::chrono::system_clock::to_time_t(now);
    }

    // Convert totalElapsedTime to duration since epoch
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::microseconds(totalElapsedTime.asMicroseconds()));

    // Convert start time to time_point
    auto startTimePoint = std::chrono::system_clock::from_time_t(start_time_t);

    // Add the duration to the start time_point
    auto timestampTp = startTimePoint + duration;

    // Convert the new time_point back to time_t
    std::time_t tt = std::chrono::system_clock::to_time_t(timestampTp);

    // Format into ISO 8601 string
    std::stringstream outputSS;
    std::tm localTime; // Local time structure

    // Get local time with DST information
    localtime_r(&tt, &localTime);

    if (!dateTimeString.empty() && dateTimeString.back() == 'Z') { // Check if original timestamp was in UTC
        outputSS << std::put_time(std::gmtime(&tt), "%FT%TZ"); // UTC timezone
    } else {
        outputSS << std::put_time(&localTime, "%FT%T%z"); // Local timezone with offset (using localTime)
    }

    return outputSS.str();
}

// Convert a string to an sf::Color object
sf::Color stringToColor(std::string colorStr) {

    // Mapping of color names to sf::Color objects
    static const std::unordered_map<std::string, sf::Color> colorMap = {

        {"red", sf::Color::Red},
        {"green", sf::Color::Green},
        {"blue", sf::Color::Blue},
        {"black", sf::Color::Black},
        {"white", sf::Color::White},
        {"yellow", sf::Color::Yellow},
        {"magenta", sf::Color::Magenta},
        {"cyan", sf::Color::Cyan},
        {"pink", sf::Color(255, 192, 203)},
        {"brown", sf::Color(165, 42, 42)},
        {"turquoise", sf::Color(64, 224, 208)},
        {"gray", sf::Color(128, 128, 128)},
        {"purple", sf::Color(128, 0, 128)},
        {"violet", sf::Color(238, 130, 238)},
        {"orange", sf::Color(198, 81, 2)},
        {"indigo", sf::Color(75, 0, 130)},
        {"grey", sf::Color(128, 128, 128)}
    };

    // Convert to lowercase directly for faster comparison
    std::transform(colorStr.begin(), colorStr.end(), colorStr.begin(),
                [](unsigned char c){ return std::tolower(c); }); 

    // Fast lookup using a hash map
    auto it = colorMap.find(colorStr);
    if (it != colorMap.end()) {
        return it->second;
    }

    // Handle hex codes (same as your original implementation)
    if (colorStr.length() == 7 && colorStr[0] == '#') {
        int r, g, b;
        if (sscanf(colorStr.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
            return sf::Color(r, g, b);
        }
    }

    std::cerr << "Warning: Unrecognized color string '" << colorStr << "'. Using black instead." << std::endl;
    return sf::Color::Black;
}