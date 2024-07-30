# void Agent::calculateTrajectory(float waypointDistance) {

#     trajectory.clear();

#     // Add the start as the first waypoint
#     trajectory.push_back(start);

#     // Set the initial position
#     sf::Vector2f currentPosition = start;

#     // Calculate the number of intermediate waypoints
#     int numWaypoints = std::floor(std::sqrtf(std::powf((target.x - start.x), 2) + std::powf((target.y - start.y), 2)) / waypointDistance);
    
#     if(numWaypoints < 1) {
#         trajectory.push_back(target);
#         return;
#     } 

#     // Calculate the componental distance between waypoints (intermediate and last waypoint)
#     float waypointDistanceX = (target.x - start.x) / numWaypoints + 1;
#     float waypointDistanceY = (target.y - start.y) / numWaypoints + 1;

#     // Calculate intermediate waypoints
#     for (int i = 1; i < static_cast<int>(std::floor(numWaypoints)); ++i) {
#         while (currentPosition.x < target.x) {
#             currentPosition.x += waypointDistanceX;
#             currentPosition.y += waypointDistanceY;
#             trajectory.push_back(currentPosition);
#         }
#     }

#     // Add the target as the last waypoint
#     trajectory.push_back(target);
# }

import numpy as np
import math

agentStart = [0, 3]
agentEnd = [16, 3]
waypointDistance = 4
trajectory = []

def calculateTrajectory(waypointDistance):
    trajectory.clear()
    trajectory.append(agentStart[:])

    currentPosition = agentStart

    totalDistance = math.sqrt(math.pow((agentEnd[0] - agentStart[0]), 2) + math.pow((agentEnd[1] - agentStart[1]), 2))
    numWaypoints = math.floor(totalDistance / waypointDistance)
    
    if numWaypoints < 1:
        trajectory.append(agentEnd)
        return
    
    angle = math.atan2(agentEnd[1] - agentStart[1], agentEnd[0] - agentStart[0]) # in degrees
    
    deltaX = waypointDistance * math.cos(angle)
    deltaY = waypointDistance * math.sin(angle)

    for i in range(0, numWaypoints):

        currentPosition[0] += deltaX
        currentPosition[1] += deltaY
        trajectory.append(currentPosition[:])

    trajectory.append(agentEnd)

    return trajectory

print(calculateTrajectory(waypointDistance))
