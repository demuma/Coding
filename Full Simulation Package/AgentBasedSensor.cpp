#include "AgentBasedSensor.hpp"
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>

// Constructor
AgentBasedSensor::AgentBasedSensor(
    float frameRate, 
    sf::FloatRect detectionArea, 
    sf::Color detectionAreaColor,
    const std::string& databaseName,
    const std::string& collectionName,
    std::shared_ptr<mongocxx::client> client)
    : Sensor(frameRate, detectionArea, 
        detectionAreaColor, client), 
        db(client->database(databaseName)), // Initialize db in initializer list
        collection(db[collectionName]) {
        this->detectionArea = detectionArea;
        this->detectionAreaColor = detectionAreaColor;
    }

// Destructor
AgentBasedSensor::~AgentBasedSensor() {}

// Update method for agent-based sensor, taking snapshot of agents in detection area
void AgentBasedSensor::update(const std::vector<Agent>& agents, float deltaTime, int frameCount, sf::Time totalElapsedTime, std::string datetime) {
    
    // Clear the data storage
    dataStorage.clear();

    // Update the time since the last update
    timeSinceLastUpdate += deltaTime;

    // Update the estimated velocities at the specified frame rate
    // if (timeSinceLastUpdate >= 1.0f / frameRate || frameCount != 0) {
    if (timeSinceLastUpdate >= 1.0f / frameRate) {

        // Capture agent data (Modified)
        captureAgentData(agents, totalElapsedTime, datetime);

        // Estimate the velocities of the agents
        estimateVelocities(estimatedVelocities);

        // Reset for next update
        previousPositions = currentPositions;
        currentPositions.clear();
        timeSinceLastUpdate = 0.0f;
    }
}

// Make a snapshot of the agents in the detection area
void AgentBasedSensor::captureAgentData(const std::vector<Agent>& agents, sf::Time totalElapsedTime, std::string date) {

    // Capture the current positions of the agents
    for (const Agent& agent : agents) {

        // Check if the agent is within the detection area
        if (detectionArea.contains(agent.position)) {

            // Prepare SensorData object for the agent
            AgentBasedSensorData agentData;
            agentData.sensor_id = sensor_id;
            agentData.agent_id = agent.uuid;
            agentData.timestamp = generateISOTimestamp(totalElapsedTime, date);
            agentData.type = agent.type;
            agentData.position = agent.position;

            // Estimate and store the velocity of the agent TODO: Only save with velocity
            if (previousPositions.find(agent.uuid) != previousPositions.end()) {
                sf::Vector2f prevPos = previousPositions[agent.uuid];
                agentData.estimatedVelocity = (agent.position - previousPositions[agent.uuid]) * frameRate;
            }

            // Store the agent data
            dataStorage.push_back(agentData);

            // Store the current position of a specific agent using its UUID 
            currentPositions[agent.uuid] = agent.position;
        }
    }
}

// Post data method for agent-based sensor
void AgentBasedSensor::postData() {

    // Check if there is data to post
    if (!dataStorage.empty()) {

        // Prepare a vector of BSON documents to insert in bulk
        std::vector<bsoncxx::document::value> documents;
        documents.reserve(dataStorage.size());

        // Iterate through the stored data
        for (const auto& agentData : dataStorage) {

            // Construct a BSON document for each agent data object
            bsoncxx::builder::stream::document document{};

            // Append the fields to the document
            document << "timestamp" << agentData.timestamp
                     << "sensor_id" << agentData.sensor_id
                     << "agent_id" << agentData.agent_id
                     << "type" << agentData.type
                     << "position" 
                        << bsoncxx::builder::stream::open_array
                        << agentData.position.x 
                        << agentData.position.y
                        << bsoncxx::builder::stream::close_array
                     << "estimated_velocity"
                        << bsoncxx::builder::stream::open_array
                        << agentData.estimatedVelocity.x 
                        << agentData.estimatedVelocity.y
                        << bsoncxx::builder::stream::close_array;

            documents.push_back(document << bsoncxx::builder::stream::finalize);
        }

        try {
            // Bulk insert the documents into the collection
            collection.insert_many(documents);
        } catch (const mongocxx::exception& e) {
            // Handle errors
            std::cerr << "Error inserting agent data: " << e.what() << std::endl;
        }
    }
}

void AgentBasedSensor::printData() {

    // Print the stored data
    for (const AgentBasedSensorData& data : dataStorage) {

        std::cout << "  Timestamp: " << data.timestamp << std::endl;
        std::cout << "  Sensor ID: " << data.sensor_id << std::endl;
        std::cout << "  Agent ID: " << data.agent_id << std::endl;
        std::cout << "  Agent Type: " << data.type << std::endl;
        std::cout << "  Position: (" << data.position.x << ", " << data.position.y << ")" << std::endl;
        std::cout << "  Estimated Velocity: (" << data.estimatedVelocity.x << ", " << data.estimatedVelocity.y << ")" << std::endl;
        std::cout << std::endl;
    }
}