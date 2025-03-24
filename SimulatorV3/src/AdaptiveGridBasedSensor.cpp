#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <iostream>

#include "../include/AdaptiveGridBasedSensor.hpp"

// Constructor
AdaptiveGridBasedSensor::AdaptiveGridBasedSensor(
    float frameRate, 
    sf::FloatRect detectionArea, 
    float cellSize, 
    int maxDepth,
    const std::string& databaseName, 
    const std::string& collectionName,
    std::shared_ptr<mongocxx::client> client
)

    : Sensor(frameRate, detectionArea, client), 
    cellSize(cellSize), // Half the longest side of the detection area
    maxDepth(maxDepth),
    db(client->database(databaseName)),
    collection(db[collectionName]), 
    adaptiveGrid(detectionArea.left, detectionArea.top, cellSize, maxDepth) {
        this->detectionArea = detectionArea;
    }

// Alternative constructor for rendering
AdaptiveGridBasedSensor::AdaptiveGridBasedSensor(
    sf::FloatRect detectionArea, 
    sf::Color detectionAreaColor, 
    float cellSize, 
    int maxDepth,
    bool showGrid
)
    : Sensor(detectionArea, detectionAreaColor), 
    cellSize(cellSize), adaptiveGrid(detectionArea.left, detectionArea.top, cellSize, maxDepth), maxDepth(maxDepth) {
        this->detectionArea = detectionArea;
        this->detectionAreaColor = detectionAreaColor;
        this->showGrid = showGrid;
    }

// Destructor
AdaptiveGridBasedSensor::~AdaptiveGridBasedSensor() {
    
    // Empty
    adaptiveGridData.clear();
    dataStorage.clear();
    adaptiveGrid.reset(); // Reset instead of clear
}

// Update grid-based agent detection and output one gridData entry per frame
void AdaptiveGridBasedSensor::update(std::vector<Agent>& agents, float timeStep, sf::Time simulationTime, std::string datetime) {

    adaptiveGrid.printChildren(12);
    adaptiveGrid.printChildren(13);
    adaptiveGrid.printChildren(14);
    adaptiveGrid.printChildren(15);
    // Clear data storage
    dataStorage.clear();

    std::string currentTime = generateISOTimestamp(simulationTime, datetime);

    // Update the time since the last update
    timeSinceLastUpdate += timeStep;

    // Update the estimated velocities at the specified frame rate
    if (timeSinceLastUpdate >= 1.0f / frameRate) {

        // Clear the grid data
        adaptiveGridData.clear();
        adaptiveGrid.agents.clear();
        adaptiveGrid.positions.clear();

        // Reset boolean flag
        bool hasAgents = false;

        // Declare a variable to store the cell index
        int cellId;

        // Iterate through the agents
        for (Agent& agent : agents) {
            if (detectionArea.contains(agent.position)) {
                adaptiveGrid.agents.push_back(&agent);
                adaptiveGrid.positions.push_back(agent.position);
                hasAgents = true;
            }
        }

        if(hasAgents) {

            // Generate split sequence
            adaptiveGrid.splitFromPositions();

            for (Agent* agentPtr : adaptiveGrid.agents) {
                Agent& agent = *agentPtr;

                // Add the agent to the adaptive grid
                cellId = adaptiveGrid.addAgent(&agent);

                // Increment the count of the agent type in the cell
                adaptiveGridData[cellId][agent.type]++;
            }

            // Reset the time since the last update
            timeSinceLastUpdate = 0.0f;
            dataStorage.push_back({currentTime, adaptiveGridData}); // Pushing exactly one entry to dataStorage
        }
    }
}

// Post timestamped grid data to the database
void AdaptiveGridBasedSensor::postData() {

    // Check if there is data to post
    if(!dataStorage.empty()) {

        // Create a vector of documents for bulk insert
        std::vector<bsoncxx::document::value> documents;

        // Iterate through the data storage (currently only one entry in dataStorage)
        for (const auto& [timestamp, adaptiveGridData] : dataStorage) {

            // Iterate through the grid data
            for (const auto& [cellId, cellData] : adaptiveGridData) {
                
                // Document for the grid cell
                bsoncxx::builder::stream::document document{}; 
                document << "timestamp" << timestamp
                         << "sensor_id" << sensorId
                         << "data_type" << "adaptive_grid_data"
                         << "cell_id" << cellId
                         << "cell_position"
                            << bsoncxx::builder::stream::open_array
                            << adaptiveGrid.getCellPosition(cellId).x
                            << adaptiveGrid.getCellPosition(cellId).y
                            << bsoncxx::builder::stream::close_array;
            
                // Embed agent counts as a subdocument
                int totalAgents = 0;

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
void AdaptiveGridBasedSensor::postMetadata() {

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
             << "sensor_id" << sensorId
             << "sensor_type" << "adaptive-grid-based"
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
void AdaptiveGridBasedSensor::printData() {

    auto currentTime = std::chrono::system_clock::now();

    std::time_t now = std::chrono::system_clock::to_time_t(currentTime);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%FT%TZ");
    
    // Print grid data
    for (const auto& kvp : adaptiveGridData) { // key-value pair

        const int& cellId = kvp.first;
        const std::unordered_map<std::string, int>& cellData = kvp.second;

        std::cout << "Current time: " << ss.str() << " Cell ID(" << cellId << "): ";

        // Going through key-value pairs in the cell data
        for (const auto& kvp : cellData) {
            
            // Print agent type and count
            const std::string& agentType = kvp.first;
            int count = kvp.second;
            std::cout << agentType << ": " << count << ", ";
        }
        std::cout << std::endl;
    }
    adaptiveGridData.clear();
}

// Clear the database
void AdaptiveGridBasedSensor::clearDatabase() {

    // Clear the collection
    collection.delete_many({});
}