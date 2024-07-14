#ifndef GRID_HPP
#define GRID_HPP

#include <SFML/System/Vector2.hpp>
#include <unordered_map>
#include <vector>
#include "Agent.hpp" // Assuming your Agent class is defined in Agent.hpp
#include "Utilities.hpp"

// GridCell structure for storing agents in each cell
struct GridCell {
    std::vector<Agent*> agents;
};

class Grid {
public:
    Grid(float cellSize, int width, int height);
    void addAgent(Agent* agent);
    void clear();
    void checkCollisions(); // Handle collision checks within the grid
    sf::Vector2i getGridCellIndex(const sf::Vector2f& position); // Function to get grid cell index based on position

    // Accessor function for cells (now returns non-const reference)
    std::unordered_map<sf::Vector2i, GridCell, Vector2iHash>& getCells() {  
        return cells; 
    }

private:
    float cellSize;
    int width; // Number of cells horizontally
    int height; // Number of cells vertically
    std::unordered_map<sf::Vector2i, GridCell, Vector2iHash> cells; 

    // Helper function to get adjacent cell indices
    std::vector<sf::Vector2i> getAdjacentCellIndices(const sf::Vector2i& cellIndex) const;
};

#endif // GRID_HPP
