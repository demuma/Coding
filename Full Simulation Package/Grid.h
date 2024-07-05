#ifndef GRID_H
#define GRID_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <unordered_map>
#include "Agent.h"

// GridCell structure for storing agents in each cell
struct GridCell {
    std::vector<Agent*> agents;
};

// Function declaration
sf::Vector2i getGridCellIndex(const sf::Vector2f& position, float cellSize);

#endif // GRID_H
