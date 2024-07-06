#include <SFML/Graphics.hpp>
#include <yaml-cpp/yaml.h> 
#include "Simulation.hpp"

int main() {
    // Configuration Loading
    YAML::Node config;
    try {
        config = YAML::LoadFile("../config.yaml"); 
    } catch (const YAML::Exception& e) {
        std::cerr << "Error loading config file: " << e.what() << std::endl;
        return 1;
    }

    // Update parameters based on configuration
    int windowWidth = config["display"]["width"].as<int>();
    int windowHeight = config["display"]["height"].as<int>();
    int fps = config["display"]["frame_rate"].as<int>();

    // Window setup
    sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), "Road User Simulation");
    window.setFramerateLimit(fps);

    // Load font
    sf::Font font;
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) { // Update with correct path if needed
        std::cerr << "Error loading font\n";
        return -1;
    }

    // Create simulation instance (passing the configuration)
    Simulation simulation(window, font, config);

    // Run the simulation
    simulation.run();

    return 0;
}