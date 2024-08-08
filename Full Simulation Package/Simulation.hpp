#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <SFML/Graphics.hpp>
#include <vector>
#include <yaml-cpp/yaml.h> 
#include <iostream>
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/exception/exception.hpp>
#include <iomanip>
#include <unordered_map>
#include <random>

#include "Agent.hpp"
#include "Grid.hpp"
#include "Obstacle.hpp"
#include "PerlinNoise.hpp"
#include "Sensor.hpp"
#include "AgentBasedSensor.hpp"
#include "GridBasedSensor.hpp"


// Simulation class
class Simulation {
public:
    Simulation(
        sf::RenderWindow& window, 
        const sf::Font& font, 
        const YAML::Node& config);
    void run(); // Main simulation loop
    void initializeDatabase();
    void initializeSensors();

protected:
    int frameCount;

private:

    // Database
    std::string dbUri;
    std::string databaseName;
    std::shared_ptr<mongocxx::client> client;
    mongocxx::instance instance;
    mongocxx::collection collection; // For agent data storage
    
    // Sensors
    std::vector<std::unique_ptr<Sensor>> sensors;

    // Data Members
    sf::Clock clock;
    float cumulativeSum = 0.0f;
    const YAML::Node& config; // not used
    const sf::Font& font;

    // Configuration Data Members (read from YAML) unit: meters
    int windowWidth;
    int windowHeight;
    float windowWidthScaled = 0.0f;
    float windowHeightScaled = 0.f;
    float collisionGridCellSize = 100.0f;
    float sensorGridCellSize = 100.0f;
    bool showCollisionGrid;
    bool showSensorGrid;
    bool showTrajectories;
    bool showInfo;
    int numAgents;
    float durationSeconds;
    int maxFrames;
    sf::Time timeStep;
    float speedFactor = 1.0f;
    int scale = 20; // Pixels per meter
    float simulationWidth;
    float simulationHeight;
    float simulationWidthOffsetScaled = 0.0f;
    float simulationHeightOffsetScaled = 0.0f;
    std::string datetime;

    // Window
    sf::RenderWindow& window;

    // Agents
    std::vector<Agent> agents;

    // Grid
    Grid grid;

    // Obstacles
    std::vector<Obstacle> obstacles;

    // UI Elements
    sf::Text frameText;
    sf::Text frameRateText;
    sf::Text agentCountText;
    sf::Text timeText;
    sf::RectangleShape pauseButton;
    sf::Text pauseButtonText;
    sf::RectangleShape resetButton;
    sf::Text resetButtonText;

    // Frame Rate Calculation
    std::vector<float> frameRates;
    const size_t frameRateBufferSize = 50;
    static const int warmupFrames = 10;
    float frameRate;
    float movingAverageFrameRate;

    // Simulation State
    bool isPaused = false;
    sf::Time totalElapsedTime;
    // float simulationTime = 0.0f;

    // Database

    // Helper functions
    void loadConfiguration(); // Loads configuration data
    void initializeGrid(); // Initializes the grid
    void initializeAgents();
    void initializeUI();
    void update(float deltaTime);
    void handleEvents(sf::Event event);
    void calculateFrameRate();
    void render();
    void resetSimulation();
    void updateFrameRateText(float frameRate);
    void updateFrameCountText(int frameCount);
    void updateAgentCountText();
    void updateTimeText();
    void loadObstacles();
    void postData(const std::vector<Agent>& agents);
    sf::Color stringToColor(std::string colorStr);
};

#endif // SIMULATION_HPP

