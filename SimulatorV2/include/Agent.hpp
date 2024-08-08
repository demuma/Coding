#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <random>
#include <cmath>

#include "PerlinNoise.hpp"
#include "Logging.hpp"

/*********************************/
/********** AGENT CLASS **********/
/*********************************/

// Agent class
class Agent {
public:
    Agent();

    void calculateTrajectory(float waypointDistance);
    void getNextWaypoint();

    void updatePosition(float deltaTime);
    sf::Vector2f getFuturePositionAtTime(float time) const;

    void calculateVelocity(sf::Vector2f waypoint);
    void updateVelocity(float deltaTime, sf::Time totalElapsedTime);

    void stop();
    bool canResume(const std::vector<Agent>& agents);
    void resume(const std::vector<Agent>& agents);
    void resetCollisionState();
    void setBufferZoneSize();

    // Agent features
    std::string uuid;
    std::string sensor_id;
    std::string type;
    sf::Color color;
    sf::Color initialColor;
    int priority;
    float bodyRadius;

    // Positions
    sf::Vector2f position;
    sf::Vector2f initialPosition;
    sf::Vector2f targetPosition;
    sf::Vector2f heading;

    // Velocity
    sf::Vector2f velocity;
    sf::Vector2f initialVelocity;
    float velocityMagnitude;
    float minVelocity;
    float maxVelocity;
    float velocityMu;
    float velocitySigma;
    float velocityNoiseFactor;
    float velocityNoiseScale;

    // Acceleration
    sf::Vector2f acceleration;
    sf::Vector2f initialAcceleration;
    float accelerationMagnitude;
    float minAcceleration;
    float maxAcceleration;

    // Trajectory
    std::vector<sf::Vector2f>(trajectory);
    float waypointDistance;
    sf::Color waypointColor;
    int nextWaypointIndex = -1;

    // Visuals
    float bufferZoneRadius;
    float minBufferZoneRadius;
    sf::Color bufferZoneColor;

    // States
    bool collisionPredicted;
    bool stopped;
    bool isActive;
    int stoppedFrameCounter;

    // Perlin noise
    unsigned int noiseSeed;
    PerlinNoise perlinNoise;
};