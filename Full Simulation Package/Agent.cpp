#include <cmath> // For std::sqrt

#include "Agent.hpp"

// Constructor
Agent::Agent() {
    bufferRadius = 0;
    collisionPredicted = false;
    stopped = false;
    stoppedFrameCounter = 0;
    isActive = true;
}

// Generate a unique identifier for the agent
std::string Agent::generateUUID() {

    uuid_t uuid;
    uuid_generate(uuid);
    char uuidStr[37];
    uuid_unparse(uuid, uuidStr);

    return std::string(uuidStr);
}

// Generate a unique identifier for the agent with a given UUID
std::string Agent::generateUUID(uuid_t uuid) {
    
    uuid_generate(uuid);
    char uuidStr[37];
    uuid_unparse(uuid, uuidStr);

    return std::string(uuidStr);
}

// Initialize the agent with default values and calculate buffer radius
void Agent::initialize() {
    float velocityMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    bufferRadius = minBufferZoneRadius + radius + (velocityMagnitude / maxVelocity); 
}

// Update the agent's position based on velocity
void Agent::updatePosition(float deltaTime) {
    position += velocity * deltaTime;
}

// Update the agent's velocity based on Perlin noise
void Agent::updateVelocity(float deltaTime, sf::Time totalElapsedTime) {
    
    // Use the agent's own Perlin noise generator to fluctuate velocity
    float noiseX = perlinNoise.noise(position.x * 0.01f, totalElapsedTime.asSeconds(), 0);
    float noiseY = perlinNoise.noise(position.y * 0.01f, totalElapsedTime.asSeconds() + 1000.0f, 0);

    // Apply noise to velocity
    velocity.x += noiseX * velocityNoiseFactor * originalVelocity.x * deltaTime;
    velocity.y += noiseY * velocityNoiseFactor * originalVelocity.y * deltaTime;
}

// Get the future position of the agent at a given time
sf::Vector2f Agent::getFuturePositionAtTime(float time) const {

    return position + velocity * time;
}

// Reset the collision state of the agent
void Agent::resetCollisionState() {

    bufferColor = sf::Color::Green;
    collisionPredicted = false;
}

// Stop the agent
void Agent::stop() {

    if (!stopped) {
        this->originalVelocity = this->velocity;
        this->velocity = sf::Vector2f(0.0f, 0.0f);
        this->stopped = true;
        this->stoppedFrameCounter = 0;
    }
}

// Check if the agent can resume movement without colliding with other agents
bool Agent::canResume(const std::vector<Agent>& agents) {

    for (const auto& other : agents) {
        if (&other == this) continue;

        float dx = position.x - other.position.x;
        float dy = position.y - other.position.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        if (distance < bufferRadius + other.bufferRadius) {
            
            //return false;
        }
    }
    return true;
}

// Resume the agent's movement if there are no collisions
void Agent::resume(const std::vector<Agent>& agents) {
    //if (stopped && stoppedFrameCounter >= 20) {
    if (stopped) {
        if (canResume(agents)) {
            velocity = originalVelocity;
            stopped = false;
        }
    }
}