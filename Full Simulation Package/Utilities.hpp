#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <SFML/System/Vector2.hpp>
#include <functional>
#include <string>
#include <uuid/uuid.h>
#include <chrono>
#include <iomanip>
#include <sstream>

// Function declarations
std::string generateUUID();
std::string generateISOTimestamp();

struct Vector2iHash {
    std::size_t operator()(const sf::Vector2i& v) const {
        std::hash<int> hasher;
        return hasher(v.x) ^ (hasher(v.y) << 1); // Combine hash values of x and y
    }
};

#endif // UTILITIES_HPP