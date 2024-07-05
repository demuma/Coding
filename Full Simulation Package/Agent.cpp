#include "Agent.h"
#include <cmath> // For std::sqrt

Agent::Agent() {
    bufferRadius = 0;
    hasCollision = false;
    stopped = false;
    stoppedFrameCounter = 0;
}

void Agent::initialize() {
    float velocityMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    bufferRadius = radius * (2 + velocityMagnitude / 2.0f); 
}

void Agent::updatePosition() {
    position += velocity;
}

sf::Vector2f Agent::getFuturePositionAtTime(float time) const {
    return position + velocity * time;
}

sf::CircleShape Agent::getBufferZone() const {
    sf::CircleShape bufferZone(bufferRadius);
    bufferZone.setOrigin(bufferRadius, bufferRadius);
    bufferZone.setPosition(position);
    bufferZone.setFillColor(sf::Color::Transparent);
    bufferZone.setOutlineThickness(2.f);
    bufferZone.setOutlineColor(bufferColor);
    return bufferZone;
}

void Agent::resetCollisionState() {
    bufferColor = sf::Color::Green;
    hasCollision = false;
}

void Agent::stop() {
    if (!stopped) {
        original_velocity = velocity;
        velocity = sf::Vector2f(0.0f, 0.0f);
        stopped = true;
        stoppedFrameCounter = 0;
    }
}

bool Agent::canResume(const std::vector<Agent>& agents) {
    for (const auto& other : agents) {
        if (&other == this) continue;

        float dx = position.x - other.position.x;
        float dy = position.y - other.position.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        if (distance < bufferRadius + other.bufferRadius) {
            return false;
        }
    }
    return true;
}

void Agent::resume(const std::vector<Agent>& agents) {
    if (stopped && stoppedFrameCounter >= 20) {
        if (canResume(agents)) {
            velocity = original_velocity;
            stopped = false;
        }
    }
}
