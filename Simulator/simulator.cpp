#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
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
    sf::Vector2f original_velocity; // To store original velocity
    float radius = 5.0f; // Example radius
    sf::Color color = sf::Color::Black; // Start color is black
    sf::Color bufferColor = sf::Color::Green; // Start buffer color is green

    float bufferRadius;
    bool hasCollision;
    bool stopped; // Flag indicating if the agent is stopped
    int stoppedFrameCounter; // Counter to handle stopping duration

    Agent() {
        bufferRadius = 0;
        hasCollision = false;
        stopped = false;
        stoppedFrameCounter = 0;
    }

    void initialize() {
        float velocityMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        bufferRadius = radius * (2 + velocityMagnitude / 2.0f); // Calculate buffer zone radius based on initial velocity
    }

    void updatePosition() {
        position += velocity;
        // Calculate buffer zone radius based on velocity
        //float velocityMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        //bufferRadius = radius * (2 + velocityMagnitude / 2.0f); // Adjust as necessary
    }

    sf::Vector2f getFuturePositionAtTime(float time) const {
        return position + velocity * time;
    }

    sf::CircleShape getBufferZone() const {
        sf::CircleShape bufferZone(bufferRadius);
        bufferZone.setOrigin(bufferRadius, bufferRadius);
        bufferZone.setPosition(position);
        bufferZone.setFillColor(sf::Color::Transparent);
        bufferZone.setOutlineThickness(2.f);
        bufferZone.setOutlineColor(bufferColor);
        return bufferZone;
    }

    void resetCollisionState() {
        bufferColor = sf::Color::Green;
        hasCollision = false;
    }

    void stop() {
        if (!stopped) {
            original_velocity = velocity;
            velocity = sf::Vector2f(0.0f, 0.0f);
            stopped = true;
            stoppedFrameCounter = 0; // Reset the counter when stopping
        }
    }

    bool canResume(const std::vector<Agent>& agents) {
        for (const auto& other : agents) {
            if (&other == this) continue; // Skip self

            // Check if this agent's buffer zone will intersect with any other agent's buffer zone
            float dx = position.x - other.position.x;
            float dy = position.y - other.position.y;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance < bufferRadius + other.bufferRadius) {
                return false;
            }
        }
        return true;
    }

    void resume(const std::vector<Agent>& agents) {
        if (stopped && stoppedFrameCounter >= 20) { // Resume after 20 frames (adjust as needed)
            if (canResume(agents)) {
                velocity = original_velocity;
                stopped = false;
            }
        }
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

// Improved collision prediction using incremental lookahead steps
bool predictCollision(Agent& agent1, Agent& agent2) {
    const float lookaheadStep = 0.5f; // Time step for predictions
    const float maxLookahead = 3.0f; // Maximum lookahead time

    for (float t = 0; t <= maxLookahead; t += lookaheadStep) {
        sf::Vector2f futurePos1 = agent1.getFuturePositionAtTime(t);
        sf::Vector2f futurePos2 = agent2.getFuturePositionAtTime(t);

        // Check if the future positions (including buffer radius) intersect
        float dx = futurePos1.x - futurePos2.x;
        float dy = futurePos1.y - futurePos2.y;
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < agent1.bufferRadius + agent2.bufferRadius) {
            agent1.bufferColor = sf::Color::Red;
            agent2.bufferColor = sf::Color::Red;
            agent1.hasCollision = true;
            agent2.hasCollision = true;

            // Implement collision avoidance: stop the slower agent
            float speed1 = std::sqrt(agent1.velocity.x * agent1.velocity.x + agent1.velocity.y * agent1.velocity.y);
            float speed2 = std::sqrt(agent2.velocity.x * agent2.velocity.x + agent2.velocity.y * agent2.velocity.y);

            if (speed1 < speed2) {
                agent1.stop(); // Slower agent stops
            } else {
                agent2.stop(); // Slower agent stops
            }

            return true; // Collision detected
        }
    }

    return false; // No collision detected in the lookahead time frame
}

void resetSimulation(std::vector<Agent>& agents, std::mt19937& gen, std::uniform_real_distribution<>& dis) {
    // Reset each agent's position and velocity
    for (auto& agent : agents) {
        agent.position = sf::Vector2f(rand() % 3440, rand() % 1440);
        agent.initial_position = agent.position;
        agent.velocity = sf::Vector2f(dis(gen), dis(gen)); // Random velocity
        agent.original_velocity = agent.velocity; // Store initial velocity
        agent.color = sf::Color::Black; // Reset color
        agent.bufferColor = sf::Color::Green; // Reset buffer color
        agent.hasCollision = false;
        agent.stopped = false; // Reset stopped state
        agent.stoppedFrameCounter = 0; // Reset the counter
        agent.initialize(); // Calculate the buffer zone based on the initial velocity
    }
}

int main() {
    // Window setup
    sf::RenderWindow window(sf::VideoMode(3440, 1440), "Road User Simulation");
    window.setFramerateLimit(60); // Cap FPS for smoother visuals

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-4.0, 4.0);

    // Agent initialization (example)
    std::vector<Agent> agents;
    for (int i = 0; i < 1000; ++i) {
        Agent agent;
        agent.position = sf::Vector2f(rand() % 3440, rand() % 1440);
        agent.initial_position = agent.position;
        agent.velocity = sf::Vector2f(dis(gen), dis(gen)); // Random velocity
        agent.original_velocity = agent.velocity; // Store initial velocity
        agent.initialize(); // Calculate the buffer zone based on initial velocity
        agents.push_back(agent);
    }

    // Grid parameters
    const float cellSize = 100.0f; // Size of each cell in the grid
    const int numCellsX = window.getSize().x / cellSize; // Number of cells horizontally
    const int numCellsY = window.getSize().y / cellSize; // Number of cells vertically
    std::unordered_map<sf::Vector2i, GridCell> grid;

    // Initialize clock
    sf::Clock clock;

    // Collision counters
    size_t gridBasedCollisionCount = 0;
    size_t globalCollisionCount = 0;

    const int maxFrames = 10000; // Number of frames the simulation should run
    int frameCount = 0;

    // Load font
    sf::Font font;
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        std::cerr << "Error loading font\n";
        return -1;
    }

    // Create text to display frame count
    sf::Text frameText;
    frameText.setFont(font);
    frameText.setCharacterSize(24);
    frameText.setFillColor(sf::Color::Black);

    // Create text to display frame rate
    sf::Text frameRateText;
    frameRateText.setFont(font);
    frameRateText.setCharacterSize(24);
    frameRateText.setFillColor(sf::Color::Black);

    // Initialize frame rate buffer
    std::vector<float> frameRates;
    const size_t frameRateBufferSize = 100; // Adjust the buffer size for the moving average
    float cumulativeSum = 0.0f;

    // Create pause button
    sf::RectangleShape pauseButton(sf::Vector2f(100, 50));
    pauseButton.setFillColor(sf::Color::Green);
    pauseButton.setPosition(window.getSize().x - 110, window.getSize().y - 160); // Adjusted position
    sf::Text pauseButtonText;
    pauseButtonText.setFont(font);
    pauseButtonText.setString("Pause");
    pauseButtonText.setCharacterSize(20);
    pauseButtonText.setFillColor(sf::Color::Black);
    sf::FloatRect buttonTextRect = pauseButtonText.getLocalBounds();
    pauseButtonText.setOrigin(buttonTextRect.width / 2, buttonTextRect.height / 2);
    pauseButtonText.setPosition(pauseButton.getPosition().x + pauseButton.getSize().x / 2,
                                pauseButton.getPosition().y + pauseButton.getSize().y / 2);

    // Create reset button
    sf::RectangleShape resetButton(sf::Vector2f(100, 50));
    resetButton.setFillColor(sf::Color::Blue);
    resetButton.setPosition(window.getSize().x - 110, window.getSize().y - 220); // Adjusted position
    sf::Text resetButtonText;
    resetButtonText.setFont(font);
    resetButtonText.setString("Reset");
    resetButtonText.setCharacterSize(20);
    resetButtonText.setFillColor(sf::Color::Black);
    sf::FloatRect resetButtonTextRect = resetButtonText.getLocalBounds();
    resetButtonText.setOrigin(resetButtonTextRect.width / 2, resetButtonTextRect.height / 2);
    resetButtonText.setPosition(resetButton.getPosition().x + resetButton.getSize().x / 2,
                                resetButton.getPosition().y + resetButton.getSize().y / 2);

    bool isPaused = false;
    sf::Time elapsedTime;

    while (window.isOpen() && frameCount < maxFrames) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2f mousePos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));

                    // Check if pause button is clicked
                    if (pauseButton.getGlobalBounds().contains(mousePos)) {
                        isPaused = !isPaused;
                        if (isPaused) {
                            pauseButton.setFillColor(sf::Color::Red);
                            pauseButtonText.setString("Resume");
                        } else {
                            pauseButton.setFillColor(sf::Color::Green);
                            pauseButtonText.setString("Pause");
                        }
                    }

                    // Check if reset button is clicked
                    if (resetButton.getGlobalBounds().contains(mousePos)) {
                        resetSimulation(agents, gen, dis);
                        if (isPaused) {
                            isPaused = false;
                            pauseButton.setFillColor(sf::Color::Green);
                            pauseButtonText.setString("Pause");
                        }
                        frameCount = 0; // Reset frame count
                        frameRates.clear(); // Reset frame rate buffer
                        cumulativeSum = 0.0f;
                    }
                }
            }
        }

        if (!isPaused) {
            elapsedTime = clock.restart();

            // Update agent positions
            grid.clear(); // Clear the grid before updating positions
            for (auto& agent : agents) {
                agent.updatePosition();
                agent.resetCollisionState(); // Reset collision state at the start of each frame for each agent
                sf::Vector2i cellIndex = getGridCellIndex(agent.position, cellSize);
                grid[cellIndex].agents.push_back(&agent);

                // Resume agents if they are stopped
                if (agent.stopped) {
                    ++agent.stoppedFrameCounter;
                    agent.resume(agents); // The agent resumes after a timeout
                }
            }

            // Collision detection using grid (same as before)
            for (const auto& [cellIndex, cell] : grid) {
                // Check collisions within the same cell
                for (size_t i = 0; i < cell.agents.size(); ++i) {
                    for (size_t j = i + 1; j < cell.agents.size(); ++j) {
                        gridBasedCollisionCount++;
                        globalCollisionCount++;

                        Agent& agent1 = *cell.agents[i];
                        Agent& agent2 = *cell.agents[j];

                        predictCollision(agent1, agent2);
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
                                gridBasedCollisionCount++;
                                globalCollisionCount++;

                                predictCollision(*agent1, *agent2);
                            }
                        }
                    }
                }
            }

            // For global collision comparison, calculate once per frame outside grid-based collision detection
            globalCollisionCount += agents.size() * (agents.size() - 1) / 2;

            // Increment frame count
            frameCount++;
        } else {
            elapsedTime = clock.restart(); // Prevent deltaTime accumulation while paused
        }

        float frameRate = 1.0f / elapsedTime.asSeconds();

        // Update frame rate buffer using cumulative sum
        if (frameRates.size() == frameRateBufferSize) {
            cumulativeSum -= frameRates[0]; // Subtract the oldest frame rate from the sum
            frameRates.erase(frameRates.begin());
        }

        frameRates.push_back(frameRate);
        cumulativeSum += frameRate; // Add the new frame rate to the sum

        // Calculate moving average of frame rate
        float movingAverageFrameRate = cumulativeSum / frameRates.size();

        // Update frame count text
        frameText.setString("Frame " + std::to_string(frameCount) + "/" + std::to_string(maxFrames));
        sf::FloatRect textRect = frameText.getLocalBounds();
        frameText.setOrigin(textRect.width, 0); // Right-align the text
        frameText.setPosition(window.getSize().x - 10, 10); // Padding of 10 pixels from top-right corner

        // Update frame rate text
        frameRateText.setString("FPS: " + std::to_string(static_cast<int>(movingAverageFrameRate)));
        textRect = frameRateText.getLocalBounds();
        frameRateText.setOrigin(textRect.width, 0); // Right-align the text
        frameRateText.setPosition(window.getSize().x - 10, 40); // Padding of 10 pixels below frame count

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

            // Draw the buffer zone
            sf::CircleShape bufferZone = agent.getBufferZone();
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

        // Draw frame count text
        window.draw(frameText);

        // Draw frame rate text
        window.draw(frameRateText);

        // Draw pause button
        window.draw(pauseButton);
        window.draw(pauseButtonText);

        // Draw reset button
        window.draw(resetButton);
        window.draw(resetButtonText);

        window.display();
    }

    // Print collision calculation counts
    std::cout << "Grid-based collision calculations: " << gridBasedCollisionCount << std::endl;
    std::cout << "Global collision calculations (estimated): " << globalCollisionCount << std::endl;

    return 0;
}