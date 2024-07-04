#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <functional>
#include <random>
#include <iostream>
#include <unordered_map>

// Custom hash function for sf::Vector2i
namespace std {
    template <> struct hash<sf::Vector2i> {
        std::size_t operator()(const sf::Vector2i& v) const noexcept {
            return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
        }
    };
}

// Simple Agent class
class Agent {
public:
    sf::Vector2f position;
    sf::Vector2f initial_position;
    sf::Vector2f velocity;
    float radius = 5.0f; // Example radius
    sf::Color color = sf::Color::Red; // Example color
    sf::Color originalColor = color;

    void updatePosition() {
        position += velocity;
    }

    sf::CircleShape getFuturePosition(float deltaTime) const {
        sf::Vector2f futurePos = position + velocity * deltaTime;
        sf::CircleShape futureShape(radius);
        futureShape.setOrigin(radius, radius);
        futureShape.setPosition(futurePos);
        return futureShape;
    }
};

// GridCell structure for storing agents in each cell
struct GridCell {
    std::vector<Agent*> agents;
};

// Function to get grid cell index based on position
sf::Vector2i getGridCellIndex(const sf::Vector2f& position, float cellSize) {
    return sf::Vector2i(static_cast<int>(position.x / cellSize), static_cast<int>(position.y / cellSize));
}

int main() {
    // Window setup
    sf::RenderWindow window(sf::VideoMode(3440, 1440), "Road User Simulation");
    window.setFramerateLimit(30); // Cap FPS for smoother visuals

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-4.0, 4.0);

    // Agent initialization (example)
    std::vector<Agent> agents;
    for (int i = 0; i < 500; ++i) {
        Agent agent;
        agent.position = sf::Vector2f(rand() % 3440, rand() % 1440);
        agent.initial_position = agent.position;
        agent.velocity = sf::Vector2f(dis(gen), dis(gen)); // Random velocity
        agents.push_back(agent);
    }

    // Grid parameters
    const float cellSize = 100.0f; // Size of each cell in the grid
    const int numCellsX = window.getSize().x / cellSize; // Number of cells horizontally
    const int numCellsY = window.getSize().y / cellSize; // Number of cells vertically
    std::unordered_map<sf::Vector2i, GridCell> grid;

    // Initialize clock
    sf::Clock clock;
    float deltaTime = 0.f;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        deltaTime = clock.restart().asSeconds();

        // Update agent positions
        grid.clear(); // Clear the grid before updating positions
        for (auto& agent : agents) {
            agent.updatePosition();
            sf::Vector2i cellIndex = getGridCellIndex(agent.position, cellSize);
            grid[cellIndex].agents.push_back(&agent);
        }

        // Collision detection using grid
        for (const auto& [cellIndex, cell] : grid) {
            // Check collisions within the same cell
            for (size_t i = 0; i < cell.agents.size(); ++i) {
                for (size_t j = i + 1; j < cell.agents.size(); ++j) {
                    Agent& agent1 = *cell.agents[i];
                    Agent& agent2 = *cell.agents[j];

                    // Calculate lookahead distance (you can adjust this formula)
                    float lookaheadDistance1 = agent1.radius + agent1.velocity.x * deltaTime;
                    float lookaheadDistance2 = agent2.radius + agent2.velocity.x * deltaTime;

                    // Get future positions
                    sf::CircleShape futurePos1 = agent1.getFuturePosition(deltaTime);
                    sf::CircleShape futurePos2 = agent2.getFuturePosition(deltaTime);

                    // Calculate buffer zone radii based on velocity
                    float velocityMagnitude1 = std::sqrt(agent1.velocity.x * agent1.velocity.x + agent1.velocity.y * agent1.velocity.y);
                    float bufferRadius1 = agent1.radius * (2 + velocityMagnitude1 / 2.0f);

                    float velocityMagnitude2 = std::sqrt(agent2.velocity.x * agent2.velocity.x + agent2.velocity.y * agent2.velocity.y);
                    float bufferRadius2 = agent2.radius * (2 + velocityMagnitude2 / 2.0f);

                    // Check if the future buffer zones of the agents intersect
                    float dx = futurePos1.getPosition().x - futurePos2.getPosition().x;
                    float dy = futurePos1.getPosition().y - futurePos2.getPosition().y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance < bufferRadius1 + bufferRadius2) {
                        // Collision detected!
                        agent1.color = sf::Color::Yellow; // Change color to indicate collision (optional)
                        agent2.color = sf::Color::Yellow;
                    }
                }
            }

            // Check collisions with adjacent cells
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) continue; // Skip the current cell

                    sf::Vector2i adjacentCellIndex = cellIndex + sf::Vector2i(dx, dy);
                    if (grid.find(adjacentCellIndex) == grid.end()) continue; // No agents in the adjacent cell

                    const auto& adjacentCell = grid[adjacentCellIndex];
                    for (Agent* agent1 : cell.agents) {
                        for (Agent* agent2 : adjacentCell.agents) {
                            // Calculate lookahead distance (you can adjust this formula)
                            float lookaheadDistance1 = agent1->radius + agent1->velocity.x * deltaTime;
                            float lookaheadDistance2 = agent2->radius + agent2->velocity.x * deltaTime;

                            // Get future positions
                            sf::CircleShape futurePos1 = agent1->getFuturePosition(deltaTime);
                            sf::CircleShape futurePos2 = agent2->getFuturePosition(deltaTime);

                            // Calculate buffer zone radii based on velocity
                            float velocityMagnitude1 = std::sqrt(agent1->velocity.x * agent1->velocity.x + agent1->velocity.y * agent1->velocity.y);
                            float bufferRadius1 = agent1->radius * (2 + velocityMagnitude1 / 2.0f);

                            float velocityMagnitude2 = std::sqrt(agent2->velocity.x * agent2->velocity.x + agent2->velocity.y * agent2->velocity.y);
                            float bufferRadius2 = agent2->radius * (2 + velocityMagnitude2 / 2.0f);

                            // Check if the future buffer zones of the agents intersect
                            float dx = futurePos1.getPosition().x - futurePos2.getPosition().x;
                            float dy = futurePos1.getPosition().y - futurePos2.getPosition().y;
                            float distance = std::sqrt(dx * dx + dy * dy);
                            if (distance < bufferRadius1 + bufferRadius2) {
                                // Collision detected!
                                agent1->color = sf::Color::Yellow; // Change color to indicate collision (optional)
                                agent2->color = sf::Color::Yellow;
                            }
                        }
                    }
                }
            }
        }

        // Rendering
        window.clear(sf::Color::White); // Clear the window with a white background
        
        // Draw the grid
        for (int x = 0; x <= numCellsX; ++x) {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(x * cellSize, 0), sf::Color::Black),
                sf::Vertex(sf::Vector2f(x * cellSize, window.getSize().y), sf::Color::Black)
            };
            window.draw(line, 2, sf::Lines);
        }
        for (int y = 0; y <= numCellsY; ++y) {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(0, y * cellSize), sf::Color::Black),
                sf::Vertex(sf::Vector2f(window.getSize().x, y * cellSize), sf::Color::Black)
            };
            window.draw(line, 2, sf::Lines);
        }

        for (const auto& agent : agents) {
            // Draw the agent as a circle
            sf::CircleShape agentShape(agent.radius);
            agentShape.setFillColor(agent.color);
            agentShape.setOrigin(agent.radius, agent.radius);
            agentShape.setPosition(agent.position);
            window.draw(agentShape);

            // Calculate buffer zone radius based on velocity
            float velocityMagnitude = std::sqrt(agent.velocity.x * agent.velocity.x + 
                                                agent.velocity.y * agent.velocity.y);
            float bufferRadius = agent.radius * (2 + velocityMagnitude / 2.0f); // Scale factor of 10 (adjust as needed)

            // Draw the buffer zone
            sf::CircleShape bufferZone(bufferRadius); // Double the radius
            bufferZone.setOrigin(bufferRadius, bufferRadius);
            bufferZone.setPosition(agent.position);
            bufferZone.setFillColor(sf::Color::Transparent); // No fill
            bufferZone.setOutlineThickness(2.f);            // Outline thickness
            bufferZone.setOutlineColor(sf::Color::Green);  // Green outline
            window.draw(bufferZone);

            // Draw the arrow (a triangle)
            sf::Vector2f direction = agent.velocity;
            float arrowLength = agent.radius * 0.8f; 
            float arrowAngle = std::atan2(direction.y, direction.x);

            // Normalize the direction vector to have a length of 1 (unit vector)
            sf::Vector2f norm_vector = direction / std::sqrt(direction.x * direction.x + direction.y * direction.y);

            sf::ConvexShape arrow(3);
            arrow.setPoint(0, sf::Vector2f(0, 0)); // Tip of the arrow
            arrow.setPoint(1, sf::Vector2f(-arrowLength, arrowLength / 2));
            arrow.setPoint(2, sf::Vector2f(-arrowLength, -arrowLength / 2));
            arrow.setFillColor(sf::Color::Black);

            // Draw trajectory
            sf::Vertex trajectory[] =
            {
                sf::Vertex(agent.initial_position, sf::Color::Red),
                sf::Vertex(agent.position, sf::Color::Red)
            };

            // Line (arrow body) - Offset the start by the agent's radius
            sf::Vertex line[] =
            {
                sf::Vertex(agent.position + norm_vector * agent.radius, sf::Color::Black),  // Offset start point
                sf::Vertex(agent.position + norm_vector * agent.radius + direction * (arrowLength / 2.0f), sf::Color::Black) // Ending point (base of arrowhead)
            };

            // Set the origin of the arrowhead to the base of the triangle
            arrow.setOrigin(-arrowLength, 0); // Origin at the base of the arrowhead
            arrow.setPosition(line[1].position); // Position the arrowhead at the end of the line
            arrow.setRotation(arrowAngle * 180.0f / M_PI);
            window.draw(arrow);
            window.draw(trajectory, 2, sf::Lines);
            window.draw(line, 2, sf::Lines); 
        }

        window.display();
    }

    return 0;
}