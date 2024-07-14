#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <SFML/System/Vector2.hpp>
#include <functional>

// Custom hash function for sf::Vector2i
struct Vector2iHash {
    std::size_t operator()(const sf::Vector2i& v) const {
        std::hash<int> hasher;
        return hasher(v.x) ^ (hasher(v.y) << 1); // Combine hash values of x and y
    }
};

#endif // UTILITIES_HPP
