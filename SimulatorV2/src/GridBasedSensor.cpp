#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>

#include "../include/GridBasedSensor.hpp"

// Constructor
GridBasedSensor::GridBasedSensor(
    float frameRate, sf::FloatRect detectionArea, float cellSize, 
    const std::string& databaseName, const std::string& collectionName,
    std::shared_ptr<mongocxx::client> client)

    : Sensor(frameRate, detectionArea, client), 
    cellSize(cellSize), 
    db(client->database(databaseName)),
    collection(db[collectionName]), 
    currentGrid(cellSize, detectionArea.width, detectionArea.height), 
    previousGrid(cellSize, detectionArea.width, detectionArea.height) {
        this->detectionArea = detectionArea;
    }

// Alternative constructor for rendering
GridBasedSensor::GridBasedSensor(
    sf::FloatRect detectionArea, sf::Color detectionAreaColor, float cellSize, 
    bool showGrid)
    : Sensor(detectionArea, detectionAreaColor), 
    cellSize(cellSize), currentGrid(cellSize, detectionArea.width, detectionArea.height), 
    previousGrid(cellSize, detectionArea.width, detectionArea.height) {
        this->detectionArea = detectionArea;
        this->detectionAreaColor = detectionAreaColor;
        this->showGrid = showGrid;
    }

// Destructor
GridBasedSensor::~GridBasedSensor() {
    
    // Empty
    gridData.clear();
    dataStorage.clear();
    currentGrid.clear();
    previousGrid.clear();
}

// Update grid-based agent detection and output one gridData entry per frame
void GridBasedSensor::update(std::vector<Agent>& agents, float timeStep, sf::Time simulationTime, std::string datetime) {

    // Clear data storage
    dataStorage.clear();

    std::string currentTime = generateISOTimestamp(simulationTime, datetime);

    // Update the time since the last update
    timeSinceLastUpdate += timeStep;

    // Update the estimated velocities at the specified frame rate
    if (timeSinceLastUpdate >= 1.0f / frameRate) {

        // Clear the grid data
        gridData.clear();

        // Store the previous grid data
        previousGrid = currentGrid;
        currentGrid.clear();

        // Reset boolean flag
        bool hasAgents = false;

        // Declare a variable to store the cell index
        sf::Vector2i cellIndex;

        // Iterate through the agents
        for (Agent& agent : agents) {

            // Check if the agent is within the detection area
            if (detectionArea.contains(agent.position)) {
                
                // Set the flag to true
                hasAgents = true;

                // Get the cell index of the agent's position
                cellIndex = getCellIndex(agent.position);

                // Add the agent to the current grid
                currentGrid.addAgent(&agent);

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
                document << "timestamp" << timestamp
                         << "sensor_id" << sensor_id
                         << "data_type" << "grid_data"
                         << "cell_index" // Vector2i as an array
                            << bsoncxx::builder::stream::open_array 
                            << cellIndex.x 
                            << cellIndex.y
                            << bsoncxx::builder::stream::close_array
                         << "cell_position"
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

                    // Open a document for each agent type and count
                    agentTypeBuilder << bsoncxx::builder::stream::open_document
                                << "type" << agentType
                                << "count" << count
                                << bsoncxx::builder::stream::close_document;  // Close the document
                    
                    // Increment the total agent count
                    totalAgents += count;
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

// Post metadata to the database
void GridBasedSensor::postMetadata() {

    // Prepare a BSON document for the metadata
    bsoncxx::builder::stream::document document{};

    // // Append the metadata fields to the document
    // document << "timestamp" << timestamp
    //          << "sensor_id" << sensor_id
    //          << "data_type" << "metadata"
    //          << "position"
    //             << bsoncxx::builder::stream::open_array
    //             << detectionArea.left
    //             << detectionArea.top
    //             << bsoncxx::builder::stream::close_array
    //          << "detection_area" 
    //             << bsoncxx::builder::stream::open_array
    //             << detectionArea.width
    //             << detectionArea.height
    //             << bsoncxx::builder::stream::close_array
    //          << "frame_rate" << frameRate
    //          << "cell_size" << cellSize;
    // Append the metadata fields to the document

    bsoncxx::builder::stream::document positionDocument{}, detectionAreaDocument{};
    positionDocument << "x" << detectionArea.left
                      << "y" << detectionArea.top;
    detectionAreaDocument << "width" << detectionArea.width
                          << "height" << detectionArea.height;

    document << "timestamp" << timestamp
             << "sensor_id" << sensor_id
             << "sensor_type" << "grid-based"
             << "data_type" << "metadata"
             << "position" << positionDocument
             << "detection_area" << detectionAreaDocument
             << "frame_rate" << frameRate
             << "cell_size" << cellSize;

    // Insert the metadata document into the collection
    try {
        collection.insert_one(document << bsoncxx::builder::stream::finalize);
    } catch (const mongocxx::exception& e) {
        // Handle errors
        std::cerr << "Error inserting metadata: " << e.what() << std::endl;
    }
}

// Print grid data -> TODO: Time stamp not accurate, use dataStorage instead
void GridBasedSensor::printData() {

    auto currentTime = std::chrono::system_clock::now();

    std::time_t now = std::chrono::system_clock::to_time_t(currentTime);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%FT%TZ");
    
    // Print grid data
    for (const auto& kvp : gridData) { // key-value pair

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

// Clear the database
void GridBasedSensor::clearDatabase() {

    // Clear the collection
    collection.delete_many({});
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