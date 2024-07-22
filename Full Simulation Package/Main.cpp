#include <SFML/Graphics.hpp>
#include <yaml-cpp/yaml.h>
#include <iostream>

#include "Simulation.hpp"

int main() {

    // Configuration Loading  -> TODO: Move to loadConfiguration function
    YAML::Node config;
    try {
        config = YAML::LoadFile("../config.yaml"); 
    } catch (const YAML::Exception& e) {
        std::cerr << "Error loading config file: " << e.what() << std::endl;
        return 1;
    }

    // Update parameters based on configuration -> TODO: Move to loadConfiguration function
    int windowWidth = config["display"]["width"].as<int>();
    int windowHeight = config["display"]["height"].as<int>();

    // Window setup -> TODO: Move to separate function within Simulation class
    sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), "Road User Simulation");

    // Load font -> TODO: Move to separate function within Simulation class
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