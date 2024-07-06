#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <random>
#include <iostream>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <filesystem>

#include "Agent.hpp"
#include "Grid.hpp"
#include "CollisionPrediction.hpp"

// Custom hash function for sf::Vector2i
namespace std {
    template <> struct hash<sf::Vector2i> {
        std::size_t operator()(const sf::Vector2i& v) const noexcept {
            // Combine the hash values of x and y components
            return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1); 
        }
    };
}

// Function to convert a string to sf::Color (case-insensitive)
sf::Color stringToColor(const std::string& colorStr) {
    std::string lowerColorStr = colorStr;
    std::transform(lowerColorStr.begin(), lowerColorStr.end(), lowerColorStr.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lowerColorStr == "red") return sf::Color::Red;
    if (lowerColorStr == "green") return sf::Color::Green;
    if (lowerColorStr == "blue") return sf::Color::Blue;
    if (lowerColorStr == "black") return sf::Color::Black;
    if (lowerColorStr == "white") return sf::Color::White;
    if (lowerColorStr == "yellow") return sf::Color::Yellow;
    if (lowerColorStr == "magenta") return sf::Color::Magenta;
    if (lowerColorStr == "cyan") return sf::Color::Cyan;
    if (lowerColorStr == "purple") return sf::Color::Magenta;  // Using Magenta for Purple
    if (lowerColorStr == "orange") return sf::Color(255, 165, 0); // Orange

    if (colorStr.length() == 7 && colorStr[0] == '#') {
        int r, g, b;
        if (sscanf(colorStr.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
            return sf::Color(r, g, b);
        }
    }

    std::cerr << "Warning: Unrecognized color string '" << colorStr << "'. Using black instead." << std::endl;
    return sf::Color::Black;
}

// Function to reset the simulation
void resetSimulation(std::vector<Agent>& agents, std::mt19937& gen, std::uniform_real_distribution<>& dis, const YAML::Node& config) {
    int windowWidth = config["display"]["width"].as<int>();
    int windowHeight = config["display"]["height"].as<int>();
    
    // Reset each agent's position and velocity
    for (auto& agent : agents) {
        //agent.position = sf::Vector2f(dis(gen) * windowWidth / 2, dis(gen) * windowHeight) / 2;
        agent.position = sf::Vector2f(rand() % windowWidth, rand() % windowHeight);
        agent.initial_position = agent.position;

        // // Iterate over each agent type in the configuration
        // for (const auto& agentType : config["agents"]["road_user_taxonomy"]) {
        //     // Check if this is the right type for the current agent
        //     if (agentType["type"].as<std::string>() == agent.initial_color_str) {
        //         // Extract the properties for this agent type
        //         float minVelocity = agentType["min_velocity"].as<float>();
        //         float maxVelocity = agentType["max_velocity"].as<float>();

        //         // Set the agent's velocity based on the config values
        //         agent.velocity = sf::Vector2f(
        //             dis(gen) * (maxVelocity - minVelocity) + minVelocity,
        //             dis(gen) * (maxVelocity - minVelocity) + minVelocity
        //         );

        //         break; // Found the correct type, stop searching
        //     }
        // }

        agent.velocity = agent.original_velocity;
        agent.color = agent.initial_color;
        agent.bufferColor = sf::Color::Green;
        agent.hasCollision = false;
        agent.stopped = false;
        agent.stoppedFrameCounter = 0;
        agent.initialize();
    }
}

int main() {

    // Configuration Loading
    YAML::Node config;
    try {
        config = YAML::LoadFile("../config.yaml"); 
    } catch (const YAML::Exception& e) {
        std::cerr << "Error loading config file: " << e.what() << std::endl;
        return 1;
    }

    // Update parameters based on configuration
    int windowWidth = config["display"]["width"].as<int>();
    int windowHeight = config["display"]["height"].as<int>();
    float cellSize = config["grid"]["cell_size"].as<float>();
    int numCellsX = windowWidth / cellSize;
    int numCellsY = windowHeight / cellSize;
    bool showTrajectories = config["grid"]["show_trajectories"].as<bool>();
    int numAgents = config["agents"]["num_agents"].as<int>();

    // Load simulation parameters
    float durationSeconds = config["simulation"]["duration_seconds"].as<float>();
    int fps = config["simulation"]["fps"].as<int>();
    int maxFrames = config["simulation"]["maximum_frames"].as<int>();

    std::chrono::duration<double> collisionTime(0.0);

    // Window setup
    sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), "Road User Simulation");
    window.setFramerateLimit(fps);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    // Agent initialization using configuration
    std::vector<Agent> agents;
    for (const auto& agentType : config["agents"]["road_user_taxonomy"]) {
        int numTypeAgents = numAgents * agentType["proportion"].as<float>();
        sf::Color agentColor = stringToColor(agentType["color"].as<std::string>());
        // Error handling if the color is invalid
        if (agentColor == sf::Color::Black) { 
            std::cerr << "Invalid color string in config file for agent type: " << agentType["type"].as<std::string>() << std::endl;
            return 1;
        }
        for (int i = 0; i < numTypeAgents; ++i) {
            Agent agent;
            agent.position = sf::Vector2f(dis(gen) * (windowWidth / 2) + (windowWidth / 2), // Shift to window center
                                     dis(gen) * (windowHeight / 2) + (windowHeight / 2)); // Adjusted for half the window size
            agent.position = sf::Vector2f((rand() % windowWidth) + 1, (rand() % windowHeight) + 1);
            agent.initial_position = agent.position;
            float minVelocity = agentType["min_velocity"].as<float>();
            float maxVelocity = agentType["max_velocity"].as<float>();
            //agent.velocity = sf::Vector2f(dis(gen) * (maxVelocity - minVelocity) + minVelocity,
                                        //dis(gen) * (maxVelocity - minVelocity) + minVelocity);
            agent.velocity = sf::Vector2f(dis(gen) * maxVelocity, dis(gen) * maxVelocity); // Random velocity         
            if(std::abs(agent.velocity.x) < minVelocity) {
                if(agent.velocity.x < -minVelocity) {
                    agent.velocity.x = -minVelocity;
                } else {
                    agent.velocity.x = minVelocity;
                }
            }
            if(std::abs(agent.velocity.y) < minVelocity) {
                if(agent.velocity.y < -minVelocity) {
                    agent.velocity.y = -minVelocity;
                } else {
                    agent.velocity.y = minVelocity;
                }
            }
            agent.original_velocity = agent.velocity;
            agent.radius = agentType["radius"].as<float>();
            agent.color = agentColor;
            agent.initial_color = agentColor;
            agent.initialize();
            agents.push_back(agent);
        }
    }
    
    // Grid parameters
    std::unordered_map<sf::Vector2i, GridCell> grid;

    // Initialize clock
    sf::Clock clock;

    // Collision counters
    size_t gridBasedCollisionCount = 0;
    size_t globalCollisionCount = 0;

    // Load font
    sf::Font font;
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) { // Update with correct path if needed
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
    const size_t frameRateBufferSize = 100; 
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

    // Create reset button (with updated position)
    sf::RectangleShape resetButton(sf::Vector2f(100, 50));
    resetButton.setFillColor(sf::Color::Blue);
    resetButton.setPosition(window.getSize().x - 110, window.getSize().y - 220); // Adjusted
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

    // Max time of simulation
    sf::Time maxTime = sf::seconds(durationSeconds);

    int frameCount = 0; // Initialize frame count here
    sf::Time totalElapsedTime; // Track total elapsed time

    while (window.isOpen() && (totalElapsedTime < maxTime || maxFrames == 0)) { // Use maxTime or frameCount

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2f mousePos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
                    if (pauseButton.getGlobalBounds().contains(mousePos)) {
                        isPaused = !isPaused;
                        if (isPaused) {
                            pauseButton.setFillColor(sf::Color::Red);
                            pauseButtonText.setString("Resume");
                        } else {
                            pauseButton.setFillColor(sf::Color::Green);
                            pauseButtonText.setString("Pause");
                        }
                    } else if (resetButton.getGlobalBounds().contains(mousePos)) {
                        resetSimulation(agents, gen, dis, config);
                        if (isPaused) {
                            isPaused = false;
                            pauseButton.setFillColor(sf::Color::Green);
                            pauseButtonText.setString("Pause");
                        }
                        frameCount = 0; 
                        frameRates.clear();
                        cumulativeSum = 0.0f;
                        totalElapsedTime = sf::seconds(0); // Reset the totalElapsedTime
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
                    agent.resume(agents);
                }
            }

            // Collision detection using grid with OpenMP parallelization
            auto startTime = std::chrono::high_resolution_clock::now();
            for (const auto& [cellIndex, cell] : grid) {
                // Check collisions within the same cell
                for (size_t i = 0; i < cell.agents.size(); ++i) {
                    for (size_t j = i + 1; j < cell.agents.size(); ++j) {
                        gridBasedCollisionCount++;
                        globalCollisionCount++;

                        Agent& agent1 = *cell.agents[i];
                        Agent& agent2 = *cell.agents[j];

                        {
                            predictCollision(agent1, agent2);
                        }
                    }
                }

                // Check collisions with adjacent cells (parallelized)
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx == 0 && dy == 0) continue;

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

            auto endTime = std::chrono::high_resolution_clock::now();
            collisionTime += endTime - startTime;

            std::cout << "Collision detection time: " << collisionTime.count() * 1000 << " milliseconds" << std::endl;
            collisionTime = std::chrono::duration<double>(0.0);

            // For global collision comparison, calculate once per frame outside grid-based collision detection
            globalCollisionCount += agents.size() * (agents.size() - 1) / 2;

            // Increment frame count
            frameCount++;
            // Calculate total elapsed time
            totalElapsedTime += elapsedTime;
        }

        float frameRate = 1.0f / elapsedTime.asSeconds();

        // Update frame rate buffer using cumulative sum
        if (frameRates.size() == frameRateBufferSize) {
            cumulativeSum -= frameRates[0];
            frameRates.erase(frameRates.begin());
        }

        frameRates.push_back(frameRate);
        cumulativeSum += frameRate;

        // Calculate moving average of frame rate
        float movingAverageFrameRate = cumulativeSum / frameRates.size();

        // Update frame count text
        frameText.setString("Frame " + std::to_string(frameCount) + "/" + (maxFrames > 0 ? std::to_string(maxFrames) : "∞"));
        sf::FloatRect textRect = frameText.getLocalBounds();
        frameText.setOrigin(textRect.width, 0);
        frameText.setPosition(window.getSize().x - 10, 10); 

        // Update frame rate text
        frameRateText.setString("FPS: " + std::to_string(static_cast<int>(movingAverageFrameRate)));
        textRect = frameRateText.getLocalBounds();
        frameRateText.setOrigin(textRect.width, 0);
        frameRateText.setPosition(window.getSize().x - 10, 40);

        // Rendering
        window.clear(sf::Color::White);

        // Draw the grid (if enabled)
        if (config["grid"]["show_grid"].as<bool>()) {
            for (int x = 0; x <= numCellsX; ++x) {
                sf::Vertex line[] = {
                    sf::Vertex(sf::Vector2f(x * cellSize, 0), sf::Color(220, 220, 220)), // Light gray for grid
                    sf::Vertex(sf::Vector2f(x * cellSize, window.getSize().y), sf::Color(220, 220, 220))
                };
                window.draw(line, 2, sf::Lines);
            }
            for (int y = 0; y <= numCellsY; ++y) {
                sf::Vertex line[] = {
                    sf::Vertex(sf::Vector2f(0, y * cellSize), sf::Color(220, 220, 220)),
                    sf::Vertex(sf::Vector2f(window.getSize().x, y * cellSize), sf::Color(220, 220, 220))
                };
                window.draw(line, 2, sf::Lines);
            }
       }

        for (const auto& agent : agents) {
            // Draw the agent as a circle
            sf::CircleShape agentShape(agent.radius);
            agentShape.setFillColor(agent.color);
            agentShape.setOrigin(agentShape.getRadius(), agentShape.getRadius());
            //agentShape.setOrigin(agent.radius, agent.radius);
            agentShape.setPosition(agent.position);
            window.draw(agentShape);

            // Draw the buffer zone
            sf::CircleShape bufferZone = agent.getBufferZone();
            window.draw(bufferZone);

            // Draw the arrow (a triangle) representing the velocity vector
            sf::Vector2f direction = agent.velocity;
            float arrowLength = agent.radius * 0.8f;
            float arrowAngle = std::atan2(direction.y, direction.x);

            // Normalize the direction vector for consistent arrow length
            sf::Vector2f normalizedDirection = direction / std::sqrt(direction.x * direction.x + direction.y * direction.y);

            sf::ConvexShape arrow(3);
            arrow.setPoint(0, sf::Vector2f(0, 0)); // Tip of the arrow
            arrow.setPoint(1, sf::Vector2f(-arrowLength, arrowLength / 2));
            arrow.setPoint(2, sf::Vector2f(-arrowLength, -arrowLength / 2));
            arrow.setFillColor(sf::Color::Black);

            // Line (arrow body) - Offset the start by the agent's radius
            sf::Vertex line[] =
            {
                sf::Vertex(agent.position + normalizedDirection * agent.radius, sf::Color::Black),  // Offset start point
                sf::Vertex(agent.position + normalizedDirection * agent.radius + direction * (arrowLength / 2.0f), sf::Color::Black) // Ending point (base of arrowhead)
            };

            // Set the origin of the arrowhead to the base of the triangle
            arrow.setOrigin(-arrowLength, 0); 
            //arrow.setPosition(agent.position + normalizedDirection * (agentShape.getRadius() + 2.f)); // Position at the edge of the agent
            arrow.setPosition(line[1].position); // Position the arrowhead at the end of the line
            arrow.setRotation(arrowAngle * 180.0f / M_PI); // Convert radians to degrees for SFML
            window.draw(arrow);
            window.draw(line, 2, sf::Lines);

            // Draw trajectory (if enabled in config)
            if (showTrajectories) {
                sf::Vertex trajectory[] =
                {
                    sf::Vertex(agent.initial_position, agent.initial_color),
                    sf::Vertex(agent.position, agent.initial_color)
                };
                window.draw(trajectory, 2, sf::Lines);
            }
        }

        // Draw frame count and frame rate text
        window.draw(frameText);
        window.draw(frameRateText);

        // Draw buttons
        window.draw(pauseButton);
        window.draw(pauseButtonText);
        window.draw(resetButton);
        window.draw(resetButtonText);

        window.display();

    }

    // Output collision statistics
    std::cout << "Grid-based collision calculations: " << gridBasedCollisionCount << std::endl;
    std::cout << "Global collision calculations (estimated): " << globalCollisionCount << std::endl;
    std::cout << "Total procentual reduction: " << 100.0f * (1.0f - static_cast<float>(gridBasedCollisionCount) / globalCollisionCount) << "%" << std::endl;
    return 0;
}