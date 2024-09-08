#include "GridBasedSensor.hpp"
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>

// Constructor
GridBasedSensor::GridBasedSensor(

    float frameRate, sf::FloatRect detectionArea, sf::Color detectionAreaColor, float cellSize, 
    bool showGrid, const std::string& databaseName, const std::string& collectionName,
    std::shared_ptr<mongocxx::client> client)

    : Sensor(frameRate, detectionArea, detectionAreaColor, client), 
    cellSize(cellSize),
    db(client->database(databaseName)), // Initialize db in initializer list
    collection(db[collectionName]) {
        this->detectionArea = detectionArea;
        this->detectionAreaColor = detectionAreaColor;
        this->showGrid = showGrid;
    }

// Destructor
GridBasedSensor::~GridBasedSensor() {
    
    // Empty
    gridData.clear();
    dataStorage.clear();
}

// Update grid-based agent detection and output one gridData entry per frame
void GridBasedSensor::update(const std::vector<Agent>& agents, float deltaTime, int frameCount, sf::Time totalElapsedTime, std::string datetime) {

    // Clear data storage
    dataStorage.clear();

    std::string currentTime = generateISOTimestamp(totalElapsedTime, datetime);

    // Update the time since the last update
    timeSinceLastUpdate += deltaTime;

    // Update the estimated velocities at the specified frame rate
    // if (timeSinceLastUpdate >= 1.0f / frameRate || frameCount != 0) {
    if (timeSinceLastUpdate >= 1.0f / frameRate) {

        // Clear the grid data
        gridData.clear();

        // Reset boolean flag
        bool hasAgents = false;

        // Declare a variable to store the cell index
        sf::Vector2i cellIndex;

        // Iterate through the agents
        for (const Agent& agent : agents) {

            // Check if the agent is within the detection area
            if (detectionArea.contains(agent.position)) {
                
                // Set the flag to true
                hasAgents = true;

                // Get the cell index of the agent's position
                cellIndex = getCellIndex(agent.position);

                // Increment the count of the agent type in the cell
                gridData[cellIndex][agent.type]++;
            }
        }

        // Clear the previous positions and reset the time since the last update
        if(hasAgents) {

            timeSinceLastUpdate = 0.0f;
            dataStorage.push_back({currentTime, gridData}); // Pushing exactly one entry to dataStorage
        }
    }
}

// Post timestamped grid data to the database
void GridBasedSensor::postData() {

    // Check if there is data to post
    if(!dataStorage.empty()) {

        // Create a vector of documents for bulk insert
        std::vector<bsoncxx::document::value> documents;

        // Iterate through the data storage (currently only one entry in dataStorage)
        for (const auto& [timestamp, gridData] : dataStorage) {

            // Iterate through the grid data
            for (const auto& [cellIndex, cellData] : gridData) {
                
                // Document for the grid cell
                bsoncxx::builder::stream::document document{}; 
                document << "timestamp" << timestamp;
                document << "sensor_id" << sensor_id;
                document << "cell_index" // Vector2i as an array
                    << bsoncxx::builder::stream::open_array 
                    << cellIndex.x 
                    << cellIndex.y
                    << bsoncxx::builder::stream::close_array;
                document << "cell_position"
                    << bsoncxx::builder::stream::open_array
                    << getCellPosition(cellIndex).x
                    << getCellPosition(cellIndex).y
                    << bsoncxx::builder::stream::close_array;
            
                // Embed agent counts as a subdocument
                int totalAgents = 0;
                std::vector<int> numbers = {1, 2, 3, 4, 5};

                // Open an array for the agent type count
                auto agentTypeBuilder = document << "agent_type_count" << bsoncxx::builder::stream::open_array;

                // Loop through the vector and add each element to the array
                for (const auto& [agentType, count] : cellData) {

                    totalAgents += count;
                    // Open a document for each agent type and count
                    agentTypeBuilder << bsoncxx::builder::stream::open_document
                                << "type" << agentType
                                << "count" << count
                                << bsoncxx::builder::stream::close_document;  // Close the document
                }

                // Close the array
                agentTypeBuilder << bsoncxx::builder::stream::close_array;

                // Add total agent count for the cell
                document << "total_agents" << totalAgents;
                
                // Add the document to the vector for bulk insert
                documents.push_back(document << bsoncxx::builder::stream::finalize);
            }
        }

        // Bulk insert the documents into the collection
        try {
            collection.insert_many(documents);
        } catch (const mongocxx::exception& e) {
            // Handle errors
            std::cerr << "Error inserting agent data: " << e.what() << std::endl;
        }
    }
}

// Print grid data -> TODO: Time stamp not accurate, use dataStorage instead
void GridBasedSensor::printData() {

    auto currentTime = std::chrono::system_clock::now();

    std::time_t now = std::chrono::system_clock::to_time_t(currentTime);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%FT%TZ");
    
    // Print grid data
    for (const auto& kvp : gridData) {

        const sf::Vector2i& cellIndex = kvp.first;
        const std::unordered_map<std::string, int>& cellData = kvp.second;

        std::cout << "Current time: " << ss.str() << " Cell (" << cellIndex.x << ", " << cellIndex.y << "): ";

        // Going through key-value pairs in the cell data
        for (const auto& kvp : cellData) {
            
            // Print agent type and count
            const std::string& agentType = kvp.first;
            int count = kvp.second;
            std::cout << agentType << ": " << count << ", ";
        }
        std::cout << std::endl;
    }
    gridData.clear();

}

// Helper function to get cell index based on position
sf::Vector2i GridBasedSensor::getCellIndex(const sf::Vector2f& position) const {

    int x = static_cast<int>((position.x - detectionArea.left) / cellSize);
    int y = static_cast<int>((position.y - detectionArea.top)/ cellSize);

    return sf::Vector2i(x, y);
}

sf::Vector2f GridBasedSensor::getCellPosition(const sf::Vector2i& cellIndex) const {

    float x = (cellIndex.x * cellSize + detectionArea.left + cellSize / 2) / scale;
    float y = (cellIndex.y * cellSize + detectionArea.top + cellSize / 2) / scale;

    return sf::Vector2f(x, y);
}