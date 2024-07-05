#include "Grid.h"

// Function definition (implementation)
sf::Vector2i getGridCellIndex(const sf::Vector2f& position, float cellSize) {
    return sf::Vector2i(static_cast<int>(position.x / cellSize), static_cast<int>(position.y / cellSize));
}
