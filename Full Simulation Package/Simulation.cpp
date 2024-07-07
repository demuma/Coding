#include "Simulation.hpp"
#include <iostream>
#include <chrono> // For timing
#include "CollisionAvoidance.hpp"
#include <random>
#include <uuid/uuid.h> // For generating UUIDs

// Constructor
Simulation::Simulation(sf::RenderWindow& window, const sf::Font& font, const YAML::Node& config) 
    : window(window), font(font), config(config), grid(cellSize, window.getSize().x / cellSize, window.getSize().y / cellSize)
{
    loadConfiguration(); 
    initializeAgents();
    initializeUI(); 
}

// Function to load simulation parameters from the YAML configuration file
void Simulation::loadConfiguration() {
    windowWidth = config["display"]["width"].as<int>();
    windowHeight = config["display"]["height"].as<int>();
    cellSize = config["grid"]["cell_size"].as<float>();
    showGrid = config["grid"]["show_grid"].as<bool>();
    showTrajectories = config["grid"]["show_trajectories"].as<bool>();
    numAgents = config["agents"]["num_agents"].as<int>();
    durationSeconds = config["simulation"]["duration_seconds"].as<float>();
    maxFrames = config["simulation"]["maximum_frames"].as<int>();
    fps = config["display"]["frame_rate"].as<int>();
    showInfo = config["grid"]["show_info"].as<bool>();
}

// Function to initialize agents based on the YAML configuration
void Simulation::initializeAgents() {

    // Random number generation for initial agent positions and velocities
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    // Generate general sensor ID
    uuid_t sensor_uuid;
    uuid_generate(sensor_uuid); 

    // Initialize agents based on the proportion of each agent type (TODO: Find better way!!)
    for (const auto& agentType : config["agents"]["road_user_taxonomy"]) {
        int numTypeAgents = numAgents * agentType["proportion"].as<float>();
        sf::Color agentColor = stringToColor(agentType["color"].as<std::string>());

        // Error handling for invalid colors
        if (agentColor == sf::Color::Black) {
            std::cerr << "Invalid color string in config file for agent type: " << agentType["type"].as<std::string>() << std::endl;
            exit(1); // Exit with an error code
        }

        // Create agents of the current type
        for (int i = 0; i < numTypeAgents; ++i) {
            Agent agent;
            agent.uuid = agent.generateUUID();
            agent.sensor_id = agent.generateUUID(sensor_uuid);
            agent.type = agentType["type"].as<std::string>();
            agent.position = sf::Vector2f(rand() % windowWidth, rand() % windowHeight);
            agent.initial_position = agent.position;
            float minVelocity = agentType["min_velocity"].as<float>();
            float maxVelocity = agentType["max_velocity"].as<float>();
            agent.velocity = sf::Vector2f(dis(gen) * maxVelocity, dis(gen) * maxVelocity); 

            // Ensure minimum velocity
            if (std::abs(agent.velocity.x) < minVelocity) {
                agent.velocity.x = agent.velocity.x < 0 ? -minVelocity : minVelocity;
            }
            if (std::abs(agent.velocity.y) < minVelocity) {
                agent.velocity.y = agent.velocity.y < 0 ? -minVelocity : minVelocity;
            }
            
            agent.original_velocity = agent.velocity;
            agent.radius = agentType["radius"].as<float>();
            agent.color = agentColor;
            agent.initial_color = agentColor;
            agent.initialize();
            agents.push_back(agent);
            std::cout << "Agent " << agent.uuid << " created with type " << agent.type << std::endl;
        }
    }
}

// Function to initialize UI elements
void Simulation::initializeUI() {

    // Frame count text
    frameText.setFont(font);
    frameText.setCharacterSize(24);
    frameText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop

    // Frame rate text
    frameRateText.setFont(font);
    frameRateText.setCharacterSize(24);
    frameRateText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop

     // Frame count text
    agentCountText.setFont(font);
    agentCountText.setCharacterSize(24);
    agentCountText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop
    

    // Pause button
    pauseButton.setSize(sf::Vector2f(100, 50)); 
    pauseButton.setFillColor(sf::Color::Green);
    pauseButton.setPosition(window.getSize().x - 110, window.getSize().y - 60);

    pauseButtonText.setFont(font);
    pauseButtonText.setString("Pause");
    pauseButtonText.setCharacterSize(20);
    pauseButtonText.setFillColor(sf::Color::Black);
    
    // Center the pause button text
    sf::FloatRect pauseButtonTextRect = pauseButtonText.getLocalBounds();
    pauseButtonText.setOrigin(pauseButtonTextRect.left + pauseButtonTextRect.width / 2.0f,
                              pauseButtonTextRect.top + pauseButtonTextRect.height / 2.0f);
    pauseButtonText.setPosition(pauseButton.getPosition().x + pauseButton.getSize().x / 2.0f,
                                pauseButton.getPosition().y + pauseButton.getSize().y / 2.0f);

    // Reset button
    resetButton.setSize(sf::Vector2f(100, 50));
    resetButton.setFillColor(sf::Color::Blue);
    resetButton.setPosition(window.getSize().x - 110, window.getSize().y - 120); 

    resetButtonText.setFont(font);
    resetButtonText.setString("Reset");
    resetButtonText.setCharacterSize(20);
    resetButtonText.setFillColor(sf::Color::Black);

    // Center the reset button text
    sf::FloatRect resetButtonTextRect = resetButtonText.getLocalBounds();
    resetButtonText.setOrigin(resetButtonTextRect.left + resetButtonTextRect.width / 2.0f,
                              resetButtonTextRect.top + resetButtonTextRect.height / 2.0f);
    resetButtonText.setPosition(resetButton.getPosition().x + resetButton.getSize().x / 2.0f,
                                resetButton.getPosition().y + resetButton.getSize().y / 2.0f);
}

// Function to convert a string to sf::Color (case-insensitive)
sf::Color Simulation::stringToColor(const std::string& colorStr) {  // Note the Simulation:: scope
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

// Main simulation loop
void Simulation::run() {
    // Collision timing
    std::chrono::duration<double> collisionTime(0.0);

    // Frame rate buffer and calculation
    cumulativeSum = 0.0f;

    // Simulation duration control
    frameCount = 0;

    // Stop main loop if the window is closed, the simulation duration or maximum frames are reached
    while (window.isOpen() && (totalElapsedTime < sf::seconds(durationSeconds) && (maxFrames > 0 && frameCount < maxFrames))) {
        
        // Event handling
        sf::Event event;
        while (window.pollEvent(event)) {
            handleEvents(event); // Delegate event handling (reset, pause, etc.)
        }

        // Update the simulation
        if (!isPaused) {
            elapsedTime = clock.restart();

            // Clear the grid
            grid.clear();
            
            // Update agent positions and add them to the grid
            for (auto& agent : agents) {
                agent.updatePosition(); // Pass deltaTime
                grid.addAgent(&agent);
            }

            // Collision detection using grid
            grid.checkCollisions();

            // Global collision count estimation (for comparison)
            globalCollisionCount += agents.size() * (agents.size() - 1) / 2;

            // Increment frame count and total elapsed time
            frameCount++;
            totalElapsedTime += elapsedTime;

            // Frame rate calculation
            frameRate = 1.0f / elapsedTime.asSeconds();
            if (frameRates.size() == frameRateBufferSize) {
                cumulativeSum -= frameRates[0];
                frameRates.erase(frameRates.begin());
            }
            frameRates.push_back(frameRate);
            cumulativeSum += frameRate;
            movingAverageFrameRate = cumulativeSum / frameRates.size();

            updateFrameRateText(movingAverageFrameRate);
            updateFrameCountText(frameCount);
            updateAgentCountText();
        }

        render(); 
    }
}

// Function to render the simulation
void Simulation::render() {
    window.clear(sf::Color::White);

    // Draw the grid (if enabled)
    if (showGrid) {
        for (int x = 0; x <= window.getSize().x / cellSize; ++x) {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(x * cellSize, 0), sf::Color(220, 220, 220)), // Light gray for grid
                sf::Vertex(sf::Vector2f(x * cellSize, window.getSize().y), sf::Color(220, 220, 220))
            };
            window.draw(line, 2, sf::Lines);
        }
        for (int y = 0; y <= window.getSize().y / cellSize; ++y) {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(0, y * cellSize), sf::Color(220, 220, 220)),
                sf::Vertex(sf::Vector2f(window.getSize().x, y * cellSize), sf::Color(220, 220, 220))
            };
            window.draw(line, 2, sf::Lines);
        }
    }
    
    // Draw agents
    for (const auto& agent : agents) {
        // Draw the agent as a circle
        sf::CircleShape agentShape(agent.radius);
        agentShape.setFillColor(agent.color);
        agentShape.setOrigin(agentShape.getRadius(), agentShape.getRadius());
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
        arrow.setPosition(line[1].position); 
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

    // Show simulation info (frame count, frame rate, agent count)
    if(showInfo) {

        // Draw frame count text
        window.draw(frameText);

        // Draw frame rate text
        window.draw(frameRateText);

        // Draw agent count text
        window.draw(agentCountText);
    }

    // Draw pause button
    window.draw(pauseButton);
    window.draw(pauseButtonText);

    // Draw reset button
    window.draw(resetButton);
    window.draw(resetButtonText);

    // Display the window
    window.display();
}

// Function to update the text that displays the current frame rate
void Simulation::updateFrameRateText(float frameRate) {
    frameRateText.setString("FPS: " + std::to_string(static_cast<int>(frameRate)));
    sf::FloatRect textRect = frameRateText.getLocalBounds();
    frameRateText.setOrigin(textRect.width, 0); // Right-align the text
    frameRateText.setPosition(window.getSize().x - 10, 35); // Position with padding
}

// Function to update the text that displays the current frame count
void Simulation::updateFrameCountText(int frameCount) {
    frameText.setString("Frame " + std::to_string(frameCount) + "/" + (maxFrames > 0 ? std::to_string(maxFrames) : "∞")); // ∞ for unlimited frames
    sf::FloatRect textRect = frameText.getLocalBounds();
    frameText.setOrigin(textRect.width, 0); // Right-align the text
    frameText.setPosition(window.getSize().x - 10, 5); // Position with padding
}

// Function to update the text that displays the current agent count
void Simulation::updateAgentCountText() {
    agentCountText.setString("Agents: " + std::to_string(agents.size()));
    sf::FloatRect textRect = agentCountText.getLocalBounds();
    agentCountText.setOrigin(textRect.width, 0); // Right-align the text
    agentCountText.setPosition(window.getSize().x - 5, 65); // Position with padding
}

// Function to handle events (mouse clicks, window close, etc.)
void Simulation::handleEvents(sf::Event event) {

    if (event.type == sf::Event::Closed) {
        window.close();
    } else if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2f mousePos(event.mouseButton.x, event.mouseButton.y);

            // Pause/Resume button click
            if (pauseButton.getGlobalBounds().contains(mousePos)) {
                isPaused = !isPaused; // Toggle pause state
                pauseButtonText.setString(isPaused ? "  Play" : "Pause");
                pauseButton.setFillColor(isPaused ? sf::Color::Red : sf::Color::Green);
            }

            // Reset button click
            else if (resetButton.getGlobalBounds().contains(mousePos)) {
                resetSimulation();
                // Reset frame count, frame rate, and total elapsed time
                frameCount = 0;
                frameRates.clear();
                cumulativeSum = 0.0f;
                totalElapsedTime = sf::seconds(0.0f);

                // Resume simulation if it was paused
                if (isPaused) {
                    isPaused = false;
                    pauseButtonText.setString("Pause");
                    pauseButton.setFillColor(sf::Color::Green);
                }
            }
        }
    }
}

// Function to reset the simulation
void Simulation::resetSimulation() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    for (auto& agent : agents) {
        agent.position = sf::Vector2f(rand() % windowWidth, rand() % windowHeight);
        agent.initial_position = agent.position;
        
        // Update agent's velocity with the original velocity to make it look like all agents change direction at once
        agent.velocity = agent.original_velocity; 

        agent.color = agent.initial_color;
        agent.bufferColor = sf::Color::Green;
        agent.hasCollision = false;
        agent.stopped = false;
        agent.stoppedFrameCounter = 0;
        agent.initialize();
    }
    
    grid.clear();
    gridBasedCollisionCount = 0;
    globalCollisionCount = 0;
}

void Simulation::update(float deltaTime) {
    for (auto& agent : agents) {
        agent.updatePosition();
        agent.resetCollisionState(); // Reset collision state at the start of each frame for each agent

        // Check if agent is out of bounds and wrap around
        if (agent.position.x > windowWidth + agent.radius) {
            agent.position.x = -agent.radius; // Wrap around from right to left
            agent.initial_position.x = agent.position.x; // Reset initial position for trajectory
        } else if (agent.position.x < -agent.radius) {
            agent.position.x = windowWidth + agent.radius; // Wrap around from left to right
            agent.initial_position.x = agent.position.x; // Reset initial position for trajectory
        }

        if (agent.position.y > windowHeight + agent.radius) {
            agent.position.y = -agent.radius; // Wrap around from bottom to top
            agent.initial_position.y = agent.position.y; // Reset initial position for trajectory
        } else if (agent.position.y < -agent.radius) {
            agent.position.y = windowHeight + agent.radius; // Wrap around from top to bottom
            agent.initial_position.y = agent.position.y; // Reset initial position for trajectory
        }

        sf::Vector2i cellIndex = getGridCellIndex(agent.position, cellSize);
        grid.getCells()[cellIndex].agents.push_back(&agent);

        // Resume agents if they are stopped
        if (agent.stopped) {
            ++agent.stoppedFrameCounter;
            agent.resume(agents); 
        }
    }

    // Collision detection using grid
    grid.checkCollisions();
}