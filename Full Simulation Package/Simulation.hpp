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

// Simulation class
class Simulation {
public:
    Simulation(sf::RenderWindow& window, const sf::Font& font, const YAML::Node& config);
    void run(); // Main simulation loop

private:
    
    // Data Members
    sf::Clock clock;
    float cumulativeSum = 0.0f;
    const YAML::Node& config;

    // Configuration Data Members (read from YAML)
    int windowWidth;
    int windowHeight;
    float windowWidthScaled = 0.0f;
    float windowHeightScaled = 0.f;
    float cellSize = 100.0f;
    bool showGrid;
    bool showTrajectories;
    bool showInfo;
    int numAgents;
    float durationSeconds;
    int maxFrames;
    sf::Time timeStep;
    int scale = 20; // Pixels per meter

    // Simulation Data Members
    sf::RenderWindow& window;
    const sf::Font& font;
    std::vector<Agent> agents;
    Grid grid;

    // Obstacles
    std::vector<Obstacle> obstacles;

    // UI Elements
    sf::Text frameText;
    sf::Text frameRateText;
    sf::Text agentCountText;
    sf::RectangleShape pauseButton;
    sf::Text pauseButtonText;
    sf::RectangleShape resetButton;
    sf::Text resetButtonText;

    // Frame Rate Calculation
    std::vector<float> frameRates;
    const size_t frameRateBufferSize = 100;
    float frameRate;
    int frameCount;
    float movingAverageFrameRate;

    // Simulation State
    bool isPaused = false;
    sf::Time elapsedTime;
    sf::Time totalElapsedTime;

    // Database
    mongocxx::collection collection;

    // Helper functions
    void loadConfiguration(); // Loads configuration data
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
    void loadObstacles();
    void storeAgentData(const std::vector<Agent>& agents);
    sf::Color stringToColor(std::string colorStr);
    float generateRandomNumberFromTND(float mean, float stddev, float min, float max);
    sf::Vector2f generateRandomVelocityVector(float mu, float sigma, float min, float max);
};

#endif // SIMULATION_HPP

