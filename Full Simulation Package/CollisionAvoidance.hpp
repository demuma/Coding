#ifndef COLLISIONAVOIDANCE_HPP
#define COLLISIONAVOIDANCE_HPP

#include "Agent.hpp"
#include "Obstacle.hpp"

// Function declaration
bool predictCollisionAgents_v1(Agent& agent1, Agent& agent2);
bool predictCollisionAgents_v2(Agent& agent1, Agent& agent2);
bool predictCollisionAgents(Agent& agent1, Agent& agent2);
bool predictCollisionObstacle(Agent& agent, const std::vector<Obstacle>& obstacles);
bool agentAgentCollision(Agent& agent1, Agent& agent2);
bool agentObstacleCollision(Agent& agent, const std::vector<Obstacle>& obstacles);
bool collisionPossible(Agent& agent1, Agent& agent2);

#endif // COLLISIONAVOIDANCE_HPP
