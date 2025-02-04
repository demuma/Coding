#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <random>
#include <yaml-cpp/yaml.h>
#include "../include/Logging.hpp"
#include "../include/Quadtree.hpp"
#include "../include/Agent.hpp"

int main() {

    // Configuration loading
    YAML::Node config;
    try {
        config = YAML::LoadFile("config.yaml");
    } catch (const YAML::Exception& e) {
        ERROR_MSG("Error loading config file: " << e.what());
        return 1;
    }

    // Load global configuration data
    int maxDepth = config["quadtree"]["max_depth"].as<int>();
    int agentSize = config["quadtree"]["agent_size"].as<int>();
    int agentSpeed = config["quadtree"]["agent_speed"].as<int>();
    int fontSize = config["quadtree"]["font_size"].as<int>();
    std::map<std::string, Agent::AgentTypeAttributes> agentTypeAttributes;

    sf::RenderWindow window(sf::VideoMode(1200, 1200), "Quadtree (2x2 Base Grid)");
    sf::Font font;
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        std::cerr << "Error: Failed to load font.\n";
        return -1;
    }

    Quadtree quadtree(600, maxDepth);

    sf::Clock clock;
    sf::Time timePerFrame = sf::seconds(1.f / 30.f); // 30 Hz
    sf::Time timeSinceLastUpdate = sf::Time::Zero;

    int numberAgents = 1;

    // Generate agents
    for (int i = 0; i < numberAgents; ++i) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> disPosX(0, quadtree.cellSize * 2);
        std::uniform_real_distribution<> disPosY(0, quadtree.cellSize * 2);

        Agent agent(agentTypeAttributes["Adult Cyclist"]);
        agent.initialPosition = sf::Vector2f(disPosX(gen), disPosY(gen));
        quadtree.positions.push_back(agent.initialPosition);
        std::cout << "Agent " << i << " at (" << agent.initialPosition.x << ", " << agent.initialPosition.y << ")\n";
    }

    // quadtree.generatePositions(2);

    try {
        while (window.isOpen()) {
            timeSinceLastUpdate += clock.restart();

            while (timeSinceLastUpdate > timePerFrame) {
                timeSinceLastUpdate -= timePerFrame;

                sf::Event event;
                while (window.pollEvent(event)) {
                    if (event.type == sf::Event::Closed)
                        window.close();

                    if (event.type == sf::Event::KeyPressed) {
                        if (event.key.code == sf::Keyboard::Escape) {
                            window.close();
                        } 
                        if (event.key.code == sf::Keyboard::R) {
                            quadtree.clear();
                            quadtree.positions.clear();
                        }
                    }
                    if (event.type == sf::Event::MouseButtonPressed) {
                        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                        int nearestCellId = quadtree.getNearestCell(mousePos);
                        sf::Vector2f cellCenter = quadtree.getCellCenter(nearestCellId);

                        std::cout << "Clicked at (" << mousePos.x << ", " << mousePos.y << ") -> Nearest cell: " << nearestCellId
                                << " with center at (" << cellCenter.x << ", " << cellCenter.y << ")\n";

                        std::cout << std::endl;

                        quadtree.positions.push_back(mousePos);

                    }
                }
                // Clear the existing quadtree
                // quadtree.clear();
                quadtree.reset();

                // Move positions
                quadtree.movePositionsRight(agentSpeed);

                // Split the quadtree based on new positions
                quadtree.splitFromPositions();
            }

            window.clear(sf::Color::White);
            quadtree.draw(window, font);
            quadtree.drawPositions(window, quadtree.positions); // Assuming you want to draw positions every frame
            window.display();
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}