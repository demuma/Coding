// Obstacle.hpp
#ifndef OBSTACLE_HPP
#define OBSTACLE_HPP

#include <SFML/Graphics.hpp>

class Obstacle {
public:
    Obstacle(sf::FloatRect bounds, sf::Color color = sf::Color::Black);

    sf::FloatRect getBounds() const { return bounds; }
    sf::Color getColor() const { return color; }

private:
    sf::FloatRect bounds;
    sf::Color color;
};

#endif // OBSTACLE_HPP