#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <SFML/Graphics.hpp>
#include <vector>
#include <yaml-cpp/yaml.h> 
#include "Agent.hpp"
#include "Grid.hpp"
#include <iostream>

class Simulation {
public:
    Simulation(sf::RenderWindow& window, const sf::Font& font, const YAML::Node& config);
    void run(); // Main simulation loop

private:
    sf::RenderWindow& window;
    const sf::Font& font;
    std::vector<Agent> agents;
    Grid grid;

    sf::Clock clock;
    size_t gridBasedCollisionCount = 0;
    size_t globalCollisionCount = 0;
    float cumulativeSum = 0.0f;
    const YAML::Node& config;

    // Configuration Data Members (read from YAML)
    int windowWidth;
    int windowHeight;
    float cellSize;
    bool showGrid;
    bool showTrajectories;
    int numAgents;
    float durationSeconds;
    int maxFrames;
    float frameRate;
    int frameCount;
    int fps;

    // UI Elements
    sf::Text frameText;
    sf::Text frameRateText;
    sf::RectangleShape pauseButton;
    sf::Text pauseButtonText;
    sf::RectangleShape resetButton;
    sf::Text resetButtonText;

    // Frame Rate Calculation
    std::vector<float> frameRates;
    const size_t frameRateBufferSize = 100;

    bool isPaused = false;
    sf::Time elapsedTime;
    sf::Time totalElapsedTime;
    std::chrono::duration<double> collisionTime;

    // Helper functions
    void loadConfiguration(); // Loads configuration data
    void initializeAgents();
    void initializeUI();
    void update(float deltaTime);
    void handleEvents(sf::Event event);
    void render();
    void resetSimulation();
    void updateFrameRateText(float frameRate);
    void updateFrameCountText(int frameCount);

    // Auxiliary functions
    sf::Color stringToColor(const std::string& colorStr);
};

#endif // SIMULATION_HPP
