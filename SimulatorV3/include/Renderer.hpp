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
#include "QuadtreeSnapshot.hpp"
#include "Logging.hpp"
#include "Obstacle.hpp"
#include "Utilities.hpp"
#include "Sensor.hpp"

/************************************/
/********** RENDERER CLASS **********/
/************************************/

// Renderer class
class Renderer {
public:
    Renderer(SharedBuffer<std::vector<Agent>>& buffer, 
        std::unordered_map<std::string, std::shared_ptr<SharedBuffer<std::shared_ptr<QuadtreeSnapshot::Node>>>> sensorBuffers,
        std::atomic<float>& currentSimulationTimeStep, const YAML::Node& config);
    void run();
    void loadConfiguration();
    void loadAgentsAttributes();
    void loadObstacles();
    void initializeSensors();
    void initializeWindow();
    void initializeGUI();
    void updateFrameCountText();
    void updateFrameRateText();
    void updateAgentCountText();
    void updateTimeText();
    void updatePlayBackSpeedText();
    // void handleEvents(sf::Event& event); // Note: SFML 2.6.2 or prior
    void handleEvents();
    sf::Time calculateSleepTime();

private:
    void render();
    void appendBufferZones(sf::VertexArray& vertices, const Agent& agent);
    void appendAgentBodies(sf::VertexArray& triangles, const Agent& agent);

    // Renderer parameters
    sf::RenderWindow window;
    const YAML::Node& config;

    // Display parameters
    sf::Font font;
    std::string title;
    bool showInfo = true;

    // Event handling
    bool isShiftPressed = false;
    bool isCtrlPressed = false;

    // Window scaling
    unsigned int windowWidth;  // Pixels
    unsigned int windowHeight; // Pixels
    float simulationWidth;  // Pixels
    float simulationHeight; // Pixels
    float initialSimulationWidth;  // Pixels
    float initialSimulationHeight; // Pixels
    float initialScale;
    float scale;
    sf::Vector2f offset;
    bool isPanning = false; // Currently dragging
    sf::Vector2f lastMousePosition = {0, 0}; // Last mouse position

    // Renderer parameters
    float timeStep;
    float playbackSpeed;
    float previousPlaybackSpeed;
    bool paused = false;
    bool live_mode;
    int numAgents;
    bool showGrids = false;
    bool showBufferZones = false;
    float EPSILON = 1e-6f;

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

    // Vertex arrays
    sf::VertexArray bufferZonesVertexArray;
    sf::VertexArray agentBodyVertexArray;
    sf::VertexArray agentArrowHeadVertexArray;
    sf::VertexArray agentArrowBodyVertexArray;

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

    // Temporary frame
    std::vector<Agent> pauseFrame;
    sf::Time pauseSleepTime;

    // Helper
    int frameEmptyCount = 0;

    // Shared buffer
    SharedBuffer<std::vector<Agent>>& buffer;
    std::unordered_map<std::string, std::shared_ptr<SharedBuffer<std::shared_ptr<QuadtreeSnapshot::Node>>>> sensorBuffers;

    // Sensors
    std::vector<std::unique_ptr<Sensor>> sensors;
    bool showSensors = false;
    bool showSensorGrid = true;

    // Obstacles
    bool showObstacles = false;
    std::vector<Obstacle> obstacles;

    // Corridors
    bool showCorridors = false;

    // Agents
    std::vector<Agent> currentFrame;
    int currentNumAgents;
    float waypointRadius;
    bool showTrajectories = false;
    bool showWaypoints = false;
    bool showArrow = false;

    // Collision
    bool showCollisionGrid = false;
    float collisionGridCellSize;

    // Agent attributes
    std::map<std::string, Agent::AgentTypeAttributes> agentTypeAttributes;
};