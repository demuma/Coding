#include "../include/Renderer.hpp"
#include "../include/GridBasedSensor.hpp"
#include "../include/AgentBasedSensor.hpp"
#include "../include/AdaptiveGridBasedSensor.hpp"

// Renderer member functions
Renderer::Renderer(SharedBuffer<std::vector<Agent>>& buffer, 
    std::unordered_map<std::string, std::shared_ptr<SharedBuffer<std::shared_ptr<QuadtreeSnapshot::Node>>>> sensorBuffers, 
    std::atomic<float>& currentSimulationTimeStep, const YAML::Node& config)
:   buffer(buffer), sensorBuffers(sensorBuffers),
    currentSimulationTimeStep(currentSimulationTimeStep), config(config), // Reserve VRAM for the vertex arrays depending on the number of agents
    bufferZonesVertexArray(sf::PrimitiveType::Points), 
    agentBodyVertexArray(sf::PrimitiveType::Triangles, 6), // Note: SFML 2.6.2 or prior -> (Quads, 4)
    agentArrowHeadVertexArray(sf::PrimitiveType::Triangles, 3), 
    agentArrowBodyVertexArray(sf::PrimitiveType::Lines, 2),
    font(), frameText(font,"", 20), frameRateText(font, "", 24), agentCountText(font, "",  24), timeText(font, "", 24), 
    playbackSpeedText(font, "", 24), pauseButton(), pauseButtonText(font, "", 20), resetButton(), resetButtonText(font, "", 20) {

    DEBUG_MSG("Renderer: read buffer: " << buffer.currentReadFrameIndex);
    loadConfiguration();
    loadObstacles();
    initializeSensors();
    initializeWindow();
    initializeGUI();
}

// Function to load the configuration parameters -> TODO: Add error handling
void Renderer::loadConfiguration() {

    // Window parameters
    title = config["display"]["title"].as<std::string>();
    windowWidth = config["display"]["width"].as<int>();
    windowHeight = config["display"]["height"].as<int>();
    scale = config["display"]["pixels_per_meter"].as<float>();
    initialScale = scale;
    simulationWidth = (config["simulation"]["width"].as<int>() * scale) ;
    simulationHeight = (config["simulation"]["height"].as<int>() * scale);
    initialSimulationWidth = simulationWidth;
    initialSimulationHeight = simulationHeight;
    offset = sf::Vector2f((windowWidth - simulationWidth) / 2, (windowHeight - simulationHeight) / 2);
    DEBUG_MSG("Offset: " << offset.x << ", " << offset.y);

    // Renderer parameters
    timeStep = config["simulation"]["time_step"].as<float>();
    playbackSpeed = config["simulation"]["playback_speed"].as<float>();
    numAgents = config["agents"]["num_agents"].as<int>();
    currentNumAgents = numAgents;
    
    // Set the maximum number of frames if the duration is specified
    if(config["simulation"]["duration_seconds"]) {
        maxFrames = config["simulation"]["duration_seconds"].as<int>() / timeStep;
        targetRenderTime = config["simulation"]["duration_seconds"].as<int>();
    } else {
        maxFrames = config["simulation"]["maximum_frames"].as<int>();
        targetRenderTime = maxFrames * timeStep;
    }

    // Load display parameters
    showInfo = config["renderer"]["show_info"].as<bool>();
    showTrajectories = config["renderer"]["show_trajectories"].as<bool>();
    showObstacles = config["renderer"]["show_obstacles"].as<bool>();
    showCorridors = config["renderer"]["show_corridors"].as<bool>();
    showWaypoints = config["renderer"]["show_waypoints"].as<bool>();
    waypointRadius = config["agents"]["waypoint_radius"].as<float>();
    showGrids = config["renderer"]["show_grids"].as<bool>();
    showSensors = config["renderer"]["show_sensors"].as<bool>();
    showBufferZones = config["renderer"]["show_buffer"].as<bool>();
    showArrow = config["renderer"]["show_arrow"].as<bool>();

    // Load collision parameters
    showCollisionGrid = config["collision"]["grid"]["show_grid"].as<bool>();
    collisionGridCellSize = (config["collision"]["grid"]["cell_size"].as<float>());

    // Load font
    if (!font.openFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        ERROR_MSG("Error loading font\n");
    }

    // Set the frame rate buffer size
    frameRateBufferSize = 1.0f / timeStep;

    // Resize vertex arrays
    bufferZonesVertexArray.resize(numAgents * 300);
    agentBodyVertexArray.resize(numAgents * 6);
    agentArrowHeadVertexArray.resize(numAgents * 3);
    agentArrowBodyVertexArray.resize(numAgents * 2);
}

// Function to load agents attributes from the YAML configuration file
void Renderer::loadAgentsAttributes() {

    // Extract agent data
    if (config["agents"] && config["agents"]["road_user_taxonomy"]) {
        for (const auto& agent : config["agents"]["road_user_taxonomy"]) {
            std::string type = agent["type"].as<std::string>();
            Agent::AgentTypeAttributes attributes;

            // Load agent attributes from the configuration
            attributes.probability = agent["probability"].as<double>();
            attributes.priority = agent["priority"].as<int>();
            attributes.bodyRadius = agent["radius"].as<double>();
            attributes.color = agent["color"].as<std::string>();
            attributes.velocity.min = agent["velocity"]["min"].as<double>();
            attributes.velocity.max = agent["velocity"]["max"].as<double>();
            attributes.velocity.mu = agent["velocity"]["mu"].as<double>();
            attributes.velocity.sigma = agent["velocity"]["sigma"].as<double>();
            attributes.velocity.noiseScale = agent["velocity"]["noise_scale"].as<double>();
            attributes.velocity.noiseFactor = agent["velocity"]["noise_factor"].as<double>();
            attributes.acceleration.min = agent["acceleration"]["min"].as<double>();
            attributes.acceleration.max = agent["acceleration"]["max"].as<double>();
            attributes.lookAheadTime = agent["look_ahead_time"].as<double>();

            // Store in map
            agentTypeAttributes[type] = attributes;
        }
    }
}

// Function to load obstacles from the YAML configuration file
void Renderer::loadObstacles() {

    // Check if the 'obstacles' key exists and is a sequence
    int i = 0;
    if (config["obstacles"] && config["obstacles"].IsSequence()) {
        for (const auto& obstacleNode : config["obstacles"]) {
            std::string type = obstacleNode["type"] && obstacleNode["type"].IsScalar()
                ? obstacleNode["type"].as<std::string>() : "unknown";

            if (type == "rectangle") { // Only handle rectangles
                std::vector<float> position = obstacleNode["position"].as<std::vector<float>>();
                std::vector<float> size = obstacleNode["size"].as<std::vector<float>>();
                obstacles.push_back(Obstacle(
                    sf::FloatRect({position[0], position[1]}, {size[0], size[1]}), 
                    stringToColor(obstacleNode["color"].as<std::string>())
                ));
            } else {
                ERROR_MSG("Error: Unknown obstacle type in config file at position " << i);
            }

            ++i;
        }
    } else {
        ERROR_MSG("Error: Could not find 'obstacles' key in config file or it is not a sequence");
    }
}

// Initialize sensors with YAML configuration
void Renderer::initializeSensors() {

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

            {
                sensorNode["detection_area"]["x"].as<float>(),
                sensorNode["detection_area"]["y"].as<float>()
            },
            {
                sensorNode["detection_area"]["width"].as<float>(),
                sensorNode["detection_area"]["height"].as<float>()
            }
        );

        // Create the agent-based sensor
        if (type == "agent-based") {
            
            // Create the agent-based sensor and add to sensors vector
            sensors.push_back(std::make_unique<AgentBasedSensor>(detectionArea, colorAlpha));
            sensors.back()->scale = scale;


        // Create the grid-based sensor
        } else if (type == "grid-based") {

            float cellSize = sensorNode["grid"]["cell_size"].as<float>();
            bool showSensorGrid = sensorNode["grid"]["show_grid"].as<bool>();

            // Create the grid-based sensor and add to sensors vector
            sensors.push_back(std::make_unique<GridBasedSensor>(detectionArea, colorAlpha, cellSize, showSensorGrid));
            sensors.back()->scale = scale;
        } else if (type == "adaptive-grid-based") {

            float cellSize = sensorNode["grid"]["cell_size"].as<float>();
            bool showSensorGrid = sensorNode["grid"]["show_grid"].as<bool>();
            int maxDepth = sensorNode["grid"]["max_depth"].as<int>();

            // Create the grid-based sensor and add to sensors vector
            sensors.push_back(std::make_unique<AdaptiveGridBasedSensor>(detectionArea, colorAlpha, cellSize, maxDepth, showSensorGrid));
            sensors.back()->scale = scale;
        }
    }
}

// Initialize the window
void Renderer::initializeWindow() {

    // Turn on antialiasing
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 16;

    // Create window
    // window.create(sf::VideoMode(windowWidth, windowHeight), title, sf::Style::Default, settings); // Note: SFML 2.6.2 or prior
    window.create(sf::VideoMode({windowWidth, windowHeight}), title, sf::Style::Default, sf::State::Windowed, settings);
}

// Initialize the GUI elements
void Renderer::initializeGUI() {

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

    // Frame count text
    playbackSpeedText.setFont(font);
    playbackSpeedText.setCharacterSize(24);
    playbackSpeedText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop
    
    // Pause button
    pauseButton.setSize(sf::Vector2f(100, 50)); 
    pauseButton.setFillColor(sf::Color::Green);
    pauseButton.setPosition({static_cast<float>(window.getSize().x) - 110, static_cast<float>(window.getSize().y) - 60});

    pauseButtonText.setFont(font);
    pauseButtonText.setString("Pause");
    pauseButtonText.setCharacterSize(20);
    pauseButtonText.setFillColor(sf::Color::Black);
    
    // Center the pause button text
    sf::FloatRect pauseButtonTextRect = pauseButtonText.getLocalBounds();
    pauseButtonText.setOrigin({pauseButtonTextRect.position.x + pauseButtonTextRect.size.x / 2.0f,
                              pauseButtonTextRect.position.y + pauseButtonTextRect.size.y / 2.0f});
    pauseButtonText.setPosition({pauseButton.getPosition().x + pauseButton.getSize().x / 2.0f,
                                pauseButton.getPosition().y + pauseButton.getSize().y / 2.0f});

    // Reset button
    resetButton.setSize(sf::Vector2f(100, 50));
    resetButton.setFillColor(sf::Color::Cyan);
    resetButton.setPosition({static_cast<float>(window.getSize().x) - 110, static_cast<float>(window.getSize().y) - 120}); 

    resetButtonText.setFont(font);
    resetButtonText.setString("Infos");
    resetButtonText.setCharacterSize(20);
    resetButtonText.setFillColor(sf::Color::Black);

    // Center the reset button text
    sf::FloatRect resetButtonTextRect = resetButtonText.getLocalBounds();
    resetButtonText.setOrigin({resetButtonTextRect.position.x + resetButtonTextRect.size.x / 2.0f,
                              resetButtonTextRect.position.y + resetButtonTextRect.size.y / 2.0f});
    resetButtonText.setPosition({resetButton.getPosition().x + resetButton.getSize().x / 2.0f,
                                resetButton.getPosition().y + resetButton.getSize().y / 2.0f});

}

// Function to update the text that displays the current frame count
void Renderer::updateFrameCountText() {

    frameText.setString("Frame " + std::to_string(buffer.currentReadFrameIndex) + " / " + (maxFrames > 0 ? std::to_string(maxFrames) : "∞")); // ∞ for unlimited frames
    sf::FloatRect textRect = frameText.getLocalBounds();
    frameText.setOrigin({textRect.size.x, 0}); // Right-align the text
    frameText.setPosition({static_cast<float>(window.getSize().x) - 10, 40}); // Position with padding
}

// Function to update the text that displays the current agent count
void Renderer::updateAgentCountText() {

    agentCountText.setString("Agents: " + std::to_string(currentNumAgents));
    sf::FloatRect textRect = agentCountText.getLocalBounds();
    agentCountText.setOrigin({textRect.size.x, 0}); // Right-align the text
    agentCountText.setPosition({static_cast<float>(window.getSize().x) - 6, 100}); // Position with padding
}

// Function to update the text that displays the current elapsed time
void Renderer::updateTimeText() {

    // Convert seconds to HH:MM:SS for totalElapsedTime
    // auto totalElapsedTime = rendererClock.getElapsedTime();
    auto totalElapsedTime = renderSimulationTime;
    int elapsedHours = static_cast<int>(totalElapsedTime.asSeconds()) / 3600;
    int elapsedMinutes = (static_cast<int>(totalElapsedTime.asSeconds()) % 3600) / 60;
    int elapsedSeconds = static_cast<int>(totalElapsedTime.asSeconds()) % 60;
    int elapsedMilliseconds = (totalElapsedTime.asSeconds() - static_cast<int>(totalElapsedTime.asSeconds())) * 1000;

    std::string targetRenderTimeString = "∞";  // Default to infinity symbol
    if(targetRenderTime > 0) {
        int targetHours = static_cast<int>(targetRenderTime) / 3600;
        int targetMinutes = (static_cast<int>(targetRenderTime)) % 3600 / 60;
        int targetSeconds = static_cast<int>(targetRenderTime) % 60;
        int targetMilliseconds = (targetRenderTime - static_cast<int>(targetRenderTime)) * 1000;

        // Format the duration string using stringstream
        std::ostringstream targetRenderTimeStream;
        targetRenderTimeStream << std::setfill('0') << std::setw(2) << targetHours << ":"
                       << std::setw(2) << targetMinutes << ":"
                       << std::setw(2) << targetSeconds << ":"
                       << std::setw(3) << targetMilliseconds;
        targetRenderTimeString = targetRenderTimeStream.str();
    }

    // Format the time string using stringstream
    std::ostringstream timeStream;
    timeStream << "Time: " << std::setfill('0') << std::setw(2) << elapsedHours << ":"
               << std::setw(2) << elapsedMinutes << ":"
               << std::setw(2) << elapsedSeconds << ":"
               << std::setw(3) << elapsedMilliseconds << " / " << targetRenderTimeString;
    timeText.setString(timeStream.str()); 

    // Right-align and position the text
    sf::FloatRect textRect = timeText.getLocalBounds();
    timeText.setOrigin({textRect.size.x, 0}); 
    timeText.setPosition({static_cast<float>(window.getSize().x) - 10, 10}); 
}

// Function to calculate the moving average frame rate and update the text after warmup
void Renderer::updateFrameRateText() {

    // Wait for the warmup frames to pass
    if(buffer.currentReadFrameIndex < warmupFrames) {
        return;
    }
    
    // Wait for frame buffer to fill up
    if(frameRates.size() != frameRateBufferSize) {
        frameRates.push_back(frameRate);
        
    } else {

        // Calculate the moving average frame rate
        float sum = std::accumulate(frameRates.begin(), frameRates.end(), 0.0f);
        movingAverageFrameRate = sum / frameRates.size();
        
        // Update the frame rate text
        frameRateText.setString("FPS: " + std::to_string(static_cast<int>(movingAverageFrameRate)));
        sf::FloatRect textRect = frameRateText.getLocalBounds();
        frameRateText.setOrigin({textRect.size.x, 0}); // Right-align the text
        frameRateText.setPosition({static_cast<float>(window.getSize().x) - 10, 70}); // Position with padding

        // Update the frame rate buffer
        frameRates.pop_front();
        frameRates.push_back(frameRate);
    }
}

// Function to update the text that displays the current frame count
void Renderer::updatePlayBackSpeedText() {

    std::ostringstream playbackSpeedStream;
    playbackSpeedStream << std::fixed << std::setprecision(1) << "Playback Speed: " << playbackSpeed << std::setw(2) << "x";

    // playbackSpeedText.setString("Playback Speed: " + std::to_string(playbackSpeed));
    playbackSpeedText.setString(playbackSpeedStream.str());
    sf::FloatRect textRect = playbackSpeedText.getLocalBounds();
    playbackSpeedText.setOrigin({textRect.size.x, 0}); // Right-align the text
    playbackSpeedText.setPosition({static_cast<float>(window.getSize().x) - 6, 130}); // Position with padding
}

// Function to handle events
// void Renderer::handleEvents(sf::Event& event) { // Note: SFML 2.6.2 or prior
// void Renderer::handleEvents(const std::__1::optional<sf::Event> event) {

//     // if (event.type == sf::Event::Closed) {
//     if(event->is<sf::Event::Closed>()) {
//         window.close();
//         buffer.stop.store(true);
//         return;
//     }
//     // else if (event.type == sf::Event::Resized) {
//     else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
        
//         // Resize the window
//         // sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height); // Note: SFML 2.6.2 or prior
//         sf::FloatRect visibleArea({0, 0}, {resized->size.x, resized->size.y});
//         window.setView(sf::View(visibleArea));
//         initializeGUI(); // Reinitialize UI elements

//         // Update the simulation offsets
//         // windowWidth = event.size.width;
//         // windowHeight = event.size.height;
//         windowWidth = resized->size.x;
//         windowHeight = resized->size.y;
//         offset = sf::Vector2f((windowWidth - simulationWidth) / 2, (windowHeight - simulationHeight) / 2);
//         return;
//     }
//     else if (event.type == sf::Event::KeyReleased) {
//         if(event.key.code == sf::Keyboard::Scancode::LShift || event.key.code == sf::Keyboard::Scancode::RShift) {
//             isShiftPressed = false;
//             DEBUG_MSG("Shift released");
//             return;
//         }
//         else if(event.key.code == sf::Keyboard::Scancode::LControl || event.key.code == sf::Keyboard::Scancode::RControl) {
//             isCtrlPressed = false;
//             DEBUG_MSG("Ctrl released");
//             return;
//         }
//     } 
//     else if (event.type == sf::Event::KeyPressed) {
//         if (event.key.code == sf::Keyboard::Scancode::Space) {
//             paused = !paused;
//             pauseButtonText.setString(paused ? "  Play" : "Pause");
//             pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green);
//             window.draw(pauseButton); // NEW
//             window.draw(pauseButtonText); // NEW
//             window.display(); // NEW
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::LShift || event.key.code == sf::Keyboard::Scancode::RShift) {
//             isShiftPressed = true;
//             DEBUG_MSG("Shift pressed");
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::LControl || event.key.code == sf::Keyboard::Scancode::RControl) {
//             isCtrlPressed = true;
//             DEBUG_MSG("Ctrl pressed");
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::Left) {
//             playbackSpeed -= 0.1f;
//             DEBUG_MSG("Playback speed decreased to: " << playbackSpeed);
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::Right) {
//             playbackSpeed += 0.1f;
//             DEBUG_MSG("Playback speed increased to: " << playbackSpeed);
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::A) {
//             showArrow = !showArrow;
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::B) {
//             showBufferZones = !showBufferZones;
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::C) {
//             showCollisionGrid = !showCollisionGrid;
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::O) {
//             showObstacles = !showObstacles;
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::R) {
//             playbackSpeed = 1.0f;
//             DEBUG_MSG("Playback speed reset to: " << playbackSpeed);
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::S) {
//             showSensors = !showSensors;
//             showSensorGrid = !showSensorGrid;
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::T) {
//             showTrajectories = !showTrajectories;
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::W) {
//             showWaypoints = !showWaypoints;
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::Q) {
//             window.close();
//             buffer.stop.store(true);
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::Z) {
//             scale = initialScale;
//             simulationHeight = initialSimulationHeight;
//             simulationWidth = initialSimulationWidth;
//             offset.x = (windowWidth - initialSimulationWidth) / 2;
//             offset.y = (windowHeight - initialSimulationHeight) / 2;
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::Escape) {
//             window.close();
//             buffer.stop.store(true);
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::Up) {
//             if (playbackSpeed >= 1.0f) {
//                 playbackSpeed += 1.0f;
//                 playbackSpeed = std::floor(playbackSpeed);
//             } else if (playbackSpeed < 1.0f && playbackSpeed > 0.1f) {
//                 playbackSpeed += 0.1f;
//             } else {
//                 playbackSpeed = 0.1f;
//             }
//             DEBUG_MSG("Playback speed increased to: " << playbackSpeed);
//             return;
//         }
//         else if (event.key.code == sf::Keyboard::Scancode::Down) {
//             if (playbackSpeed > 1.0f) {
//                 playbackSpeed -= 1.0f;
//                 playbackSpeed = std::ceil(playbackSpeed);
//             } else if (playbackSpeed <= 1.0f && playbackSpeed > 0.1f) {
//                 playbackSpeed -= 0.1f;
//             } else {
//                 playbackSpeed = 0.1f;
//             }
//             DEBUG_MSG("Playback speed decreased to: " << playbackSpeed);
//             return;
//         }
//     } else if (event.type == sf::Event::MouseButtonPressed) {
//         if (event.mouseButton.button == sf::Mouse::Left) {
//             sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
//             if (pauseButton.getGlobalBounds().contains(mousePosition.x, mousePosition.y)) {
//                 paused = !paused;
//                 pauseButtonText.setString(paused ? "  Play" : "Pause");
//                 pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green);
//                 window.draw(pauseButton); // NEW
//                 window.draw(pauseButtonText); // NEW
//                 window.display(); // NEW
//             } else if (resetButton.getGlobalBounds().contains(mousePosition.x, mousePosition.y)) {
//                 showInfo = !showInfo;
//             }
//         }
//         return;
//     } else if (event.type == sf::Event::MouseWheelScrolled) {
//         if(isShiftPressed) {
//             if (event.mouseWheelScroll.delta > 0) {
//                 offset.x -= 20;
//             } else if (event.mouseWheelScroll.delta < 0) {
//                 offset.x += 20;
//             }
//             return;
//         }
//         else if(isCtrlPressed) {
//             if (event.mouseWheelScroll.delta > 0) {
//                 offset.y -= 20;
//             } else if (event.mouseWheelScroll.delta < 0) {
//                 offset.y += 20;
//             }
//             return;
//         }
//         else if (event.mouseWheelScroll.delta > 0) {
//             int previousWidth = simulationWidth;
//             int previousHeight = simulationHeight;
//             simulationHeight /= scale;
//             simulationWidth /= scale;
//             scale += 1.0f;
//             simulationHeight *= scale;
//             simulationWidth *= scale;
//             offset.x = offset.x - (simulationWidth - previousWidth) / 2;
//             offset.y = offset.y - (simulationHeight - previousHeight) / 2;
//             return;
//         } 
//         else if (event.mouseWheelScroll.delta < 0) {
//             int previousWidth = simulationWidth;
//             int previousHeight = simulationHeight;
//             simulationHeight /= scale;
//             simulationWidth /= scale;
//             if(scale != 1.0f) scale -= 1.0f;
//             simulationHeight *= scale;
//             simulationWidth *= scale;
//             offset.x = offset.x - (simulationWidth - previousWidth) / 2;
//             offset.y = offset.y - (simulationHeight - previousHeight) / 2;
//             return;
//         }
//     }
// }

// Function to handle events
void Renderer::handleEvents() {

    // Lambda to handle window close events.
    const auto onClose = [this](const sf::Event::Closed&) {
        window.close();
        buffer.stop.store(true);
    };

    // Lambda to handle window resize events.
    const auto onResized = [this](const sf::Event::Resized& resized) {
        sf::FloatRect visibleArea({0, 0}, {static_cast<float>(resized.size.x), static_cast<float>(resized.size.y)});
        window.setView(sf::View(visibleArea));
        initializeGUI(); // Reinitialize UI elements

        // Update simulation dimensions and offset.
        windowWidth  = static_cast<float>(resized.size.x);
        windowHeight = static_cast<float>(resized.size.y);
        offset = sf::Vector2f((windowWidth - simulationWidth) / 2,
                              (windowHeight - simulationHeight) / 2);
    };

    // Lambda to handle key pressed events.
    const auto onKeyPressed = [this](const sf::Event::KeyPressed& keyPressed) {
        // Toggle pause with Space.
        if (keyPressed.scancode == sf::Keyboard::Scancode::Space) {
            paused = !paused;
            pauseButtonText.setString(paused ? "  Play" : "Pause");
            pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green);
            window.draw(pauseButton);
            window.draw(pauseButtonText);
            window.display();
        }
        // Set modifier flags.
        else if (keyPressed.scancode == sf::Keyboard::Scancode::LShift ||
                 keyPressed.scancode == sf::Keyboard::Scancode::RShift) {
            isShiftPressed = true;
        }
        else if (keyPressed.scancode == sf::Keyboard::Scancode::LControl ||
                 keyPressed.scancode == sf::Keyboard::Scancode::RControl) {
            isCtrlPressed = true;
        }
        // Adjust playback speed.
        else if (keyPressed.scancode == sf::Keyboard::Scancode::Left) {
            playbackSpeed -= 0.1f;
            DEBUG_MSG("Playback speed decreased to: " << playbackSpeed);
        }
        else if (keyPressed.scancode == sf::Keyboard::Scancode::Right) {
            playbackSpeed += 0.1f;
            DEBUG_MSG("Playback speed increaased to: " << playbackSpeed);
        }
        
        else if (keyPressed.scancode == sf::Keyboard::Scancode::A) {
            showArrow = !showArrow;
        }

        else if (keyPressed.scancode == sf::Keyboard::Scancode::B) {
            showBufferZones = !showBufferZones;
        }

        else if (keyPressed.scancode == sf::Keyboard::Scancode::C) {
            showCollisionGrid = !showCollisionGrid;
        }

        else if (keyPressed.scancode == sf::Keyboard::Scancode::O) {
            showObstacles = !showObstacles;
        }
        
        else if (keyPressed.scancode == sf::Keyboard::Scancode::Q) {
            window.close();
            buffer.stop.store(true);
        }

        else if (keyPressed.scancode == sf::Keyboard::Scancode::R) {
            playbackSpeed = 1.0f;
            DEBUG_MSG("Playback speed reset to: " << playbackSpeed);
        }

        else if (keyPressed.scancode == sf::Keyboard::Scancode::S) {
            showSensors = !showSensors;
            showSensorGrid = !showSensorGrid;
        }

        else if (keyPressed.scancode == sf::Keyboard::Scancode::T) {
            showTrajectories = !showTrajectories;
        }
        
        else if (keyPressed.scancode == sf::Keyboard::Scancode::W) {
            showWaypoints = !showWaypoints;
        }

        else if (keyPressed.scancode == sf::Keyboard::Scancode::Z) {
            scale = initialScale;
            simulationHeight = initialSimulationHeight;
            simulationWidth = initialSimulationWidth;
            offset.x = (windowWidth - initialSimulationWidth) / 2;
            offset.y = (windowHeight - initialSimulationHeight) / 2;
            
        }

        else if (keyPressed.scancode == sf::Keyboard::Scancode::Escape) {
            window.close();
            buffer.stop.store(true);
        }
    };

    // Lambda to handle key released events.
    const auto onKeyReleased = [this](const sf::Event::KeyReleased& keyReleased) {
        if (keyReleased.scancode == sf::Keyboard::Scancode::LShift ||
            keyReleased.scancode == sf::Keyboard::Scancode::RShift) {
            isShiftPressed = false;
        }
        else if (keyReleased.scancode == sf::Keyboard::Scancode::LControl ||
                 keyReleased.scancode == sf::Keyboard::Scancode::RControl) {
            isCtrlPressed = false;
        }
    };

    // Lambda to handle mouse button pressed events.
    const auto onMouseButtonPressed = [this](const sf::Event::MouseButtonPressed& mouseButton) {
        if (mouseButton.button == sf::Mouse::Button::Left) {
            // Get mouse position relative to the window.
            sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
            if (!pauseButton.getGlobalBounds().contains({static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y)}) &&
                !resetButton.getGlobalBounds().contains({static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y)})) {
                isPanning = true;
                lastMousePosition = {static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y)};
            }
            else { 
                if (pauseButton.getGlobalBounds().contains({static_cast<float>(mousePosition.x),
                    static_cast<float>(mousePosition.y)})) {
                    paused = !paused;
                    pauseButtonText.setString(paused ? "  Play" : "Pause");
                    pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green);
                    window.draw(pauseButton);
                    window.draw(pauseButtonText);
                    window.display();
                }
                else if (resetButton.getGlobalBounds().contains({static_cast<float>(mousePosition.x),
                                                                static_cast<float>(mousePosition.y)})) {
                    showInfo = !showInfo;
                }
            }
        }
    };

    // Mouse released
    const auto onMouseButtonReleased = [this](const sf::Event::MouseButtonReleased& mouseButton) {
        if (mouseButton.button == sf::Mouse::Button::Left) {
            isPanning = false; // Stop dragging
        }
    };

    const auto onMouseMoved = [this](const sf::Event::MouseMoved& mouseMoved) {
        if (isPanning) {
            // Current mouse position
            sf::Vector2f currentPosition(static_cast<float>(mouseMoved.position.x), static_cast<float>(mouseMoved.position.y));
    
            // Compute the difference
            sf::Vector2f delta = currentPosition - lastMousePosition;
    
            // Update offset by that difference
            offset += delta;
    
            // Update lastMousePos for next movement
            lastMousePosition = currentPosition;
        }
    };

    // Lambda to handle mouse wheel scroll events.
    const auto onMouseWheelScrolled = [this](const sf::Event::MouseWheelScrolled& scroll) {
        if (isShiftPressed) {
            if (scroll.delta > 0) {
                offset.x -= 20;
            } else if (scroll.delta < 0) {
                offset.x += 20;
            }
        }
        else if (isCtrlPressed) {
            if (scroll.delta > 0) {
                offset.y -= 20;
            } else if (scroll.delta < 0) {
                offset.y += 20;
            }
        }
        else {
            int previousWidth = simulationWidth;
            int previousHeight = simulationHeight;
            simulationHeight /= scale;
            simulationWidth /= scale;
            if (scroll.delta > 0) {
                scale += 1.0f;
            } else if (scroll.delta < 0) {
                if (scale != 1.0f) scale -= 1.0f;
            }
            simulationHeight *= scale;
            simulationWidth *= scale;
            offset.x = offset.x - (simulationWidth - previousWidth) / 2;
            offset.y = offset.y - (simulationHeight - previousHeight) / 2;
        }
    };

    // Now pass all these lambdas to the window's handleEvents function.
    window.handleEvents(onClose,
                        onResized,
                        onKeyPressed,
                        onKeyReleased,
                        onMouseButtonPressed,
                        onMouseButtonReleased,
                        onMouseMoved,
                        onMouseWheelScrolled);
}

// Main rendering loop
void Renderer::run() {

    // Set the target frame rate
    targetFrameRate = playbackSpeed * (1.0f / timeStep);
    targetFrameTime = sf::seconds(1.0f / targetFrameRate);

    // Initialize the event
    // sf::Event event;

    // Timing variables
    sf::Clock rendererFrameClock;
    
    // Start the clock
    rendererClock.restart();
    rendererRealTime = sf::Time::Zero;
    renderSimulationTime = sf::Time::Zero;
    sf::Time timeStepTime = sf::seconds(timeStep);
    sf::Time rendererTotalFrameTime; // Total time taken to render the frame

    // Frame rate calculation variables
    sf::Clock frameTimeClock;
    float rendererFrameRate;

    // Statistics
    float writeBufferTime = 0.f;
    float readBufferTime = 0.f;
    float eventHandlingTime = 0.f;

    // Add the setup time to the frame rate buffer
    rendererRealTime += rendererClock.restart();

    // Main rendering loop
    while (window.isOpen() && buffer.currentReadFrameIndex < maxFrames) {

        // Reset the frame time clock
        rendererFrameClock.restart();

        // Handle events
        // while (const std::optional event = window.pollEvent()) {
        //     handleEvents(event);
        // }
        handleEvents();

        // Polling mouse position for panning when dragging mouse
        if (isPanning){
            sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
            sf::Vector2f currentMousePosition(static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y));
            sf::Vector2f delta = currentMousePosition - lastMousePosition;
            offset += delta;
            lastMousePosition = currentMousePosition;
        }

        // Get the time taken to handle events
        eventHandlingTime = rendererFrameClock.getElapsedTime().asSeconds();

        // Run the renderer if not paused
        if(!paused) {

            // Read the current frame from the buffer
            currentFrame = buffer.read();
            pauseFrame = currentFrame;

            // Get the time taken to read the buffer
            readBufferTime += rendererFrameClock.getElapsedTime().asSeconds() - eventHandlingTime;

            // Get the current simulation time step
            currentSimulationFrameTime = sf::seconds(currentSimulationTimeStep.load());

            // Update the text elements
            updateAgentCountText();
            updateFrameCountText();
            updateTimeText();
            updateFrameRateText();
            updatePlayBackSpeedText();

            // Render
            render();

            // Get the time taken to render the frame
            rendererFrameTime = rendererFrameClock.getElapsedTime();

            // Calculate the sleep time
            auto sleepTime = calculateSleepTime();
            std::this_thread::sleep_for(std::chrono::duration<float>(sleepTime.asSeconds()));

            // Get the total time taken to render the frame
            rendererTotalFrameTime = rendererFrameClock.getElapsedTime();
            rendererFrameRate = 1.0f / rendererTotalFrameTime.asSeconds();
            frameRate = rendererFrameRate;

            // Update the render time
            rendererRealTime += rendererTotalFrameTime;
            renderSimulationTime += timeStepTime;
        }

        else {

            // Replay last frame and sleep time in paused state
            currentFrame = pauseFrame;
            render();
            std::this_thread::sleep_for(std::chrono::duration<float>(pauseSleepTime.asSeconds()));
        }
    }
    STATS_MSG("Total render wall time: " << rendererRealTime.asSeconds() << " seconds for " << buffer.currentReadFrameIndex << " frames");
    STATS_MSG("Total render time: " << targetRenderTime << " seconds" << " for " << numAgents << " agents");
    STATS_MSG("Render speedup: " << targetRenderTime / rendererRealTime.asSeconds());
    STATS_MSG("Average frame rate: " << 1/(rendererRealTime.asSeconds() / (buffer.currentReadFrameIndex + 1)));
    STATS_MSG("Average render frame time: " << rendererRealTime.asSeconds() / (buffer.currentReadFrameIndex + 1));
    STATS_MSG("Average read buffer time: " << readBufferTime / (buffer.currentReadFrameIndex + 1));
}

// Function to calculate the sleep time for each frame
sf::Time Renderer::calculateSleepTime() {

    // Calculate the target frame rate based on the playback speed
    targetFrameRate = playbackSpeed * (1.0f / timeStep);
    targetFrameTime = sf::seconds(1.0f / targetFrameRate);

    // Check whether target frame time with the current playback speed is greater than the current frame time
    if(targetFrameTime < currentSimulationFrameTime){
        targetFrameTime = currentSimulationFrameTime;
        playbackSpeed = timeStep / currentSimulationFrameTime.asSeconds();
        DEBUG_MSG("Playback speed adjusted to: " << playbackSpeed);
        
    }

    if (rendererFrameTime >= targetFrameTime) {
        // The rendering frame time is already slower than the target frame rate; do not sleep.
        return sf::Time::Zero;
    }

    if (currentSimulationFrameTime >= targetFrameTime) {
        // Simulation frame time is greater than or equal to target rendering time.
        return sf::Time::Zero;  // No sleep needed, as we can't render faster than the simulation allows.
    }

    // Calculate the remaining time to meet the target rendering frame time
    sf::Time remainingTime = targetFrameTime - rendererFrameTime;

    // Ensure the sleep time is non-negative
    return std::max(remainingTime, sf::Time::Zero);
}

// Main rendering function
void Renderer::render() {  // Input: meters, Output: pixels

    // Clear the window
    window.clear(sf::Color::White);

    // Get the current number of agents
    currentNumAgents = currentFrame.size();

    // Check if the frame is empty
    if (currentNumAgents == 0) {
            ++frameEmptyCount;
            ERROR_MSG("Frame is empty: " << frameEmptyCount);
    }

    // Loop through all agents and render them
    if (currentNumAgents != 0 || buffer.currentReadFrameIndex != maxFrames) {

        // Draw the grid if enabled
        if(showGrids) {
            
            // Draw the collision grid
            if(showCollisionGrid) {

                // Draw outer rectangle
                sf::RectangleShape gridBoundaries(sf::Vector2f(simulationWidth, simulationHeight));
                gridBoundaries.setPosition(offset); // Set position in pixels
                gridBoundaries.setFillColor(sf::Color::Transparent);
                gridBoundaries.setOutlineColor(sf::Color::Red);
                gridBoundaries.setOutlineThickness(1);
                window.draw(gridBoundaries);

                // Draw vertical lines
                for (int x = 0; x <= (simulationWidth / (collisionGridCellSize * scale)); x++) {
                    sf::Vertex line[] = {
                        sf::Vertex({sf::Vector2f((x * collisionGridCellSize * scale), 0) + offset, sf::Color(220, 220, 220)}),
                        sf::Vertex({sf::Vector2f((x * collisionGridCellSize * scale), simulationHeight) + offset, sf::Color(220, 220, 220)})
                    };
                    window.draw(line, 2, sf::PrimitiveType::Lines);
                }

                // Draw horizontal lines
                for (int x = 0; x <= (simulationHeight / (collisionGridCellSize * scale)); x++) {
                    sf::Vertex line[] = {
                        sf::Vertex({sf::Vector2f(0, (x * collisionGridCellSize * scale)) + offset, sf::Color(220, 220, 220)}),
                        sf::Vertex({sf::Vector2f(simulationWidth, (x * collisionGridCellSize * scale)) + offset, sf::Color(220, 220, 220)})
                    };
                    window.draw(line, 2, sf::PrimitiveType::Lines);
                }
            }

            // Loop through sensors pointer
            for (const auto& sensor : sensors) {

                // Check if the sensor is grid-based
                if (auto gridBasedSensor = dynamic_cast<GridBasedSensor*>(sensor.get())) {

                    // Draw the grid if enabled
                    if(gridBasedSensor->showGrid && showSensors) {

                        // Draw outer rectangle
                        sf::RectangleShape gridBoundaries(sf::Vector2f(gridBasedSensor->detectionArea.size.x * scale, gridBasedSensor->detectionArea.size.y * scale));
                        gridBoundaries.setPosition({gridBasedSensor->detectionArea.position.x * scale + offset.x, gridBasedSensor->detectionArea.position.y * scale + offset.y}); // Set position in pixels
                        // gridBoundaries.setFillColor(sf::Color::Transparent);
                        gridBoundaries.setFillColor(gridBasedSensor->detectionAreaColor);
                        gridBoundaries.setOutlineColor(sf::Color::Black);
                        gridBoundaries.setOutlineThickness(1);
                        window.draw(gridBoundaries);

                        // Draw vertical lines
                        for (int x = 0; x <= (gridBasedSensor->detectionArea.size.x / gridBasedSensor->cellSize); x++) {
                            sf::Vertex line[] = {
                                sf::Vertex({sf::Vector2f((gridBasedSensor->detectionArea.position.x + x * gridBasedSensor->cellSize) * scale, gridBasedSensor->detectionArea.position.y * scale) + offset, sf::Color(220, 220, 220)}),
                                sf::Vertex({sf::Vector2f((gridBasedSensor->detectionArea.position.x + x * gridBasedSensor->cellSize) * scale, (gridBasedSensor->detectionArea.size.y + gridBasedSensor->detectionArea.position.y) * scale) + offset, sf::Color(220, 220, 220)})
                            };
                            window.draw(line, 2, sf::PrimitiveType::Lines);
                        }

                        // Draw horizontal lines
                        for (int x = 0; x <= (gridBasedSensor->detectionArea.size.y / gridBasedSensor->cellSize); x++) {
                            sf::Vertex line[] = {
                                sf::Vertex({sf::Vector2f((gridBasedSensor->detectionArea.position.x) * scale, (gridBasedSensor->detectionArea.position.y + x * gridBasedSensor->cellSize) * scale) + offset, sf::Color(220, 220, 220)}),
                                sf::Vertex({sf::Vector2f((gridBasedSensor->detectionArea.position.x + gridBasedSensor->detectionArea.size.x) * scale, (gridBasedSensor->detectionArea.position.y + x * gridBasedSensor->cellSize) * scale) + offset, sf::Color(220, 220, 220)})
                            };
                            window.draw(line, 2, sf::PrimitiveType::Lines);
                        }
                    }
                }

                if(auto adaptiveGridBasedSensor = dynamic_cast<AdaptiveGridBasedSensor*>(sensor.get())) {

                    // Draw the grid if enabled
                    if(adaptiveGridBasedSensor->showGrid && showSensors) {

                        // Draw outer rectangle
                        // sf::RectangleShape gridBoundaries(sf::Vector2f(adaptiveGridBasedSensor->detectionArea.size.x * scale, adaptiveGridBasedSensor->detectionArea.height * scale));
                        // gridBoundaries.setPosition(adaptiveGridBasedSensor->detectionArea.position.x * scale + offset.x, adaptiveGridBasedSensor->detectionArea.top * scale + offset.y); // Set position in pixels
                        // gridBoundaries.setFillColor(sf::Color::Transparent);
                        // gridBoundaries.setOutlineColor(sf::Color::Black);
                        // gridBoundaries.setOutlineThickness(1);
                        // window.draw(gridBoundaries);

                        // Draw the adaptive grid with scaling and offset
                        adaptiveGridBasedSensor->adaptiveGrid.draw(window, font, scale, offset);
                        // adaptiveGridBasedSensor->adaptiveGrid.printChildren(12);
                        // adaptiveGridBasedSensor->adaptiveGrid.printChildren(13);
                        // adaptiveGridBasedSensor->adaptiveGrid.printChildren(14);
                        // adaptiveGridBasedSensor->adaptiveGrid.printChildren(15);

                    }
                }
            }
        }

        // Draw obstacles if enabled
        if(showObstacles) {

            // Draw obstacles
            for (const Obstacle& obstacle : obstacles) {
                
                sf::RectangleShape obstacleShape(sf::Vector2f(obstacle.getBounds().size.x * scale, obstacle.getBounds().size.y * scale));
                obstacleShape.setPosition({obstacle.getBounds().position.x * scale + offset.x, obstacle.getBounds().position.y * scale + offset.y}); // Scale only position here
                obstacleShape.setFillColor(obstacle.getColor());
                window.draw(obstacleShape);
            }
        }

        // Clear the vertex arrays
        bufferZonesVertexArray.clear();
        agentBodyVertexArray.clear();
        agentArrowHeadVertexArray.clear();
        agentArrowBodyVertexArray.clear();

        // Draw agents
        for (auto& agent : currentFrame) {

            // Scale agent data
            agent.position *= scale; // Pixels
            agent.initialPosition *= scale; // Pixels
            agent.targetPosition *= scale; // Pixels
            agent.bodyRadius *= scale; // Pixels
            agent.velocity *= scale; // Pixels
            agent.bufferZoneRadius *= scale; // Pixels
            agent.velocityMagnitude *= scale; // Pixels

            // Scale the trajectory waypoints
            for(auto& waypoint : agent.trajectory) {
                waypoint *= scale;
            }

            // Determine the next waypoint index that is ahead of the agent
            if(showWaypoints) {
                
                // Draw the trajectory waypoints ahead of the agent
                // sf::VertexArray waypoints(sf::Quads); // Note: SFML 2.6.2 or prior
                // sf::VertexArray waypoints(sf::PrimitiveType::Triangles, 6);
                sf::VertexArray waypoints(sf::PrimitiveType::Triangles, 6);

                // Calculate the next waypoint
                agent.getNextWaypoint();

                // Calculate the neighboring pixels for the quad
                if (agent.nextWaypointIndex != -1) {
                    for (size_t i = agent.nextWaypointIndex; i < agent.trajectory.size(); ++i) {
                        sf::Vector2f center = agent.trajectory[i];
                        sf::Color color = agent.waypointColor;

                        // Calculate quad vertices
                        sf::Vector2f topLeft(center.x - waypointRadius + offset.x, center.y - waypointRadius + offset.y);
                        sf::Vector2f topRight(center.x + waypointRadius + offset.x, center.y - waypointRadius + offset.y);
                        sf::Vector2f bottomRight(center.x + waypointRadius + offset.x, center.y + waypointRadius + offset.y);
                        sf::Vector2f bottomLeft(center.x - waypointRadius + offset.x, center.y + waypointRadius + offset.y);

                        // Add vertices to the VertexArray
                        // Note: SFML 2.6.2 or prior
                        // waypoints.append(sf::Vertex(topLeft, color));
                        // waypoints.append(sf::Vertex(topRight, color));
                        // waypoints.append(sf::Vertex(bottomRight, color));
                        // waypoints.append(sf::Vertex(bottomLeft, color));

                        // // First triangle
                        waypoints.append(sf::Vertex({topLeft, color}));
                        waypoints.append(sf::Vertex({bottomLeft, color}));
                        waypoints.append(sf::Vertex({bottomRight, color}));

                        // // Second triangle
                        waypoints.append(sf::Vertex({topLeft, color}));
                        waypoints.append(sf::Vertex({topRight, color}));
                        waypoints.append(sf::Vertex({bottomRight, color}));
                    }
                }

                // Draw waypoints quads
                window.draw(waypoints);
            }   

            // Draw the trajectory lines from initial position to current position respectively to target position
            if(showTrajectories) {
                
                sf::Vertex pastTrajectory[] = {
                    sf::Vertex({agent.initialPosition + offset, sf::Color::Blue}), 
                    sf::Vertex({agent.position + offset, sf::Color::Blue})
                };
                sf::Vertex futureTrajectory[] = {
                    sf::Vertex({agent.position + offset, sf::Color::Red}),
                    sf::Vertex({agent.targetPosition + offset, sf::Color::Red})
                };
                window.draw(pastTrajectory, 2, sf::PrimitiveType::Lines);
                window.draw(futureTrajectory, 2, sf::PrimitiveType::Lines);

            }

            // Draw the arrow (a triangle) representing the velocity vector
            if(showArrow) {

                sf::Vector2f direction = agent.velocity;  // Direction of the arrow from velocity vector
                float theta = std::atan2(agent.heading.y, agent.heading.x) * 180.0f / M_PI; // Convert to degrees
                sf::Angle thetaAngle = sf::degrees(theta);
                float arrowLength = 5.0;

                // Normalize the direction vector for consistent arrow length
                float magnitude = std::sqrt(direction.x * direction.x + direction.y * direction.y);
                sf::Vector2f normalizedDirection;
                
                // Normalize the direction vector if its magnitude is greater than a small threshold
                if (magnitude > EPSILON) {

                    // Normalize the direction vector
                    normalizedDirection = direction / magnitude;

                    // Arrow shape factor
                    float arrowHeadLength = 0.4 * scale;
                    float arrowHeadWidth = 0.25 * scale;
                    int lineThickness = 4;

                    // Arrowhead (triangle) - Offset the tip by the agent's radius
                    sf::ConvexShape arrow(3);
                    arrow.setPoint(0, sf::Vector2f(0, 0)); // Tip of the arrow
                    arrow.setPoint(1, sf::Vector2f(-arrowHeadLength, arrowHeadWidth / 2));
                    arrow.setPoint(2, sf::Vector2f(-arrowHeadLength , -arrowHeadWidth / 2));
                    arrow.setFillColor(sf::Color::Black);

                    sf::Vertex line[] =
                    {
                        sf::Vertex({agent.position + offset, sf::Color::Black}),  // Offset start point
                        sf::Vertex({line[0].position + normalizedDirection * (agent.bodyRadius + arrowHeadLength + agent.velocityMagnitude / arrowLength), sf::Color::Black}) // Ending point (base of arrowhead)
                    };

                    // Append the arrow lines to the vertex array
                    agentArrowBodyVertexArray.append(line[0]);
                    agentArrowBodyVertexArray.append(line[1]);

                    // Create a triangle shape for the arrowhead
                    sf::Vertex triangle[] =
                    {
                        sf::Vertex({line[1].position, sf::Color::Black}),  // Tip of the arrowhead
                        sf::Vertex({triangle[0].position + sf::Vector2f(-arrowHeadLength, arrowHeadWidth / 2), sf::Color::Black}),  // Left point of the arrowhead
                        sf::Vertex({triangle[0].position + sf::Vector2f(-arrowHeadLength, -arrowHeadWidth / 2), sf::Color::Black})  // Right point of the arrowhead
                    };

                    // Rotate the triangle shape based on the agent's heading
                    sf::Transform transform;
                    transform.rotate(thetaAngle, triangle[0].position);

                    // Apply the transformation to the triangle vertices and append them to the vertex array
                    for(int i = 0; i < 3; i++) {
                        triangle[i].position = transform.transformPoint(triangle[i].position);
                        agentArrowHeadVertexArray.append(triangle[i]);
                    }
                } else {
                    normalizedDirection = sf::Vector2f(0, 0);
                }
            }
            // Append the agent bodies and buffer zones to the vertex arrays
            appendAgentBodies(agentBodyVertexArray, agent);

            // Draw the buffer zones
            if(showBufferZones){ 

                appendBufferZones(bufferZonesVertexArray, agent);
            }
        }

        // Draw the sensor detection areas
        // if(showSensors) {

        //     // Draw sensor detection area of each sensor
        //     for (const auto& sensorPtr : sensors) { // Iterate over all sensor pointers

        //         // Dereference the sensor pointer
        //         const Sensor& sensor = *sensorPtr;

        //         // Draw the detection area for each sensor
        //         sf::RectangleShape detectionAreaShape(sf::Vector2f(sensor.detectionArea.size.x * scale, sensor.detectionArea.height * scale));
        //         detectionAreaShape.setPosition(sensor.detectionArea.position.x * scale + offset.x, sensor.detectionArea.top * scale + offset.y);
        //         detectionAreaShape.setFillColor(sensor.detectionAreaColor); // Set the color with alpha
        //         window.draw(detectionAreaShape);
        //     }
        // }

        // Draw the vertex arrays
        window.draw(bufferZonesVertexArray);
        window.draw(agentArrowBodyVertexArray);
        window.draw(agentBodyVertexArray);
        window.draw(agentArrowHeadVertexArray);

        // Draw simulation canvas
        sf::RectangleShape canvas(sf::Vector2f(simulationWidth, simulationHeight));
        canvas.setPosition(offset);
        canvas.setFillColor(sf::Color::Transparent);
        canvas.setOutlineColor(sf::Color::Black);
        canvas.setOutlineThickness(2.0f);
        window.draw(canvas);

        // Draw the text elements
        if(showInfo) {
            window.draw(frameText);
            window.draw(frameRateText);
            window.draw(agentCountText);
            window.draw(timeText);
            window.draw(playbackSpeedText);
        }

        // Draw buttons
        window.draw(pauseButton);
        window.draw(pauseButtonText);
        window.draw(resetButton);
        window.draw(resetButtonText);

        window.display();
    }
}

// Function to append buffer zones to the vertex array
void Renderer::appendBufferZones(sf::VertexArray& vertices, const Agent& agent) {

    // Calculate the number of segments depending on the buffer zone radius
    int numSegments = std::max(100, static_cast<int>(agent.bufferZoneRadius * 6.0f));
    
    // Loop through each segment and calculate the position of the vertices
    for (int i = 0; i < numSegments; ++i) {
        float angle = 2 * M_PI * i / numSegments;
        sf::Vector2f radius(agent.bufferZoneRadius * std::cos(angle), agent.bufferZoneRadius * std::sin(angle));
        sf::Vector2f radius2((agent.bufferZoneRadius - 1) * std::cos(angle), agent.bufferZoneRadius * std::sin(angle));
        vertices.append(sf::Vertex({agent.position + offset + radius, agent.bufferZoneColor}));
        vertices.append(sf::Vertex({agent.position + offset + radius2, agent.bufferZoneColor}));
    }
}

// Function to append agent bodies to the vertex array
void Renderer::appendAgentBodies(sf::VertexArray& triangles, const Agent& agent) {

    // Extract the type of the agent
    std::stringstream ss(agent.type);
    std::string word1, word2;
    ss >> word1 >> word2;

    sf::Vector2f position = agent.position + offset;
    float theta = std::atan2(agent.heading.y, agent.heading.x) * 180.0f / M_PI; // Convert to degrees
    sf::Angle thetaAngle = sf::degrees(theta);
    float minRadius = std::ceil(sin(M_PI/4) * agent.bodyRadius);
    float maxRadius = std::ceil(agent.bodyRadius);
    float divX, divY;

    // Determine the division factors for the agent body based on its type // TO-DO: all types?
    if(word2 == "Cyclist") {
        divX = 1;
        divY = 2;
    }
    else if(word2 == "E-Scooter") {
        divX = 1;
        divY = 3;
    }
    else if(word2 == "Pedestrian") {
        divX = 1;
        divY = 1;
    }
    else {
        divX = 1;
        divY = 1;
    }

    // Calculate the positions of the quad vertices
    sf::Vector2f topLeft = sf::Vector2f(position.x - minRadius, position.y - minRadius/divY);
    sf::Vector2f topRight = sf::Vector2f(position.x + minRadius, position.y - minRadius/divY);
    sf::Vector2f bottomRight = sf::Vector2f(position.x + minRadius, position.y + minRadius/divY);
    sf::Vector2f bottomLeft = sf::Vector2f(position.x - minRadius, position.y + minRadius/divY);

    // Create the quad and transform it based on the agent's heading
    // SFML 2.6.1 and prior
    // sf::VertexArray body(sf::Quads, 4);
    // body[0] = sf::Vertex(topLeft, agent.color);
    // body[1] = sf::Vertex(topRight, agent.color);
    // body[2] = sf::Vertex(bottomRight, agent.color);
    // body[3] = sf::Vertex(bottomLeft, agent.color);

    // Create the quad and transform it based on the agent's heading
    sf::VertexArray body(sf::PrimitiveType::Triangles, 6);

    // First triangle: topLeft, topRight, bottomRight
    body[0] = sf::Vertex({topLeft, agent.color});
    body[1] = sf::Vertex({bottomLeft, agent.color});
    body[2] = sf::Vertex({bottomRight, agent.color});
    
    // Second triangle: bottomRight, bottomLeft, topLeft
    body[3] = sf::Vertex({topLeft, agent.color});
    body[4] = sf::Vertex({topRight, agent.color});
    body[5] = sf::Vertex({bottomRight, agent.color});

    sf::Transform transform;
    // transform.rotate(theta, position); // Note: SFML 2.6.1 and prior
    transform.rotate(thetaAngle, position);

    // Apply the transformation to the quad vertices and append them to the vertex array
    for(int i = 0; i < 6; i++) {
        body[i].position = transform.transformPoint(body[i].position);
        triangles.append(body[i]);
    }
}