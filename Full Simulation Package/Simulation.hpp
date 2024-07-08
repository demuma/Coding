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
    
    // Data Members
    sf::Clock clock;
    size_t gridBasedCollisionCount = 0;
    size_t globalCollisionCount = 0;
    float cumulativeSum = 0.0f;
    const YAML::Node& config;

    // Configuration Data Members (read from YAML)
    int windowWidth;
    int windowHeight;
    float cellSize = 100.0f;
    bool showGrid;
    bool showTrajectories;
    bool showInfo;
    int numAgents;
    float durationSeconds;
    int maxFrames;
    int fps;
    sf::Time timeStep;

    // Simulation Data Members
    sf::RenderWindow& window;
    const sf::Font& font;
    std::vector<Agent> agents;
    Grid grid;

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
    void updateAgentCountText();

    // Auxiliary functions
    //sf::Color stringToColor(const std::string& colorStr);

    // Function to convert a string to sf::Color (case-insensitive)
    sf::Color stringToColor(const std::string& colorStr) {  // Note the Simulation:: scope
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
};

#endif // SIMULATION_HPP
