#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath> // For trigonometry functions
#include <functional>
#include <random>
#include <iostream>

// Simple Agent class (you'll likely have a more complex one)
class Agent {
public:
    sf::Vector2f position;
    sf::Vector2f initial_position;
    sf::Vector2f velocity;
    float radius = 5.0f; // Example radius
    float bufferRadius;
    sf::Color color = sf::Color::Red; // Example color
    sf::Color originalColor = color;
    // sf::Color color = sf::Color(rand() % 255, rand() % 255, rand() %);

    void updatePosition() {
        position += velocity;
    }

    sf::CircleShape getFuturePosition(float deltaTime) const {
        sf::Vector2f futurePos = position + velocity * deltaTime;
        sf::CircleShape futureShape(radius);
        futureShape.setOrigin(radius, radius);
        futureShape.setPosition(futurePos);
        return futureShape;
    }
};

int main() {
    // Window setup
    sf::RenderWindow window(sf::VideoMode(3440, 1440), "Road User Simulation");
    window.setFramerateLimit(30); // Cap FPS for smoother visuals

    std::random_device rd;   
    std::mt19937 gen(rd());  
    std::uniform_real_distribution<> dis(-4.0, 4.0);

    // Agent initialization (example)
    std::vector<Agent> agents;
    for (int i = 0; i < 500; ++i) {
        Agent agent;
        agent.position = sf::Vector2f(rand() % 3440, rand() % 1440);
        agent.initial_position = agent.position;
        agent.velocity = sf::Vector2f(dis(gen), dis(gen)); // Random velocity
        agents.push_back(agent);
    }

    // Initialize clock
    sf::Clock clock;
    float deltaTime = 0.f; // Time elapsed since last frame

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        deltaTime = clock.restart().asSeconds();

        // Update agent positions
        for (auto& agent : agents) {
            agent.updatePosition();
        }

        // Collision detection
        for (size_t i = 0; i < agents.size(); ++i) {
            for (size_t j = i + 1; j < agents.size(); ++j) {
                Agent& agent1 = agents[i];
                Agent& agent2 = agents[j];

                // Calculate lookahead distance (you can adjust this formula)
                float lookaheadDistance1 = agent1.radius + agent1.velocity.x * deltaTime;
                float lookaheadDistance2 = agent2.radius + agent2.velocity.x * deltaTime;

                // Get future positions
                sf::CircleShape futurePos1 = agent1.getFuturePosition(deltaTime);
                sf::CircleShape futurePos2 = agent2.getFuturePosition(deltaTime);

                // Check if buffer zones of future positions intersect
                if (futurePos1.getGlobalBounds().intersects(futurePos2.getGlobalBounds())) {
                    // Collision detected!
                    agent1.color = sf::Color::Yellow; // Change color to indicate collision (optional)
                    agent2.color = sf::Color::Yellow;
                } 
            }
        }

        // Rendering
        window.clear(sf::Color::White); // Clear the window with a white background
        
        for (const auto& agent : agents) {
            // Draw the agent as a circle
            sf::CircleShape agentShape(agent.radius);
            agentShape.setFillColor(agent.color);
            agentShape.setOrigin(agent.radius, agent.radius);
            agentShape.setPosition(agent.position);
            window.draw(agentShape); 

            // Calculate buffer zone radius based on velocity
            float velocityMagnitude = std::sqrt(agent.velocity.x * agent.velocity.x + 
                                                agent.velocity.y * agent.velocity.y);
            float bufferRadius = agent.radius * (2 + velocityMagnitude / 2.0f); // Scale factor of 10 (adjust as needed)

            // Draw the buffer zone
            sf::CircleShape bufferZone(bufferRadius); // Double the radius
            bufferZone.setOrigin(bufferRadius, bufferRadius);
            bufferZone.setPosition(agent.position);
            bufferZone.setFillColor(sf::Color::Transparent); // No fill
            bufferZone.setOutlineThickness(2.f);            // Outline thickness
            bufferZone.setOutlineColor(sf::Color::Green);  // Orange outline
            window.draw(bufferZone);

            // Draw the arrow (a triangle)
            sf::Vector2f direction = agent.velocity;
            float arrowLength = agent.radius * 0.8f; 
            float arrowAngle = std::atan2(direction.y, direction.x);

            // Normalize the direction vector to have a length of 1 (unit vector)
            sf::Vector2f norm_vector = direction / std::sqrt(direction.x * direction.x + direction.y * direction.y); 

            sf::ConvexShape arrow(3); 
            arrow.setPoint(0, sf::Vector2f(0, 0)); // Tip of the arrow
            arrow.setPoint(1, sf::Vector2f(-arrowLength, arrowLength / 2));
            arrow.setPoint(2, sf::Vector2f(-arrowLength, -arrowLength / 2));
            arrow.setFillColor(sf::Color::Black); 

            // Draw trajectory
            sf::Vertex trajectory[] = 
            {
                sf::Vertex(agent.initial_position, sf::Color::Red),
                sf::Vertex(agent.position, sf::Color::Red)
            };
            

            // Line (arrow body) - Offset the start by the agent's radius
            sf::Vertex line[] =
            {
                sf::Vertex(agent.position + norm_vector * agent.radius, sf::Color::Black),  // Offset start point
                sf::Vertex(agent.position + norm_vector * agent.radius + direction * (arrowLength / 2.0f), sf::Color::Black) // Ending point (base of arrowhead)
            };

            // Set the origin of the arrowhead to the base of the triangle
            arrow.setOrigin(-arrowLength, 0); // Origin at the base of the arrowhead
            arrow.setPosition(line[1].position); // Position the arrowhead at the end of the line
            arrow.setRotation(arrowAngle * 180.0f / M_PI);
            window.draw(arrow);
            window.draw(trajectory, 2, sf::Lines);
            window.draw(line, 2, sf::Lines); 
        }

        window.display();
    }

    return 0;
}
