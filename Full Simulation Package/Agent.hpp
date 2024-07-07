#ifndef AGENT_HPP
#define AGENT_HPP

#include <SFML/Graphics.hpp>
#include <vector>
#include <uuid/uuid.h>

class Agent {
public:
    int id; // Agent ID
    std::string  uuid;
    std::string sensor_id;
    std::string type;
    sf::Vector2f position;
    sf::Vector2f initial_position;
    sf::Vector2f velocity;
    sf::Vector2f original_velocity; // To store original velocity
    sf::Color color; // Start color is black
    sf::Color initial_color = sf::Color::Black; // Initial color is black
    std::string initial_color_str;
    sf::Color bufferColor = sf::Color::Green; // Start buffer color is green


    float radius; // Agent radius
    float min_velocity; // Minimum velocity
    float max_velocity; // Maximum velocity
    float bufferRadius;
    bool hasCollision;
    bool stopped; // Flag indicating if the agent is stopped
    int stoppedFrameCounter; // Counter to handle stopping duration
    bool isActive; // Flag indicating if the agent is active

    Agent();
    void initialize();
    void updatePosition();
    std::string generateUUID();
    std::string generateUUID(uuid_t uuid);
    sf::Vector2f getFuturePositionAtTime(float time) const;
    sf::CircleShape getBufferZone() const;
    void resetCollisionState();
    void stop();
    bool canResume(const std::vector<Agent>& agents);
    void resume(const std::vector<Agent>& agents);
};

#endif // AGENT_HPP