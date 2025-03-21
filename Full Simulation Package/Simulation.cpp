#include <iostream>
#include <chrono> // For timing
#include <random>
#include <uuid/uuid.h> // For generating UUIDs

#include "Grid.hpp"
#include "Simulation.hpp"
#include "CollisionAvoidance.hpp"

// Constructor with example sensor initialization
Simulation::Simulation(sf::RenderWindow& window, const sf::Font& font, const YAML::Node& config) 
    : window(window), font(font), config(config), grid(0, 0, 0), instance {} { // Initialize grid with dummy values
    
    loadConfiguration();
    initializeDatabase();
    initializeGrid();
    loadObstacles();
    initializeAgents();
    initializeSensors();
    initializeUI(); 
}

// Function to load simulation parameters from the YAML configuration file
void Simulation::loadConfiguration() {

    windowWidth = config["display"]["width"].as<int>();
    windowHeight = config["display"]["height"].as<int>();
    collisionGridCellSize = config["collision"]["grid"]["cell_size"].as<float>();
    showCollisionGrid = config["collision"]["grid"]["show_grid"].as<bool>();
    showTrajectories = config["agents"]["show_trajectories"].as<bool>();
    numAgents = config["agents"]["num_agents"].as<int>();
    durationSeconds = config["simulation"]["duration_seconds"].as<float>();
    maxFrames = config["simulation"]["maximum_frames"].as<int>();
    showInfo = config["display"]["show_info"].as<bool>();
    scale = config["display"]["pixels_per_meter"].as<int>();
    timeStep = sf::seconds(config["simulation"]["time_step"].as<float>());
    speedFactor = config["simulation"]["speed_factor"].as<float>();
    simulationWidth = config["simulation"]["width"].as<float>();
    simulationHeight = config["simulation"]["height"].as<float>();
    std::string dbHost = config["database"]["host"].as<std::string>();
    int dbPort = config["database"]["port"].as<int>();
    databaseName = config["database"]["db_name"].as<std::string>();
    dbUri = "mongodb://" + dbHost + ":" + std::to_string(dbPort);

    // If datetime is provided in the configuration, use it, otherwise use the current time
    if(config["simulation"]["datetime"]) {

        // Set time from configuration
        datetime = config["simulation"]["datetime"].as<std::string>();
    } else {

        // Set current time
        datetime = generateISOTimestamp();
    }
    

    // Scale window dimensions
    windowWidthScaled = static_cast<float>(windowWidth) / static_cast<float>(scale);
    windowHeightScaled = static_cast<float>(windowHeight) / static_cast<float>(scale);
    
    // Simulation offsets
    simulationWidthOffsetScaled = (windowWidthScaled - simulationWidth) / 2.0f;
    simulationHeightOffsetScaled = (windowHeightScaled - simulationHeight) / 2.0f;
}

// Initialize the database connection
void Simulation::initializeDatabase() {

    // MongoDB URI
    mongocxx::uri uri(dbUri);

    // Initialize the MongoDB client
    client = std::make_shared<mongocxx::client>(uri);
}

// Function to initialize the grid based on the YAML configuration
void Simulation::initializeGrid() {

    // Initialize the grid
    grid = Grid(collisionGridCellSize, simulationWidth / collisionGridCellSize, simulationHeight / collisionGridCellSize);
}

// Function to initialize agents based on the YAML configuration
void Simulation::initializeAgents() {

    // Random number generation for initial agent positions and velocities
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> disX(0, simulationWidth);
    std::uniform_real_distribution<> disY(0, simulationHeight);

    // Generate general sensor ID
    uuid_t sensor_uuid;
    uuid_generate(sensor_uuid); 

    // Initialize agents based on the proportion of each agent type (TODO: Find better way!!)
    for (const auto& agentType : config["agents"]["road_user_taxonomy"]) {

        // Calculate the number of agents of the current type
        int numTypeAgents = numAgents * agentType["probability"].as<float>();
        sf::Color agentColor = stringToColor(agentType["color"].as<std::string>());
        int agentPriority = agentType["priority"].as<int>();
        float agentLookAheadTime = agentType["look_ahead_time"].as<float>();

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
            agent.uuid = generateUUID();
            //agent.sensor_id = agent.generateUUID(sensor_uuid);
            agent.sensor_id = "0";
            agent.type = agentType["type"].as<std::string>();
            agent.radius = agentType["radius"].as<float>();
            agent.color = agentColor;
            agent.initial_color = agentColor;
            agent.priority = agentPriority;
            agent.lookAheadTime = agentLookAheadTime;

            // Random initial position within the window bounds
            agent.position = sf::Vector2f(disX(gen), disY(gen)); // Random position in meters

            // Ensure agents are not too close to each other
            while (agentAgentsCollision(agent, agents) || agentObstaclesCollision(agent, obstacles)) {

                // If collision detected, reposition the agent
                agent.position = sf::Vector2f(disX(gen), disY(gen)); // Random position in meters
            }
            
            // Store the initial position for future reference
            agent.initialPosition = agent.position;

            // Set velocity from individual truncated normal distribution
            agent.minVelocity = agentType["velocity"]["min"].as<float>();
            agent.maxVelocity = agentType["velocity"]["max"].as<float>();
            agent.velocityMu = agentType["velocity"]["mu"].as<float>();
            agent.velocitySigma = agentType["velocity"]["sigma"].as<float>();
            agent.velocityNoiseFactor = agentType["velocity"]["noise_factor"].as<float>();
            agent.velocityNoiseScale = agentType["velocity"]["noise_scale"].as<float>();

            // Convert angle to velocity vector from truncated normal distribution
            agent.velocity = generateRandomVelocityVector(agent.velocityMu, agent.velocitySigma, agent.minVelocity, agent.maxVelocity);
            
            // Store the original velocity for future reference
            agent.originalVelocity = agent.velocity;

            // Generate seed for Perlin noise
            agent.noiseSeed = std::random_device{}();
            agent.perlinNoise = PerlinNoise(agent.noiseSeed);

            // Random initial acceleration
            agent.minAcceleration = agentType["acceleration"]["min"].as<float>();
            agent.maxAcceleration = agentType["acceleration"]["max"].as<float>();
            
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

    // Frame count text
    timeText.setFont(font);
    timeText.setCharacterSize(24);
    timeText.setFillColor(sf::Color::Black);
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

// Initialize sensors with YAML configuration
void Simulation::initializeSensors() {

    // Load sensors from the configuration file
    const YAML::Node& sensorNodes = config["sensors"];

    // Iterate over each sensor node in the configuration
    for (const auto& sensorNode : sensorNodes) {

        // Get the sensor type and frame rate
        std::string type = sensorNode["type"].as<std::string>();
        float frameRate = sensorNode["frame_rate"].as<float>();
        sf::Color color = stringToColor(sensorNode["detection_area"]["color"].as<std::string>());
        int alpha = sensorNode["detection_area"]["alpha"].as<float>() * 255;
        sf::Color colorAlpha = sf::Color(color.r, color.g, color.b, alpha);
        std::string databaseName = sensorNode["database"]["db_name"].as<std::string>();
        std::string collectionName = sensorNode["database"]["collection_name"].as<std::string>();

        // Define the detection area for the sensor
        sf::FloatRect detectionArea(

            sensorNode["detection_area"]["left"].as<float>(),
            sensorNode["detection_area"]["top"].as<float>(),
            sensorNode["detection_area"]["width"].as<float>(),
            sensorNode["detection_area"]["height"].as<float>()
        );

        // Create the agent-based sensor
        if (type == "agent-based") {
            
            // Create the agent-based sensor and add to sensors vector
            sensors.push_back(std::make_unique<AgentBasedSensor>(frameRate, detectionArea, colorAlpha, databaseName, collectionName, client));
            sensors.back()->scale = scale;

            // Set initial positions for agents in the detection area
            for(const auto& agent : agents) {

                // Check if the agent is within the detection area
                if(sensors.back()->detectionArea.contains(agent.position)) {

                    // Get raw pointer from unique_ptr
                    Sensor* sensor = sensors.back().get();

                    // Try to cast to AgentBasedSensor*
                    auto agentBasedSensor = dynamic_cast<AgentBasedSensor*>(sensor); 

                    // Check if cast succeeded
                    if (agentBasedSensor != nullptr) { 
                        agentBasedSensor->previousPositions[agent.uuid] = agent.position;
                    }
                }
            }

        // Create the grid-based sensor
        } else if (type == "grid-based") {

            float cellSize = sensorNode["grid"]["cell_size"].as<float>();
            bool showGrid = sensorNode["grid"]["show_grid"].as<bool>();

            // Create the grid-based sensor and add to sensors vector
            sensors.push_back(std::make_unique<GridBasedSensor>(frameRate, detectionArea, colorAlpha, cellSize, showGrid, databaseName, collectionName, client));
            sensors.back()->scale = scale;
        }
    }
}

// Calculate the current frame rate
void Simulation::calculateFrameRate() {

    // Calculate frame rate only after the warmup period
    if (frameCount > warmupFrames) {
        frameRate = 1.0f / clock.restart().asSeconds();

        // Check if the frame rate buffer is full
        if (frameRates.size() == frameRateBufferSize) {
            cumulativeSum -= frameRates[0];
            frameRates.erase(frameRates.begin());
        }

        // Add the new frame rate to the buffer
        frameRates.push_back(frameRate);
        cumulativeSum += frameRate;

        // Ensure you have enough samples for the moving average
        if (frameRates.size() >= 2) {  
            movingAverageFrameRate = cumulativeSum / frameRates.size();
            updateFrameRateText(movingAverageFrameRate);
        }
    } else {
        clock.restart(); // Discard the timing of the warmup frames
    }

    frameCount++; // Increment frame count regardless of warmup
}

// Main simulation loop
void Simulation::run() {

    // MongoDB Setup from Configuration
    std::string mongoHost = config["database"]["host"].as<std::string>();
    int mongoPort = config["database"]["port"].as<int>();
    std::string mongoDbName = config["database"]["db_name"].as<std::string>();
    std::string mongoCollectionName = config["database"]["collection_name"].as<std::string>();

    std::string mongoUri = "mongodb://" + mongoHost + ":" + std::to_string(mongoPort);
    mongocxx::uri uri(mongoUri);
    mongocxx::client client(uri);
    mongocxx::database db = client[mongoDbName];
    collection = db[mongoCollectionName];

    // Frame timing variables
    totalElapsedTime = sf::Time::Zero;

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

        // Check if the simulation is paused
        if (!isPaused) {

            // Clear the grid
            grid.clear();
            update(timeStep.asSeconds());  // Use timeStep for consistent updates

            // Frame rate calculation
            calculateFrameRate();

            // Update frame count and agent count text
            updateFrameCountText(frameCount);
            updateAgentCountText();

            // Update the sensors
            updateTimeText();

            // Increment frame count and total elapsed time
            frameCount++;
            
            // Update the total simulation time
            totalElapsedTime += timeStep;
        }

        // Render the simulation
        render(); 
    }
}

// Function to render the simulation
void Simulation::render() {

    // Offset vector
    sf::Vector2f offset(simulationWidthOffsetScaled, simulationHeightOffsetScaled);

    // Convert scale to float
    float scale = static_cast<float>(this->scale);

    // Scale the offset to pixels
    sf::Vector2f offsetScaled = offset * scale; 

    // Clear the window
    window.clear(sf::Color::White);

    // Draw the grid if enabled (check)
    if(showCollisionGrid) {

        // Draw outer rectangle
        sf::RectangleShape gridBoundaries(sf::Vector2f(simulationWidth * scale, simulationHeight * scale));
        gridBoundaries.setPosition(offsetScaled.x, offsetScaled.y); // Set position in pixels
        gridBoundaries.setFillColor(sf::Color::Transparent);
        gridBoundaries.setOutlineColor(sf::Color::Red);
        window.draw(gridBoundaries);

        // Draw vertical lines
        for (int x = 0; x <= (simulationWidth / collisionGridCellSize); x++) {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f((x * collisionGridCellSize) * scale, 0) + offsetScaled, sf::Color(220, 220, 220)),
                sf::Vertex(sf::Vector2f((x * collisionGridCellSize) * scale, simulationHeight * scale) + offsetScaled, sf::Color(220, 220, 220))
            };
            window.draw(line, 2, sf::Lines);
        }

        // Draw horizontal lines
        for (int x = 0; x <= (simulationHeight / collisionGridCellSize); x++) {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(0, (x * collisionGridCellSize) * scale) + offsetScaled, sf::Color(220, 220, 220)),
                sf::Vertex(sf::Vector2f(simulationWidth * scale, (x * collisionGridCellSize) * scale) + offsetScaled, sf::Color(220, 220, 220))
            };
            window.draw(line, 2, sf::Lines);
        }
    }

    // Draw the sensor grids (check)
    if (showSensorGrid) {

        // Loop through sensors pointer
        for (const auto& sensor : sensors) {

            // Check if the sensor is grid-based
            if (auto gridBasedSensor = dynamic_cast<GridBasedSensor*>(sensor.get())) {

                // Draw the grid if enabled
                if(gridBasedSensor->showGrid) {

                    // Draw outer rectangle
                    sf::RectangleShape gridBoundaries(sf::Vector2f(gridBasedSensor->detectionArea.width * scale, gridBasedSensor->detectionArea.height * scale));
                    gridBoundaries.setPosition(gridBasedSensor->detectionArea.left + offsetScaled.x, gridBasedSensor->detectionArea.top + offsetScaled.y); // Set position in pixels
                    gridBoundaries.setFillColor(sf::Color::Transparent);
                    window.draw(gridBoundaries);

                    // Draw vertical lines
                    for (int x = 0; x <= (gridBasedSensor->detectionArea.width / gridBasedSensor->cellSize); x++) {
                        sf::Vertex line[] = {
                            sf::Vertex(sf::Vector2f((gridBasedSensor->detectionArea.left + x * gridBasedSensor->cellSize) * scale, gridBasedSensor->detectionArea.top * scale) + offsetScaled, sf::Color(220, 220, 220)),
                            sf::Vertex(sf::Vector2f((gridBasedSensor->detectionArea.left + x * gridBasedSensor->cellSize) * scale, (gridBasedSensor->detectionArea.height + gridBasedSensor->detectionArea.top) * scale) + offsetScaled, sf::Color(220, 220, 220))
                        };
                        window.draw(line, 2, sf::Lines);
                    }

                    // Draw horizontal lines
                    for (int x = 0; x <= (gridBasedSensor->detectionArea.height / gridBasedSensor->cellSize); x++) {
                        sf::Vertex line[] = {
                            sf::Vertex(sf::Vector2f((gridBasedSensor->detectionArea.left) * scale, (gridBasedSensor->detectionArea.top + x * gridBasedSensor->cellSize) * scale) + offsetScaled, sf::Color(220, 220, 220)),
                            sf::Vertex(sf::Vector2f((gridBasedSensor->detectionArea.left + gridBasedSensor->detectionArea.width) * scale, (gridBasedSensor->detectionArea.top + x * gridBasedSensor->cellSize) * scale) + offsetScaled, sf::Color(220, 220, 220))
                        };
                        window.draw(line, 2, sf::Lines);
                    }
                }
            }   
        }
    }

    // Draw obstacles (check)
    for (const Obstacle& obstacle : obstacles) {

        // Draw the obstacle as a rectangle
        sf::RectangleShape obstacleShape(sf::Vector2f(obstacle.getBounds().width * scale, obstacle.getBounds().height * scale));
        obstacleShape.setPosition((obstacle.getBounds().left) * scale + offsetScaled.x, (obstacle.getBounds().top) * scale + offsetScaled.y); // Scale only position here
        obstacleShape.setFillColor(obstacle.getColor());
        window.draw(obstacleShape);
    }
    
    // Draw agents 
    for (const auto& agent : agents) {

        // Draw the agent as a circle (check)
        sf::CircleShape agentShape(agent.radius * scale);
        agentShape.setFillColor(agent.color);
        agentShape.setOrigin(agentShape.getRadius(), agentShape.getRadius());
        agentShape.setPosition(agent.position.x * scale + offsetScaled.x, agent.position.y * scale + offsetScaled.y); // Scale the position to pixels
        window.draw(agentShape);

        // Draw the buffer zone (check)
        sf::CircleShape bufferZoneShape(agent.bufferRadius * scale);
        bufferZoneShape.setOrigin(bufferZoneShape.getRadius(), bufferZoneShape.getRadius());
        bufferZoneShape.setPosition(agent.position.x * scale + offsetScaled.x, agent.position.y * scale + offsetScaled.y);
        bufferZoneShape.setFillColor(sf::Color::Transparent);
        bufferZoneShape.setOutlineThickness(2.f);
        bufferZoneShape.setOutlineColor(agent.bufferColor);
        window.draw(bufferZoneShape);

        // Draw the arrow (a triangle) representing the velocity vector (check)
        sf::Vector2f direction = agent.velocity;  // Direction of the arrow from velocity vector
        float arrowLength = agent.radius * scale * 0.5f;
        float arrowAngle = std::atan2(direction.y, direction.x);

        // Normalize the direction vector for consistent arrow length (check)
        sf::Vector2f normalizedDirection = direction / std::sqrt(direction.x * direction.x + direction.y * direction.y);

        // Arrow shape factor (check)
        float arrowHeadLength = 0.4 * scale;
        float arrowHeadWidth = 0.25 * scale;
        int lineThickness = 3;

        // Arrowhead (triangle) - Offset the tip by the agent's radius (check)
        sf::ConvexShape arrow(3);
        arrow.setPoint(0, sf::Vector2f(0, 0)); // Tip of the arrow
        arrow.setPoint(1, sf::Vector2f(-arrowHeadLength, arrowHeadWidth / 2));
        arrow.setPoint(2, sf::Vector2f(-arrowHeadLength , -arrowHeadWidth / 2));
        arrow.setFillColor(sf::Color::Black);

        // Line (arrow body) - Offset the start by the agent's radius (check)
        sf::Vertex line[] =
        {
            sf::Vertex((agent.position * scale + normalizedDirection * agent.radius * scale) + offsetScaled, sf::Color::Black),  // Offset start point
            sf::Vertex((agent.position * scale + normalizedDirection * agent.radius * scale + direction * (arrowLength / 2.0f)) + offsetScaled, sf::Color::Black) // Ending point (base of arrowhead)
        };

        // Set the origin of the arrowhead to the base of the triangle (check)
        arrow.setOrigin(-arrowHeadLength, 0); 
        arrow.setPosition(line[1].position); 
        arrow.setRotation(arrowAngle * 180.0f / M_PI); // Convert radians to degrees for SFML
        window.draw(arrow);
        window.draw(line, 2, sf::Lines);

        // Draw trajectory (check)
        if (showTrajectories) {
            
            sf::Vertex trajectory[] =
            {
                sf::Vertex(agent.initialPosition * scale + offsetScaled, agent.initial_color),
                sf::Vertex(agent.position * scale + offsetScaled, agent.initial_color)
            };
            window.draw(trajectory, 2, sf::Lines);
        }

    }

    // Draw simulation canvas (check)
    sf::RectangleShape canvas(sf::Vector2f(simulationWidth * scale, simulationHeight * scale));
    canvas.setPosition(offsetScaled);
    canvas.setFillColor(sf::Color::Transparent);
    canvas.setOutlineColor(sf::Color::Black);
    canvas.setOutlineThickness(1.0f);
    window.draw(canvas);

    // Draw sensor detection areas (check)
    for (const auto& sensorPtr : sensors) { // Iterate over all sensor pointers

        // Dereference the sensor pointer
        const Sensor& sensor = *sensorPtr;

        // Draw the detection area for each sensor
        sf::RectangleShape detectionAreaShape(sf::Vector2f(sensor.detectionArea.width * scale, sensor.detectionArea.height * scale));
        detectionAreaShape.setPosition(sensor.detectionArea.left * scale + offsetScaled.x, sensor.detectionArea.top * scale + offsetScaled.y);
        detectionAreaShape.setFillColor(sensor.detectionAreaColor); // Set the color with alpha
        window.draw(detectionAreaShape);
    }

    // Show simulation info (frame count, frame rate, agent count)
    if(showInfo) {

        // Draw frame count text
        window.draw(frameText);

        // Draw frame rate text
        window.draw(frameRateText);

        // Draw agent count text
        window.draw(agentCountText);

        // Draw time text
        window.draw(timeText);
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

// Function to update the text that displays the current frame count
void Simulation::updateFrameCountText(int frameCount) {

    frameText.setString("Frame: " + std::to_string(frameCount) + " / " + (maxFrames > 0 ? std::to_string(maxFrames) : "∞")); // ∞ for unlimited frames
    sf::FloatRect textRect = frameText.getLocalBounds();
    frameText.setOrigin(textRect.width, 0); // Right-align the text
    frameText.setPosition(window.getSize().x - 10, 40); // Position with padding
}

// Function to update the text that displays the current frame rate
void Simulation::updateFrameRateText(float frameRate) {

    frameRateText.setString("FPS: " + std::to_string(static_cast<int>(frameRate)));
    sf::FloatRect textRect = frameRateText.getLocalBounds();
    frameRateText.setOrigin(textRect.width, 0); // Right-align the text
    frameRateText.setPosition(window.getSize().x - 10, 70); // Position with padding
}

// Function to update the text that displays the current agent count
void Simulation::updateAgentCountText() {

    agentCountText.setString("Agents: " + std::to_string(agents.size()));
    sf::FloatRect textRect = agentCountText.getLocalBounds();
    agentCountText.setOrigin(textRect.width, 0); // Right-align the text
    agentCountText.setPosition(window.getSize().x - 10, 100); // Position with padding
}

// Function to update the text that displays the current elapsed time
void Simulation::updateTimeText() {

    // Convert seconds to HH:MM:SS for totalElapsedTime
    int elapsedHours = static_cast<int>(totalElapsedTime.asSeconds()) / 3600;
    int elapsedMinutes = (static_cast<int>(totalElapsedTime.asSeconds()) % 3600) / 60;
    int elapsedSeconds = static_cast<int>(totalElapsedTime.asSeconds()) % 60;
    int elapsedMilliseconds = static_cast<int>((totalElapsedTime.asSeconds() - static_cast<int>(totalElapsedTime.asSeconds())) * 1000);

    // Convert seconds to HH:MM:SS for durationSeconds (if not infinite)
    std::string durationString = "∞";  // Default to infinity symbol
    if (durationSeconds > 0) {
        int durationToHours = static_cast<int>(durationSeconds) / 3600;
        int durationToMinutes = (static_cast<int>(durationSeconds) % 3600) / 60;
        int durationToSeconds = static_cast<int>(durationSeconds) % 60;
        int durationToMilliseconds = static_cast<int>((durationSeconds - static_cast<int>(durationSeconds)) * 1000);

        // Format the duration string using stringstream
        std::ostringstream durationStream;
        durationStream << std::setfill('0') << std::setw(2) << durationToHours << ":"
                       << std::setw(2) << durationToMinutes << ":"
                       << std::setw(2) << durationToSeconds << ":"
                       << std::setw(3) << durationToMilliseconds;
        durationString = durationStream.str();
    }

    // Format the time string using stringstream
    std::ostringstream timeStream;
    timeStream << "Time: " << std::setfill('0') << std::setw(2) << elapsedHours << ":"
               << std::setw(2) << elapsedMinutes << ":"
               << std::setw(2) << elapsedSeconds << ":"
               << std::setw(3) << elapsedMilliseconds << " / " << durationString;
    timeText.setString(timeStream.str()); 

    // Right-align and position the text
    sf::FloatRect textRect = timeText.getLocalBounds();
    timeText.setOrigin(textRect.width, 0); 
    timeText.setPosition(window.getSize().x - 10, 10); 
}

// Function to handle events (mouse clicks, window close, etc.)
void Simulation::handleEvents(sf::Event event) {

    // Handle window close
    if (event.type == sf::Event::Closed) {
        window.close();
    
    // Handle key presses
    } else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            window.close();
        }

    // Handle window resizing
    } else if (event.type == sf::Event::Resized) {
        
        // Resize the window
        sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
        window.setView(sf::View(visibleArea));
        initializeUI(); // Reinitialize UI elements

        // Update the simulation offsets
        float newWidth = static_cast<float>(event.size.width) / static_cast<float>(scale);
        float newHeight = static_cast<float>(event.size.height) / static_cast<float>(scale);
        simulationHeightOffsetScaled = std::abs(newHeight - simulationHeight) / 2.0f;
        simulationWidthOffsetScaled = std::abs(newWidth - simulationWidth) / 2.0f;

    // Handle mouse clicks
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
    frameRate = 0.0f;
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

    // Update the agent velocity
    for (auto& agent : agents) {

        // Update the agent velocity using Perlin noise
        if(!agent.stopped) {

            // Update the agent velocity using Perlin noise
            agent.updateVelocity(deltaTime, totalElapsedTime);
        }
    }

    // Update agents
    for (auto agent = agents.begin(); agent != agents.end(); ) {

        // Check if agent is out of bounds TODO: make separate function
        if (agent->position.x > simulationWidth + agent->radius || agent->position.x < -agent->radius ||
            agent->position.y > simulationHeight + agent->radius || agent->position.y < -agent->radius) {
            
            // Remove the agent from the vector and update the iterator
            agent = agents.erase(agent); // Increases agent iterator if erased
        } 
        else {

            // Update agent position and velocity with perlin noise
            agent->updatePosition(deltaTime);

            // Reset collision state at the start of each frame for each agent
            agent->resetCollisionState(); 

            // Update timestamp for each agent
            agent->timestamp = generateISOTimestamp(totalElapsedTime, datetime);

            // Assign the agent to the correct grid cell
            grid.addAgent(&(*agent)); // Add agent to the grid

            // Resume agents if they are stopped
            if (agent->stopped) {

                ++(agent->stoppedFrameCounter);
                agent->resume(agents); 
            }
            // Check for agent-obstacle collisions
            agentObstaclesCollision(*agent, obstacles);

            // Move to the next agent
            ++agent; 
        }
    }

    // Update sensors and store sensor data
    for (auto& sensor : sensors) {

        sensor->update(agents, timeStep.asSeconds(), frameCount, totalElapsedTime, datetime);
        // sensor->printData();
        sensor->postData();
    }

    // Store ground truth data in MongoDB
    postData(agents);

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
void Simulation::postData(const std::vector<Agent>& agents) {

    // Check if there are agents to store
    if (!agents.empty()) {
    
        // Prepare a vector of documents to insert in bulk
        std::vector<bsoncxx::document::value> documents;
        documents.reserve(agents.size());

        // Iterate over each agent and prepare a document for MongoDB
        for (const auto& agent : agents) {

            // Construct a BSON document for each agent
            bsoncxx::builder::stream::document document{};

            // Append the agent data to the document
            document << "timestamp" << agent.timestamp
                        << "agent_id" << agent.uuid
                        << "type" << agent.type
                        << "position" 
                        << bsoncxx::builder::stream::open_array
                        << agent.position.x 
                        << agent.position.y
                        << bsoncxx::builder::stream::close_array
                        << "velocity" 
                        << bsoncxx::builder::stream::open_array
                        << agent.velocity.x 
                        << agent.velocity.y
                        << bsoncxx::builder::stream::close_array;

            documents.push_back(document << bsoncxx::builder::stream::finalize);
        }
        try {
            // Insert documents into MongoDB in bulk
            collection.insert_many(documents);
        } catch (const mongocxx::exception& e) {
            std::cerr << "An error occurred while inserting documents: " << e.what() << std::endl;
        }
    }
}

// Function to convert a string to sf::Color (case-insensitive)
sf::Color Simulation::stringToColor(std::string colorStr) {

    // Mapping of color names to sf::Color objects
    static const std::unordered_map<std::string, sf::Color> colorMap = {

        {"red", sf::Color::Red},
        {"green", sf::Color::Green},
        {"blue", sf::Color::Blue},
        {"black", sf::Color::Black},
        {"white", sf::Color::White},
        {"yellow", sf::Color::Yellow},
        {"magenta", sf::Color::Magenta},
        {"cyan", sf::Color::Cyan},
        {"pink", sf::Color(255, 192, 203)},
        {"brown", sf::Color(165, 42, 42)},
        {"turquoise", sf::Color(64, 224, 208)},
        {"gray", sf::Color(128, 128, 128)},
        {"purple", sf::Color(128, 0, 128)},
        {"violet", sf::Color(238, 130, 238)},
        {"orange", sf::Color(198, 81, 2)},
        {"indigo", sf::Color(75, 0, 130)},
        {"grey", sf::Color(128, 128, 128)}
    };

    // Convert to lowercase directly for faster comparison
    std::transform(colorStr.begin(), colorStr.end(), colorStr.begin(),
                [](unsigned char c){ return std::tolower(c); }); 

    // Fast lookup using a hash map
    auto it = colorMap.find(colorStr);
    if (it != colorMap.end()) {
        return it->second;
    }

    // Handle hex codes (same as your original implementation)
    if (colorStr.length() == 7 && colorStr[0] == '#') {
        int r, g, b;
        if (sscanf(colorStr.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
            return sf::Color(r, g, b);
        }
    }

    std::cerr << "Warning: Unrecognized color string '" << colorStr << "'. Using black instead." << std::endl;
    return sf::Color::Black;
}