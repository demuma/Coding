#pragma once

#include <SFML/Graphics.hpp>
#include <atomic>
#include <deque>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <iomanip>
#include <thread>

#include "Agent.hpp"
#include "SharedBuffer.hpp"
#include "Logging.hpp"

/************************************/
/********** RENDERER CLASS **********/
/************************************/

// Renderer class
class Renderer {
public:
    Renderer(SharedBuffer& buffer, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config);
    void run();
    void loadConfiguration();
    void initializeWindow();
    void initializeGUI();
    void updateFrameCountText();
    void updateFrameRateText();
    void updateAgentCountText();
    void updateTimeText();
    void updatePlayBackSpeedText();
    void handleEvents(sf::Event& event);
    sf::Time calculateSleepTime();

private:
    void render();

    // Renderer parameters
    sf::RenderWindow window;
    std::atomic<bool>& stop;
    const YAML::Node& config;

    // Display parameters
    sf::Font font;
    std::string title;
    bool showInfo = true;

    // Window scaling
    int windowWidth;  // Pixels
    int windowHeight; // Pixels
    float simulationWidth;  // Pixels
    float simulationHeight; // Pixels
    float scale;
    sf::Vector2f offset;

    // Renderer parameters
    float timeStep;
    float playbackSpeed;
    float previousPlaybackSpeed;
    bool paused = false;
    bool live_mode;
    int numAgents;

    // Visualization elements
    sf::Text frameText;
    sf::Text frameRateText;
    sf::Text agentCountText;
    sf::Text timeText;
    sf::Text playbackSpeedText;
    sf::RectangleShape pauseButton;
    sf::Text pauseButtonText;
    sf::RectangleShape resetButton;
    sf::Text resetButtonText;

    // Frame rate calculation
    std::deque<float> frameRates;
    size_t frameRateBufferSize;
    float frameRate;
    float movingAverageFrameRate;
    int maxFrames = 0;
    sf::Clock clock;
    static const int warmupFrames = 10;

    // Timing
    float targetFrameRate;
    float targetRenderTime;
    sf::Clock rendererClock;
    sf::Time rendererRealTime;
    sf::Time renderSimulationTime;
    sf::Time rendererFrameTime;
    sf::Time targetFrameTime;
    sf::Time currentSimulationFrameTime;
    std::atomic<float>& currentSimulationTimeStep;

    // Helper
    int frameEmptyCount = 0;

    // Shared buffer reference
    SharedBuffer& buffer;

    // Agents
    std::vector<Agent> currentFrame;
    std::atomic<int>& currentNumAgents;
    float waypointRadius;
    bool showTrajectories = true;
    bool showWaypoints = true;
    bool showObstacles = false;
    bool showCollisionGrid = true;
    bool showArrow = false;
};