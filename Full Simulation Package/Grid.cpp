#include "Grid.hpp"
#include "CollisionAvoidance.hpp" // Include the new header
#include <vector>

// Constructor
Grid::Grid(float cellSize, int width, int height)
    : cellSize(cellSize), width(width), height(height), cells(0, Vector2iHash{}) {} // Use custom hash here

// Add agent to the grid
void Grid::addAgent(Agent* agent) {
    sf::Vector2i cellIndex = getGridCellIndex(agent->position, cellSize);
    cells[cellIndex].agents.push_back(agent);
}

// Clear the grid
void Grid::clear() {
    cells.clear();
}

// Check collisions within the grid
void Grid::checkCollisions() {
    for (const auto& [cellIndex, cell] : cells) {
        // Check collisions within the same cell
        for (size_t i = 0; i < cell.agents.size(); ++i) {
            for (size_t j = i + 1; j < cell.agents.size(); ++j) {
                if(collisionPossible(*cell.agents[i], *cell.agents[j])) {
                    predictCollisionAgents(*cell.agents[i], *cell.agents[j]);
                    // Handle collision if detected
                    // You might call the agent's handleCollision() function here
                }

            }   
        }

        // Check collisions with agents in adjacent cells
        for (const sf::Vector2i& adjacentIndex : getAdjacentCellIndices(cellIndex)) {
            if (cells.count(adjacentIndex) > 0) { // Check if the adjacent cell exists
                const GridCell& adjacentCell = cells[adjacentIndex];
                for (Agent* agent1 : cell.agents) {
                    for (Agent* agent2 : adjacentCell.agents) {
                        if (collisionPossible(*agent1, *agent2)) {
                            predictCollisionAgents(*agent1, *agent2);
                            // Handle collision if detected
                            // You might call the agent's handleCollision() function here
                        }
                    }
                }
            }
        }
    }
}

// Get cell index based on position
sf::Vector2i getGridCellIndex(const sf::Vector2f& position, float cellSize) {
    return sf::Vector2i(static_cast<int>(position.x / cellSize), 
                        static_cast<int>(position.y / cellSize));
}

// Helper function to get adjacent cell indices (including diagonals)
std::vector<sf::Vector2i> Grid::getAdjacentCellIndices(const sf::Vector2i& cellIndex) const {
    std::vector<sf::Vector2i> adjacentIndices;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue; // Skip the current cell

            int newX = cellIndex.x + dx;
            int newY = cellIndex.y + dy;

            // Check if the adjacent cell is within the grid boundaries
            if (newX >= 0 && newX < width && newY >= 0 && newY < height) {
                adjacentIndices.push_back(sf::Vector2i(newX, newY));
            }
        }
    }
    return adjacentIndices;
}
