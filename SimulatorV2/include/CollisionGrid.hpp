#pragma once

#include <SFML/System/Vector2.hpp>
#include <unordered_map>
#include <vector>
#include "Agent.hpp" // Assuming your Agent class is defined in Agent.hpp
#include "Utilities.hpp"

// GridCell structure for storing agents in each cell
struct GridCell {
    std::vector<Agent*> agents;
    float cellDensity = 0.0f;
    int totalAgents = 0;
};

class Grid {
public:
    Grid(float cellSize, int width, int height); // in cells
    void addAgent(Agent* agent);
    void clear();
    void calculateDensity(); // Calculate agent density in each cell
    void checkCollisions(); // Handle collision checks within the grid
    sf::Vector2i getGridCellIndex(const sf::Vector2f& position); // Function to get grid cell index based on position
    std::unordered_map<sf::Vector2i, GridCell, Vector2iHash> cells; 

    // Accessor function for cells (now returns non-const reference)
    std::unordered_map<sf::Vector2i, GridCell, Vector2iHash>& getCells() {  
        return cells; 
    }
    int width; // Number of cells horizontally
    int height; // Number of cells vertically

private:
    sf::FloatRect detectionArea;
    float cellSize;

    // Helper function to get adjacent cell indices
    std::vector<sf::Vector2i> getAdjacentCellIndices(const sf::Vector2i& cellIndex) const;
};