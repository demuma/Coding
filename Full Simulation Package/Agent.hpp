#ifndef AGENT_HPP
#define AGENT_HPP

#include <SFML/Graphics.hpp>
#include <vector>
#include <uuid/uuid.h>

#include "PerlinNoise.hpp"

class Agent {
public:
    int id; // Agent ID
    std::string  uuid;
    std::string sensor_id;
    std::string type;
    sf::Vector2f position;
    sf::Vector2f initialPosition;
    sf::Vector2f velocity;
    std::string timestamp;
    int priority;
    float lookAheadTime;
    sf::Vector2f acceleration;
    sf::Vector2f originalVelocity; // To store original velocity
    sf::Color color; // Start color is black
    sf::Color initial_color = sf::Color::Black; // Initial color is black
    std::string initial_color_str;
    sf::Color bufferColor = sf::Color::Green; // Start buffer color is green
    float minBufferZoneRadius = 0.5f; // Minimum buffer zone radius in meters
    float radius; // Agent radius
    float minVelocity; // Minimum velocity
    float maxVelocity; // Maximum velocity
    float velocityMu; // Mean velocity
    float velocitySigma; // Standard deviation of velocity
    float velocityMagnitude; // Magnitude of velocity
    float velocityNoiseFactor; // Noise factor for velocity
    float velocityNoiseScale;
    float minAcceleration; // Minimum acceleration
    float maxAcceleration; // Maximum acceleration
    float bufferRadius;
    bool collisionPredicted;
    bool stopped; // Flag indicating if the agent is stopped
    int stoppedFrameCounter; // Counter to handle stopping duration
    bool isActive; // Flag indicating if the agent is active
    unsigned int noiseSeed; // Seed for Perlin noise
    PerlinNoise perlinNoise; // Perlin noise generator
    
    Agent();
    void initialize();
    void updatePosition(float deltaTime);
    void updateVelocity(float deltaTime, sf::Time totalElapsedTime);
    std::string generateUUID();
    std::string generateUUID(uuid_t uuid); // Overloaded function
    sf::Vector2f getFuturePositionAtTime(float time) const;
    void resetCollisionState();
    void stop();
    bool canResume(const std::vector<Agent>& agents);
    void resume(const std::vector<Agent>& agents);
};

#endif // AGENT_HPP