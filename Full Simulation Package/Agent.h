#ifndef AGENT_H
#define AGENT_H

#include <SFML/Graphics.hpp>
#include <vector>

class Agent {
public:
    sf::Vector2f position;
    sf::Vector2f initial_position;
    sf::Vector2f velocity;
    sf::Vector2f original_velocity; // To store original velocity
    float radius = 5.0f; // Example radius
    sf::Color color = sf::Color::Black; // Start color is black
    sf::Color initial_color = sf::Color::Black; // Initial color is black
    std::string initial_color_str;
    sf::Color bufferColor = sf::Color::Green; // Start buffer color is green

    float min_velocity; // Minimum velocity
    float max_velocity; // Maximum velocity
    float bufferRadius;
    bool hasCollision;
    bool stopped; // Flag indicating if the agent is stopped
    int stoppedFrameCounter; // Counter to handle stopping duration

    Agent();
    void initialize();
    void updatePosition();
    sf::Vector2f getFuturePositionAtTime(float time) const;
    sf::CircleShape getBufferZone() const;
    void resetCollisionState();
    void stop();
    bool canResume(const std::vector<Agent>& agents);
    void resume(const std::vector<Agent>& agents);
};

#endif // AGENT_H