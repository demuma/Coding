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

#include "Agent.hpp"
#include "Grid.hpp"
#include "Obstacle.hpp"

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
    int fps;
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
    mongocxx::collection setupDatabase();
    void storeAgentData(const std::vector<Agent>& agents);

    // Auxiliary functions
    //sf::Color stringToColor(const std::string& colorStr);

    // Function to convert a string to sf::Color (case-insensitive)
    sf::Color stringToColor(std::string colorStr) {

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
};

#endif // SIMULATION_HPP
