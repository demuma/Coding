#include <iostream>
#include <chrono> // For timing
#include <random>
#include <uuid/uuid.h> // For generating UUIDs

#include "Grid.hpp"
#include "Simulation.hpp"
#include "CollisionAvoidance.hpp"

// Constructor
Simulation::Simulation(sf::RenderWindow& window, const sf::Font& font, const YAML::Node& config) 
    : window(window), font(font), config(config), grid(cellSize, windowWidthScaled / cellSize, windowHeightScaled / cellSize){
    
    loadConfiguration();
    loadObstacles();
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
    fps = config["simulation"]["frame_rate"].as<int>();
    showInfo = config["grid"]["show_info"].as<bool>();
    scale = config["display"]["pixels_per_meter"].as<int>();

    // Scale window dimensions
    windowWidthScaled = windowWidth / static_cast<float>(scale);
    windowHeightScaled = windowHeight / static_cast<float>(scale);

    // Default frame rate based on time step
    float timeStepFloat = 1.0f / fps;
    
    // Check if the simulation uses a fixed time step
    if (config["simulation"]["use_time_step"].as<bool>()) {
        timeStepFloat = config["simulation"]["time_step"].as<float>();
    }

    // Convert to sf::Time using sf::seconds()
    timeStep = sf::seconds(timeStepFloat);
}

// Function to initialize agents based on the YAML configuration
void Simulation::initializeAgents() {

    // Random number generation for initial agent positions and velocities
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    std::uniform_real_distribution<> disX(0.0, static_cast<float>(windowWidth) / static_cast<float>(scale));
    std::uniform_real_distribution<> disY(0.0, static_cast<float>(windowHeight) / static_cast<float>(scale));

    // Generate general sensor ID
    uuid_t sensor_uuid;
    uuid_generate(sensor_uuid); 

    // Initialize agents based on the proportion of each agent type (TODO: Find better way!!)
    for (const auto& agentType : config["agents"]["road_user_taxonomy"]) {
        int numTypeAgents = numAgents * agentType["probability"].as<float>();
        sf::Color agentColor = stringToColor(agentType["color"].as<std::string>());

        // Error handling for invalid colors
        if (agentColor == sf::Color::Black) {
            std::cerr << "Invalid color string in config file for agent type: " << agentType["type"].as<std::string>() << std::endl;
            exit(1); // Exit with an error code
        }

        // Create agents of the current type
        for (int i = 0; i < numTypeAgents; ++i) {

            // Create a new agent
            Agent agent;

            // Assign the agent type, UUID, and sensor ID, radius and color
            agent.uuid = agent.generateUUID();
            agent.sensor_id = agent.generateUUID(sensor_uuid);
            agent.type = agentType["type"].as<std::string>();
            agent.radius = agentType["radius"].as<float>();
            agent.color = agentColor;
            agent.initial_color = agentColor;

            // Random initial position within the window bounds
            agent.position = sf::Vector2f(disX(gen), disY(gen)); // Random position in meters
            agent.initialPosition = agent.position;

            // Random initial velocity
            agent.minVelocity = agentType["velocity"]["min"].as<float>();
            agent.maxVelocity = agentType["velocity"]["max"].as<float>();
            agent.velocity = sf::Vector2f(dis(gen) * agent.maxVelocity, dis(gen) * agent.maxVelocity);
            agent.originalVelocity = agent.velocity;

            // Random initial acceleration
            agent.minAcceleration = agentType["acceleration"]["min"].as<float>();
            agent.maxAcceleration = agentType["acceleration"]["max"].as<float>();
            agent.acceleration = sf::Vector2f(dis(gen) * agent.maxAcceleration, dis(gen) * agent.maxAcceleration);

            // Ensure minimum velocity
            if (std::abs(agent.velocity.x) < agent.minVelocity) {
                agent.velocity.x = agent.velocity.x < 0 ? -agent.minVelocity : agent.minVelocity;
            }
            if (std::abs(agent.velocity.y) < agent.minVelocity) {
                agent.velocity.y = agent.velocity.y < 0 ? -agent.minVelocity : agent.minVelocity;
            }

            // Ensure minimum acceleration
            if (std::abs(agent.acceleration.x) < agent.minAcceleration) {
                agent.acceleration.x = agent.acceleration.x < 0 ? -agent.minAcceleration : agent.minAcceleration;
            }
            if (std::abs(agent.acceleration.y) < agent.minAcceleration) {
                agent.acceleration.y = agent.acceleration.y < 0 ? -agent.minAcceleration : agent.minAcceleration;
            }
            
            // Initialize the agent
            agent.initialize();
            agents.push_back(agent);
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

// Calculate frame rate
void Simulation::calculateFrameRate() {
    // Calculate frame rate
    frameRate = 1.0f / clock.restart().asSeconds();
    if (frameCount > 0) {
        if (frameRates.size() == frameRateBufferSize) {
            cumulativeSum -= frameRates[0];
            frameRates.erase(frameRates.begin());
        }
        frameRates.push_back(frameRate);
        cumulativeSum += frameRate;
        movingAverageFrameRate = cumulativeSum / frameRates.size();

        // Update frame rate text
        updateFrameRateText(movingAverageFrameRate);
    }
}

// Main simulation loop
void Simulation::run() {

    // Set up the MongoDB database
    // collection = setupDatabase();

    // MongoDB Setup from Configuration
    std::string mongoHost = config["database"]["host"].as<std::string>();
    int mongoPort = config["database"]["port"].as<int>();
    std::string mongoDbName = config["database"]["db_name"].as<std::string>();
    std::string mongoCollectionName = config["database"]["collection_name"].as<std::string>();

    std::string mongoUri = "mongodb://" + mongoHost + ":" + std::to_string(mongoPort);
    mongocxx::instance inst{};
    mongocxx::uri uri(mongoUri);
    mongocxx::client client(uri);
    mongocxx::database db = client[mongoDbName];
    collection = db[mongoCollectionName];

    // Frame timing variables
    sf::Clock clock;  
    sf::Time timeSinceLastUpdate = sf::Time::Zero;

    // Simulation duration control and FPS variables
    frameCount = 0;
    cumulativeSum = 0.0f;
    frameRates.clear();

    // Main simulation loop
    while (window.isOpen() && (totalElapsedTime < sf::seconds(durationSeconds) || maxFrames == 0) && 
        frameCount < maxFrames) {
        
        // Event handling
        sf::Event event;
        while (window.pollEvent(event)) {
            handleEvents(event); 
        }

        // Timestep-based update
        timeSinceLastUpdate += clock.restart(); 
        while (timeSinceLastUpdate >= timeStep) { 
            if (!isPaused) {
                // Update the simulation
                // Clear the grid
                grid.clear();
                update(timeStep.asSeconds());  // Use timeStep for consistent updates

                // Store agent data in MongoDB
                storeAgentData(agents);

                // Frame rate calculation
                calculateFrameRate();

                // Update frame count and agent count text
                updateFrameCountText(frameCount);
                updateAgentCountText();

                // Increment frame count and total elapsed time
                frameCount++;
                totalElapsedTime += timeStep;
            }
            // Decrement the accumulator
            timeSinceLastUpdate -= timeStep;  // Decrement accumulator
        }

        // Render the simulation
        render(); 
    }
}

// Function to render the simulation
void Simulation::render() {

    // Clear the window
    window.clear(sf::Color::White);

    // Draw the grid
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

    // Draw obstacles
    for (const Obstacle& obstacle : obstacles) {
        sf::RectangleShape obstacleShape(sf::Vector2f(obstacle.getBounds().width * scale, obstacle.getBounds().height * scale)); // No scaling here
        obstacleShape.setPosition(obstacle.getBounds().left * scale, obstacle.getBounds().top * scale); // Scale only position here
        obstacleShape.setFillColor(obstacle.getColor());
        window.draw(obstacleShape);
    }
    
    // Draw agents
    for (const auto& agent : agents) {
        // Draw the agent as a circle
        sf::CircleShape agentShape(agent.radius * scale);
        agentShape.setFillColor(agent.color);
        agentShape.setOrigin(agentShape.getRadius(), agentShape.getRadius());
        agentShape.setPosition(agent.position.x * scale, agent.position.y * scale); // Scale the position to pixels
        window.draw(agentShape);

        // Draw the buffer zone
        sf::CircleShape bufferZoneShape(agent.bufferRadius * scale);
        bufferZoneShape.setOrigin(bufferZoneShape.getRadius(), bufferZoneShape.getRadius());
        bufferZoneShape.setPosition(agent.position.x * scale, agent.position.y * scale);
        bufferZoneShape.setFillColor(sf::Color::Transparent);
        bufferZoneShape.setOutlineThickness(2.f);
        bufferZoneShape.setOutlineColor(agent.bufferColor);
        window.draw(bufferZoneShape);

        // Draw the arrow (a triangle) representing the velocity vector
        sf::Vector2f direction = agent.velocity;  // Direction of the arrow from velocity vector
        float arrowLength = agent.radius * scale * 0.5f;
        float arrowAngle = std::atan2(direction.y, direction.x);

        // Normalize the direction vector for consistent arrow length
        sf::Vector2f normalizedDirection = direction / std::sqrt(direction.x * direction.x + direction.y * direction.y);

        // Arrow shape factor
        float arrowHeadLength = 0.4 * static_cast<float>(scale);
        float arrowHeadWidth = 0.25 * static_cast<float>(scale);
        int lineThickness = 3;

        sf::ConvexShape arrow(3);
        arrow.setPoint(0, sf::Vector2f(0, 0)); // Tip of the arrow
        arrow.setPoint(1, sf::Vector2f(-arrowHeadLength, arrowHeadWidth / 2));
        arrow.setPoint(2, sf::Vector2f(-arrowHeadLength , -arrowHeadWidth / 2));
        arrow.setFillColor(sf::Color::Black);

        // Line (arrow body) - Offset the start by the agent's radius
        sf::Vertex line[] =
        {
            sf::Vertex(agent.position * static_cast<float>(scale) + normalizedDirection * agent.radius * static_cast<float>(scale), sf::Color::Black),  // Offset start point
            sf::Vertex(agent.position * static_cast<float>(scale) + normalizedDirection * agent.radius * static_cast<float>(scale) + direction * (arrowLength / 2.0f), sf::Color::Black) // Ending point (base of arrowhead)
        };

        // Set the origin of the arrowhead to the base of the triangle
        arrow.setOrigin(-arrowHeadLength, 0); 
        arrow.setPosition(line[1].position); 
        arrow.setRotation(arrowAngle * 180.0f / M_PI); // Convert radians to degrees for SFML
        window.draw(arrow);
        window.draw(line, 2, sf::Lines);

        // Draw trajectory (if enabled in config)
        if (showTrajectories) {
            
            sf::Vertex trajectory[] =
            {
                sf::Vertex(agent.initialPosition * static_cast<float>(scale), agent.initial_color),
                sf::Vertex(agent.position * static_cast<float>(scale), agent.initial_color)
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
    } else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            window.close();
        }
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

    agents.clear();     // Clear the existing agents
    initializeAgents(); // Recreate agents based on config

    // Reset other simulation elements
    clock.restart();
    frameRates.clear();
    cumulativeSum = 0.0f;
    totalElapsedTime = sf::seconds(0.0f);

    // Reset the grid
    grid.clear();

    // Resume simulation if it was paused
    if (isPaused) {
        isPaused = false;
        pauseButtonText.setString("Pause");
        pauseButton.setFillColor(sf::Color::Green);
    }
}

// Update the simulation state on each frame based on the time step
void Simulation::update(float deltaTime) {

    for (auto agent = agents.begin(); agent != agents.end(); ) {
        agent->updatePosition(deltaTime);
        agent->resetCollisionState(); // Reset collision state at the start of each frame for each agent

        // Check if agent is out of bounds TODO: make separate function
        if (agent->position.x > windowWidthScaled + agent->radius || agent->position.x < -agent->radius ||
            agent->position.y > windowHeightScaled + agent->radius || agent->position.y < -agent->radius) {
            agent = agents.erase(agent); // Remove the agent from the vector and update the iterator
            continue; // Skip to the next agent (the iterator is already updated)
        }
        //std::cout << "Agent position: " << agent->position.x << ", " << agent->position.y << std::endl;
        //std::cout << "Window width: " << windowWidthScaled << ", " << windowHeightScaled << std::endl;

        // Assign the agent to the correct grid cell
        grid.addAgent(&(*agent)); // Add agent to the grid

        // Resume agents if they are stopped
        if (agent->stopped) {
            ++(agent->stoppedFrameCounter);
            agent->resume(agents); 
        }
        // Check for agent-obstacle collisions
        agentObstacleCollision(*agent, obstacles);

        ++agent; // Move to the next agent
    }
    
    // Collision detection using grid
    grid.checkCollisions();
}

// Function to load obstacles from the YAML configuration file
void Simulation::loadObstacles() {

    // Check if the 'obstacles' key exists and is a sequence
    if (config["obstacles"] && config["obstacles"].IsSequence()) {
        for (const auto& obstacleNode : config["obstacles"]) {
            std::string type = obstacleNode["type"] && obstacleNode["type"].IsScalar()
                ? obstacleNode["type"].as<std::string>()
                : "unknown";

            if (type == "rectangle") { // Only handle rectangles
                std::vector<float> position = obstacleNode["position"].as<std::vector<float>>();
                std::vector<float> size = obstacleNode["size"].as<std::vector<float>>();
                obstacles.push_back(Obstacle(
                    sf::FloatRect(position[0], position[1], size[0], size[1]), 
                    stringToColor(obstacleNode["color"].as<std::string>())
                ));
            } else {
                std::cerr << "Error: Unknown obstacle type '" << type << "' in config file." << std::endl;
            }
        }
    } else {
        std::cerr << "Error: Could not find 'obstacles' key in config file or it is not a sequence." << std::endl;
    }
}

// Store agent data in MongoDB
void Simulation::storeAgentData(const std::vector<Agent>& agents) {

    for (const auto& agent : agents) {

        // Prepare data for MongoDB
        bsoncxx::builder::stream::document document{};

        // Get current timestamp in ISO format
        std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now), "%FT%TZ");

        document << "timestamp" << ss.str()
                << "sensor_id" << agent.sensor_id
                << "agent_id" << agent.uuid
                << "type" << agent.type
                << "position" << bsoncxx::builder::stream::open_array
                    << agent.position.x << agent.position.y
                    << bsoncxx::builder::stream::close_array
                << "velocity" << bsoncxx::builder::stream::open_array
                    << agent.velocity.x << agent.velocity.y
                    << bsoncxx::builder::stream::close_array;

        // Insert document into MongoDB
        collection.insert_one(document.view());
    }
}

// Setup the MongoDB database
mongocxx::collection Simulation::setupDatabase() {
    // MongoDB Setup from Configuration
    std::string mongoHost = config["database"]["host"].as<std::string>();
    int mongoPort = config["database"]["port"].as<int>();
    std::string mongoDbName = config["database"]["db_name"].as<std::string>();
    std::string mongoCollectionName = config["database"]["collection_name"].as<std::string>();

    std::string mongoUri = "mongodb://" + mongoHost + ":" + std::to_string(mongoPort);
    mongocxx::instance inst{};
    mongocxx::uri uri(mongoUri);
    mongocxx::client client(uri);
    mongocxx::database db = client[mongoDbName];

    // Return the collection directly 
    return db[mongoCollectionName];
}