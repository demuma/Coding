#include "CollisionPrediction.h"
#include <cmath> // For std::sqrt

// Function definition (implementation)
bool predictCollision(Agent& agent1, Agent& agent2) {
    const float lookaheadStep = 0.1f; // Time step for predictions
    const float maxLookahead = 3.0f; // Maximum lookahead time

    for (float t = 0; t <= maxLookahead; t += lookaheadStep) {
        sf::Vector2f futurePos1 = agent1.getFuturePositionAtTime(t);
        sf::Vector2f futurePos2 = agent2.getFuturePositionAtTime(t);

        // Check if the future positions (including buffer radius) intersect
        float dx = futurePos1.x - futurePos2.x;
        float dy = futurePos1.y - futurePos2.y;
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < agent1.bufferRadius + agent2.bufferRadius) {
            agent1.bufferColor = sf::Color::Red;
            agent2.bufferColor = sf::Color::Red;
            agent1.hasCollision = true;
            agent2.hasCollision = true;

            // Implement collision avoidance: stop the slower agent
            float speed1 = std::sqrt(agent1.velocity.x * agent1.velocity.x + agent1.velocity.y * agent1.velocity.y);
            float speed2 = std::sqrt(agent2.velocity.x * agent2.velocity.x + agent2.velocity.y * agent2.velocity.y);

            if (speed1 < speed2) {
                agent1.stop(); // Slower agent stops
            } else {
                agent2.stop(); // Slower agent stops
            }

            return true; // Collision detected
        }
    }

    return false; // No collision detected in the lookahead time frame
}
