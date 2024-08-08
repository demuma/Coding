#include "../include/Renderer.hpp"

// Renderer member functions
Renderer::Renderer(SharedBuffer& buffer, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config)
: buffer(buffer), currentSimulationTimeStep(currentSimulationTimeStep), currentNumAgents(currentNumAgents), stop(stop), config(config) {

    DEBUG_MSG("Renderer: read buffer: " << buffer.currentReadFrameIndex);
    loadConfiguration();
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
    simulationWidth = (config["simulation"]["width"].as<int>() * scale) ;
    simulationHeight = (config["simulation"]["height"].as<int>() * scale);
    offset = sf::Vector2f((windowWidth - simulationWidth) / 2, (windowHeight - simulationHeight) / 2);
    DEBUG_MSG("Offset: " << offset.x << ", " << offset.y);

    // Renderer parameters
    timeStep = config["simulation"]["time_step"].as<float>();
    playbackSpeed = config["simulation"]["playback_speed"].as<float>();
    numAgents = config["agents"]["num_agents"].as<int>();
    
    // Set the maximum number of frames if the duration is specified
    if(config["simulation"]["duration_seconds"]) {
        maxFrames = config["simulation"]["duration_seconds"].as<int>() / timeStep;
        targetRenderTime = config["simulation"]["duration_seconds"].as<int>();
    } else {
        maxFrames = config["simulation"]["maximum_frames"].as<int>();
        targetRenderTime = maxFrames * timeStep;
    }

    // Load display parameters
    showInfo = config["display"]["show_info"].as<bool>();
    showTrajectories = config["agents"]["show_trajectories"].as<bool>();
    showWaypoints = config["agents"]["show_waypoints"].as<bool>();
    waypointRadius = config["agents"]["waypoint_radius"].as<float>();

    // Load font
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        ERROR_MSG("Error loading font\n");
    }

    // Set the frame rate buffer size
    frameRateBufferSize = 1.0f / timeStep;
}

void Renderer::initializeWindow() {

    // Create window
    window.create(sf::VideoMode(windowWidth, windowHeight), title);
}

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
    resetButton.setFillColor(sf::Color::Cyan);
    resetButton.setPosition(window.getSize().x - 110, window.getSize().y - 120); 

    resetButtonText.setFont(font);
    resetButtonText.setString("Infos");
    resetButtonText.setCharacterSize(20);
    resetButtonText.setFillColor(sf::Color::Black);

    // Center the reset button text
    sf::FloatRect resetButtonTextRect = resetButtonText.getLocalBounds();
    resetButtonText.setOrigin(resetButtonTextRect.left + resetButtonTextRect.width / 2.0f,
                              resetButtonTextRect.top + resetButtonTextRect.height / 2.0f);
    resetButtonText.setPosition(resetButton.getPosition().x + resetButton.getSize().x / 2.0f,
                                resetButton.getPosition().y + resetButton.getSize().y / 2.0f);

}

// Function to update the text that displays the current frame count
void Renderer::updateFrameCountText() {

    frameText.setString("Frame " + std::to_string(buffer.currentReadFrameIndex) + " / " + (maxFrames > 0 ? std::to_string(maxFrames) : "∞")); // ∞ for unlimited frames
    sf::FloatRect textRect = frameText.getLocalBounds();
    frameText.setOrigin(textRect.width, 0); // Right-align the text
    frameText.setPosition(window.getSize().x - 10, 40); // Position with padding
}

// Function to update the text that displays the current agent count
void Renderer::updateAgentCountText() {

    agentCountText.setString("Agents: " + std::to_string(currentNumAgents));
    sf::FloatRect textRect = agentCountText.getLocalBounds();
    agentCountText.setOrigin(textRect.width, 0); // Right-align the text
    agentCountText.setPosition(window.getSize().x - 6, 100); // Position with padding
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
    timeText.setOrigin(textRect.width, 0); 
    timeText.setPosition(window.getSize().x - 10, 10); 
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
        frameRateText.setOrigin(textRect.width, 0); // Right-align the text
        frameRateText.setPosition(window.getSize().x - 10, 70); // Position with padding

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
    playbackSpeedText.setOrigin(textRect.width, 0); // Right-align the text
    playbackSpeedText.setPosition(window.getSize().x - 6, 130); // Position with padding
}

// Function to handle events
void Renderer::handleEvents(sf::Event& event) {

    if (event.type == sf::Event::Closed) {
        window.close();
        stop.store(true);
        return;
    }
    else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Space) {
            paused = !paused;
            pauseButtonText.setString(paused ? "  Play" : "Pause");
            pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green);
            window.draw(pauseButton); // NEW
            window.draw(pauseButtonText); // NEW
            window.display(); // NEW
            return;
        }
        else if (event.key.code == sf::Keyboard::T) {
            showTrajectories = !showTrajectories;
            return;
        }
        else if (event.key.code == sf::Keyboard::W) {
            showWaypoints = !showWaypoints;
            return;
        }
        else if (event.key.code == sf::Keyboard::Escape) {
            window.close();
            stop.store(true);
            return;
        }
        else if (event.key.code == sf::Keyboard::R) {
            playbackSpeed = 1.0f;
            DEBUG_MSG("Playback speed reset to: " << playbackSpeed);
            return;
        }
        else if (event.key.code == sf::Keyboard::Up) {
            if (playbackSpeed >= 1.0f) {
                playbackSpeed += 1.0f;
                playbackSpeed = std::floor(playbackSpeed);
            } else if (playbackSpeed < 1.0f && playbackSpeed > 0.1f) {
                playbackSpeed += 0.1f;
            } else {
                playbackSpeed = 0.1f;
            }
            DEBUG_MSG("Playback speed increased to: " << playbackSpeed);
            return;
        }
        else if (event.key.code == sf::Keyboard::Down) {
            if (playbackSpeed > 1.0f) {
                playbackSpeed -= 1.0f;
                playbackSpeed = std::ceil(playbackSpeed);
            } else if (playbackSpeed <= 1.0f && playbackSpeed > 0.1f) {
                playbackSpeed -= 0.1f;
            } else {
                playbackSpeed = 0.1f;
            }
            DEBUG_MSG("Playback speed decreased to: " << playbackSpeed);
            return;
        }
    } else if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
            if (pauseButton.getGlobalBounds().contains(mousePosition.x, mousePosition.y)) {
                paused = !paused;
                pauseButtonText.setString(paused ? "  Play" : "Pause");
                pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green);
                window.draw(pauseButton); // NEW
                window.draw(pauseButtonText); // NEW
                window.display(); // NEW
            } else if (resetButton.getGlobalBounds().contains(mousePosition.x, mousePosition.y)) {
                showInfo = !showInfo;
            }
        }
    }
}

void Renderer::run() {

    // Set the target frame rate
    targetFrameRate = playbackSpeed * (1.0f / timeStep);
    targetFrameTime = sf::seconds(1.0f / targetFrameRate);

    // Initialize the event
    sf::Event event;

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
        while (window.pollEvent(event)) {
            handleEvents(event);
        }

        // Get the time taken to handle events
        eventHandlingTime = rendererFrameClock.getElapsedTime().asSeconds();

        // Run the renderer if not paused
        if(!paused) {

            // Read the current frame from the buffer
            currentFrame = buffer.read();

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
            auto sleep_time = calculateSleepTime();
            std::this_thread::sleep_for(std::chrono::duration<float>(sleep_time.asSeconds()));

            // Get the total time taken to render the frame
            rendererTotalFrameTime = rendererFrameClock.getElapsedTime();
            rendererFrameRate = 1.0f / rendererTotalFrameTime.asSeconds();
            frameRate = rendererFrameRate;

            // Update the render time
            rendererRealTime += rendererTotalFrameTime;
            renderSimulationTime += timeStepTime;
        }
    }
    STATS_MSG("Total render wall time: " << rendererRealTime.asSeconds() << " seconds for " << buffer.currentReadFrameIndex << " frames");
    STATS_MSG("Total render time: " << targetRenderTime << " seconds" << " for " << numAgents << " agents");
    STATS_MSG("Render speedup: " << targetRenderTime / rendererRealTime.asSeconds());
    STATS_MSG("Average frame rate: " << 1/(rendererRealTime.asSeconds() / (buffer.currentReadFrameIndex + 1)));
    STATS_MSG("Average render frame time: " << rendererRealTime.asSeconds() / (buffer.currentReadFrameIndex + 1));
    STATS_MSG("Average read buffer time: " << readBufferTime / (buffer.currentReadFrameIndex + 1));
}

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
    sf::Time remainingTime = targetFrameTime - rendererFrameTime;;

    // Ensure the sleep time is non-negative
    return std::max(remainingTime, sf::Time::Zero);
}

void Renderer::render() {  // Input: meters, Output: pixels
    window.clear(sf::Color::White);

    // Loop through all agents and render them
    if (currentFrame.size() != 0) {
        for (auto& agent : currentFrame) {

            // Scale agent data (window offset missing!)
            agent.position *= scale; // Pixels
            agent.initialPosition *= scale; // Pixels
            agent.targetPosition *= scale; // Pixels
            agent.bodyRadius *= scale; // Pixels
            agent.velocity *= scale; // Pixels

            // Scale the trajectory waypoints
            for(auto& waypoint : agent.trajectory) {
                waypoint *= scale;
            }

            // Determine the next waypoint index that is ahead of the agent
            if(showWaypoints) {
                
                // Draw the trajectory waypoints ahead of the agent
                sf::VertexArray waypoints(sf::Quads);

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
                        waypoints.append(sf::Vertex(topLeft, color));
                        waypoints.append(sf::Vertex(topRight, color));
                        waypoints.append(sf::Vertex(bottomRight, color));
                        waypoints.append(sf::Vertex(bottomLeft, color));
                    }
                }

                // Draw waypoints quads
                window.draw(waypoints);
            }   

            // Draw the trajectory line from initial position to current position
            if(showTrajectories) {
                sf::Vertex trajectory[] = {
                    sf::Vertex(agent.initialPosition + offset, sf::Color::Blue), 
                    sf::Vertex(agent.position + offset, sf::Color::Blue)
                };
                window.draw(trajectory, 2, sf::Lines);
            }

            // Draw the agent
            sf::CircleShape agentShape;
            agentShape.setPosition(
                agent.position.x - agent.bodyRadius + offset.x, 
                agent.position.y - agent.bodyRadius + offset.y
            );
            agentShape.setRadius(agent.bodyRadius);
            agentShape.setFillColor(agent.color);
            window.draw(agentShape);

            // Draw the buffer zone (check)
            sf::CircleShape bufferZoneShape(agent.bufferZoneRadius);
            bufferZoneShape.setOrigin(bufferZoneShape.getRadius(), bufferZoneShape.getRadius());
            bufferZoneShape.setPosition(agent.position.x + offset.x, agent.position.y + offset.y);
            bufferZoneShape.setFillColor(sf::Color::Transparent);
            bufferZoneShape.setOutlineThickness(2.f);
            bufferZoneShape.setOutlineColor(agent.bufferZoneColor);
            window.draw(bufferZoneShape);
        }

        // Draw simulation canvas
        sf::RectangleShape canvas(sf::Vector2f(simulationWidth, simulationHeight));
        canvas.setPosition(offset);
        canvas.setFillColor(sf::Color::Transparent);
        canvas.setOutlineColor(sf::Color::Black);
        canvas.setOutlineThickness(2.0f);
        window.draw(canvas);

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

    } else {
        ++frameEmptyCount;
        ERROR_MSG("Frame is empty: " << frameEmptyCount);
    }
}
