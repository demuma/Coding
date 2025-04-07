#include "../include/Visualizer.hpp"
#include "../include/Logging.hpp"
#include "../include/Utilities.hpp"

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

Visualizer::Visualizer() 
:   gridLinesVertexArray(sf::PrimitiveType::Lines)
{
    loadConfiguration();
    loadAgentsAttributes();
    initializeDatabase();
    initializeWindow();
    getMetadata();
    getData();
}

Visualizer::~Visualizer() {

}

void Visualizer::loadConfiguration() {

    // Load the configuration file
    config = YAML::LoadFile("config.yaml");

    // Load window parameters
    windowSize.x = config["display"]["width"].as<int>(); // Pixels
    windowSize.y = config["display"]["height"].as<int>(); // Pixels
    scale = config["display"]["pixels_per_meter"].as<float>();

    // Load simulation parameters
    simulationSize.x = config["simulation"]["width"].as<int>(); // Pixels
    simulationSize.y = config["simulation"]["height"].as<int>(); // Pixels

    // Scale
    simulationSize.x *= scale;
    simulationSize.y *= scale;

    // Calculate the offset
    offset = sf::Vector2f((windowSize.x - simulationSize.x) / 2, (windowSize.y - simulationSize.y) / 2); // Pixels
    
    // Load database parameters
    std::string dbHost = config["database"]["host"].as<std::string>();
    int dbPort = config["database"]["port"].as<int>();
    databaseName = config["database"]["db_name"].as<std::string>();
    dbUri = "mongodb://" + dbHost + ":" + std::to_string(dbPort);

    // Visualization parameters
    showGrids = config["renderer"]["show_grids"].as<bool>();

    // Video parameters
    makeVideo = config["simulation"]["make_video"].as<bool>();
}

void Visualizer::loadAgentsAttributes() {

    // Extract agent attributes
    if (config["agents"] && config["agents"]["road_user_taxonomy"]) {
        for (const auto& agent : config["agents"]["road_user_taxonomy"]) {
            std::string type = agent["type"].as<std::string>();
            Agent::AgentTypeAttributes attributes;

            attributes.color = agent["color"].as<std::string>();
            attributes.priority = agent["priority"].as<int>();

            // Store in map
            agentTypeAttributes[type] = attributes;

            // Add to allAgentTypes
            allAgentTypes.push_back(type);
        }
    }

    // Initialize ghostCellAgentCounts with zero counts for all agent types
    for (const auto& agentType : allAgentTypes) {
        ghostCellAgentCounts[agentType] = 0;
    }
}

void Visualizer::initializeDatabase() {

    mongocxx::uri uri(dbUri);
    client = std::make_shared<mongocxx::client>(uri);
    db = (*client)[databaseName];
    collection = db["GB_Sensor_Data"];
}

// Initialize the window
void Visualizer::initializeWindow() {

    // Turn on antialiasing
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;

    // Create window
    window.create(sf::VideoMode(windowSize.x, windowSize.y), "Urban Data Visualizer", sf::Style::Default, settings);

    // Initialize the render texture
    if (!renderTexture.create(windowSize.x, windowSize.y)) {
        std::cerr << "Error: Could not create render texture." << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Visualizer::loadSensorAttributes() {

    // Extract sensor attributes
    if (config["sensors"]) {
        for (const auto& sensorConfig : config["sensors"]) {
            std::string type = sensorConfig["sensor_type"].as<std::string>();
            Sensor sensor;

            sensor.frameRate =config["frame_rate"].as<float>();
            sensor.color = stringToColor(sensorConfig["color"].as<std::string>());
            sensor.alpha = sensorConfig["alpha"].as<float>() * 256;
            sensor.databaseName = sensorConfig["database_name"].as<std::string>();
            sensor.collectionName = sensorConfig["collection_name"].as<std::string>();

            if(sensorConfig["grid-based"]) {
                sensor.cellSize = sensorConfig["cell_size"].as<float>();
                sensor.showGrid = sensorConfig["grid"]["show_grid"].as<bool>();
            }

            sensorTypeAttributes[type] = sensor;
        }
    }
}

// Get metadata from database
void Visualizer::getMetadata() {

    // auto collection = (*client)[databaseName]["AB_Sensor_Data"]; 

    // Get sensor data
    auto metadataCursor = collection.find_one(make_document(kvp("data_type", "metadata")));
    if (!metadataCursor) {
        std::cerr << "Error: Metadata not found." << std::endl;
        return; 
    }

    DEBUG_MSG("Metadata: " << bsoncxx::to_json(*metadataCursor));

    // Extract metadata (adjust based on your actual metadata structure)
    bsoncxx::document::view document = metadataCursor->view();
    float detectionWidth = document["detection_area"]["width"].get_double().value * scale;
    float detectionHeight = document["detection_area"]["height"].get_double().value * scale;
    frameRate = document["frame_rate"].get_double().value;
    sf::Vector2f position = {
        static_cast<float>(document["position"]["x"].get_double().value * scale), 
        static_cast<float>(document["position"]["y"].get_double().value * scale)
    };

    if(document["sensor_type"].get_string().value.to_string() == "agent-based") {

        Sensor sensor;
        sensor.sensorID = document["sensor_id"].get_string().value.to_string();
        sensor.type = document["sensor_type"].get_string().value.to_string();
        sensor.frameRate = document["frame_rate"].get_double().value;
        sensor.color = stringToColor("Magenta");
        sensor.detectionArea = sf::FloatRect(position.x , position.y, detectionWidth, detectionHeight);
        sensor.alpha = 80;

        sensors.push_back(sensor);
    }
    else if(document["sensor_type"].get_string().value.to_string() == "grid-based") {

        Sensor sensor;
        sensor.sensorID = document["sensor_id"].get_string().value.to_string();
        sensor.type = document["sensor_type"].get_string().value.to_string();
        sensor.frameRate = document["frame_rate"].get_double().value;
        sensor.color = stringToColor("Blue");
        sensor.detectionArea = sf::FloatRect(position.x , position.y, detectionWidth, detectionHeight);
        sensor.alpha = 80;
        sensor.cellSize = document["cell_size"].get_double().value * scale;;

        sensors.push_back(sensor);
    }
    else {
        std::cerr << "Error: Invalid sensor type." << std::endl;
    }

}

void Visualizer::getData() {
    mongocxx::pipeline pipeline;

    // Filter out metadata entries
    pipeline.match(make_document(kvp("data_type", "grid data")));

    // Group by timestamp and collect grid cells in an array
    pipeline.group(make_document(
        kvp("_id", "$timestamp"),
        kvp("grid_cells", make_document(kvp("$push", "$$ROOT")))
    ));

    // Sort the results in ascending order of timestamp
    pipeline.sort(make_document(kvp("_id", 1)));

    // Execute the aggregation and catch any exceptions
    mongocxx::cursor cursor = collection.find({}, {});
    try {
        cursor = collection.aggregate(pipeline);
    } catch (const mongocxx::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return;
    }

    // Iterate over the documents
    for (auto&& doc : cursor) {
        // Access the timestamp field (referenced by "_id")
        std::string timestamp = doc["_id"].get_string().value.to_string();

        // Access the 'grid_cells' array
        bsoncxx::array::view gridCellsArray = doc["grid_cells"].get_array().value;

        // Clear the current grid data
        currentGridData.clear();

        // Iterate over the grid cells array
        for (auto&& gridCellDoc : gridCellsArray) {
            // Get the grid cell document
            auto gridCellView = gridCellDoc.get_document().value;

            // Extract the cell_index
            auto cellIndexArray = gridCellView["cell_index"].get_array().value;
            int x = cellIndexArray[0].get_int32();
            int y = cellIndexArray[1].get_int32();
            sf::Vector2i cellIndex(x, y);

            // Extract the agent_type_count
            auto agentTypeCountArray = gridCellView["agent_type_count"].get_array().value;
            std::unordered_map<std::string, int> agentCounts;

            for (auto&& agentTypeCountDoc : agentTypeCountArray) {
                auto agentTypeCountView = agentTypeCountDoc.get_document().value;
                std::string type = agentTypeCountView["type"].get_string().value.to_string();
                int count = agentTypeCountView["count"].get_int32();
                agentCounts[type] = count;
            }

            // Store in currentGridData
            currentGridData[cellIndex] = agentCounts;
        }

        // Store currentGridData in frameStorage
        frameStorage.push(currentGridData);
    }

    // Save the number of frames
    numFrames = frameStorage.size();
}


void Visualizer::update() {
    // Get the current grid data
    if (!frameStorage.empty()) {
        currentGridData = frameStorage.front();
        frameStorage.pop();

    } else {
        std::cerr << "No more frames to process." << std::endl;
    }
}

// Main rendering function (using rendering texture instead of window)
void Visualizer::render() {
    // Clear the window
    renderTexture.clear(sf::Color::White);

    // Clear vertex arrays if any
    gridLinesVertexArray.clear();

    // Iterate over all sensors
    for (const auto& sensor : sensors) {
        if (sensor.type != "grid-based") continue;

        // Draw sensor detection area first
        sf::RectangleShape detectionArea(sf::Vector2f(sensor.detectionArea.width, sensor.detectionArea.height));
        detectionArea.setPosition(sensor.detectionArea.left + offset.x, sensor.detectionArea.top + offset.y);
        detectionArea.setFillColor(sf::Color(sensor.color.r, sensor.color.g, sensor.color.b, sensor.alpha));
        detectionArea.setOutlineColor(sf::Color(220, 220, 220));
        detectionArea.setOutlineThickness(1);
        renderTexture.draw(detectionArea);

        // Use sensor.cellSize for calculations
        float cellSize = sensor.cellSize;

        // Calculate grid dimensions
        int gridWidth = static_cast<int>(std::ceil(sensor.detectionArea.width / cellSize));
        int gridHeight = static_cast<int>(std::ceil(sensor.detectionArea.height / cellSize));

        // Draw grid lines within detection area (optional)
        if (showGrids) {
            for (int x = 0; x <= gridWidth; ++x) {
                float posX = sensor.detectionArea.left + x * cellSize + offset.x;
                sf::Vertex line[] = {
                    sf::Vertex(sf::Vector2f(posX, sensor.detectionArea.top + offset.y), sf::Color(220, 220, 220)),
                    sf::Vertex(sf::Vector2f(posX, sensor.detectionArea.top + sensor.detectionArea.height + offset.y), sf::Color(220, 220, 220))
                };
                gridLinesVertexArray.append(line[0]);
                gridLinesVertexArray.append(line[1]);
            }

            for (int y = 0; y <= gridHeight; ++y) {
                float posY = sensor.detectionArea.top + y * cellSize + offset.y;
                sf::Vertex line[] = {
                    sf::Vertex(sf::Vector2f(sensor.detectionArea.left + offset.x, posY), sf::Color(220, 220, 220)),
                    sf::Vertex(sf::Vector2f(sensor.detectionArea.left + sensor.detectionArea.width + offset.x, posY), sf::Color(220, 220, 220))
                };
                gridLinesVertexArray.append(line[0]);
                gridLinesVertexArray.append(line[1]);
            }

            renderTexture.draw(gridLinesVertexArray);
        }

        // Iterate over cells within detection area
        for (int x = 0; x < gridWidth; ++x) {
            for (int y = 0; y < gridHeight; ++y) {
                sf::Vector2i cellIndex(x, y);

                // Get agent counts for this cell
                auto cellIt = currentGridData.find(cellIndex);
                std::unordered_map<std::string, int> agentCounts;
                if (cellIt != currentGridData.end()) {
                    agentCounts = cellIt->second;
                } else {
                    agentCounts = ghostCellAgentCounts; // Use ghost cell
                }

                // Calculate total agents in this cell
                int totalAgents = 0;
                for (const auto& agentType : allAgentTypes) {
                    totalAgents += agentCounts[agentType];
                }

                // Skip empty cells
                if (totalAgents == 0) {
                    continue;
                }

                // Collect agent types with non-zero counts
                std::vector<std::pair<std::string, int>> classCounts;
                for (const auto& [agentType, count] : agentCounts) {
                    if (count > 0) {
                        classCounts.emplace_back(agentType, count);
                    }
                }

                // Sort classes by count in descending order, then by priority
                std::sort(classCounts.begin(), classCounts.end(), [this](const auto& a, const auto& b) {
                    if (a.second != b.second) {
                        // Sort by count in descending order
                        return a.second > b.second;
                    } else {
                        // Counts are equal, sort by priority in descending order
                        int priorityA = agentTypeAttributes[a.first].priority;
                        int priorityB = agentTypeAttributes[b.first].priority;
                        return priorityA > priorityB;
                    }
                });

                // Determine subgrid division
                int numClasses = static_cast<int>(classCounts.size());
                int subgridRows = 2;
                int subgridCols = 2;

                if (numClasses > 4) {
                    subgridRows = 3;
                    subgridCols = 3;
                }

                // Function to get subgrid positions
                auto getSubgridPosition = [&](int index) -> sf::Vector2f {
                    int col = index % subgridCols;
                    int row = index / subgridCols;
                    float subgridWidth = cellSize / subgridCols;
                    float subgridHeight = cellSize / subgridRows;
                    float posX = sensor.detectionArea.left + x * cellSize + col * subgridWidth + offset.x;
                    float posY = sensor.detectionArea.top + y * cellSize + row * subgridHeight + offset.y;
                    return sf::Vector2f(posX, posY);
                };

                // Get the largest class count for scaling
                int maxClassCount = classCounts[0].second;

                // Loop over classes and draw circles
                for (size_t i = 0; i < classCounts.size(); ++i) {
                    if (i >= static_cast<size_t>(subgridRows * subgridCols)) {
                        // Limit to available subgrids
                        break;
                    }

                    const auto& [agentType, count] = classCounts[i];

                    // Get the subgrid position
                    sf::Vector2f subgridPos = getSubgridPosition(static_cast<int>(i));

                    // Calculate subgrid dimensions
                    float subgridWidth = cellSize / subgridCols;
                    float subgridHeight = cellSize / subgridRows;

                    // Calculate circle radius
                    float margin = 5.0f; // Adjust margin as needed
                    float maxCircleRadius = (std::min(subgridWidth, subgridHeight) - 2 * margin) / 2.0f;

                    // Scaling factor relative to the largest class
                    float scalingFactor = static_cast<float>(count) / maxClassCount;

                    // Circle radius
                    float circleRadius = maxCircleRadius * scalingFactor;

                    // Center of the circle within the subgrid
                    sf::Vector2f circleCenter = subgridPos + sf::Vector2f(subgridWidth / 2.0f, subgridHeight / 2.0f);

                    // Draw the circle
                    sf::CircleShape circle(circleRadius);
                    circle.setFillColor(stringToColor(agentTypeAttributes[agentType].color));
                    circle.setPosition(circleCenter - sf::Vector2f(circleRadius, circleRadius));
                    circle.setOutlineThickness(1);
                    circle.setOutlineColor(sf::Color::Black);

                    renderTexture.draw(circle);
                }
            }
        }
    }

    // Draw the simulation boundary
    sf::RectangleShape canvas(sf::Vector2f(simulationSize.x, simulationSize.y));
    canvas.setOutlineThickness(3);
    canvas.setOutlineColor(sf::Color::Black);
    canvas.setFillColor(sf::Color::Transparent);
    canvas.setPosition(offset);
    renderTexture.draw(canvas);

    // Display the window
    renderTexture.display();
    sf::Sprite sprite(renderTexture.getTexture());
    window.clear();
    window.draw(sprite);
    window.display();
}


void Visualizer::handleEvents(sf::Event& event) {

    if(event.type == sf::Event::Closed) {
        window.close();
    }
    else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Q) {
            window.close();
        }
        else if (event.key.code == sf::Keyboard::Space) {
            paused = !paused;
        }
    }
}

// Deprecated
// void Visualizer::captureFrame(int frameNumber) {

//     // Create an SFML image to hold the screenshot
//     sf::Image screenshot = window.capture();

//     // Generate a filename with leading zeros for proper sorting
//     const std::string framesDir = "frames";

//     std::ostringstream filename;
//     filename << framesDir << "/frame_" << std::setw(8) << std::setfill('0') << frameNumber << ".png";

//     // Save the screenshot to a file
//     if (!screenshot.saveToFile(filename.str())) {
//         std::cerr << "Failed to save screenshot " << filename.str() << std::endl;
//     }
// }

void Visualizer::captureFrame(int frameNumber) {
    // Capture the rendered texture to an image
    sf::Image screenshot = renderTexture.getTexture().copyToImage();

    // Generate a filename with leading zeros for proper sorting
    const std::string framesDir = "frames";
    std::ostringstream filename;
    filename << framesDir << "/frame_" << std::setw(8) << std::setfill('0') << frameNumber << ".png";

    // Save the screenshot to a file
    if (!screenshot.saveToFile(filename.str())) {
        std::cerr << "Failed to save screenshot " << filename.str() << std::endl;
    }
}

// Deprecated
// void Visualizer::createVideoFromFrames(int totalFrames) {

//     // Build the FFmpeg command
//     std::string command = "ffmpeg -y -framerate " + std::to_string(sensors[0].frameRate) + " -i frames/frame_%08d.png -c:v libx264 -pix_fmt yuv420p gbs_data_video.mp4";

//     // Call the command
//     int result = std::system(command.c_str());

//     if (result != 0) {
//         std::cerr << "FFmpeg command failed with code " << result << std::endl;
//     } else {
//         std::cout << "Video created successfully." << std::endl;
//     }
// }

void Visualizer::createVideoFromFrames(int totalFrames) {

    // Build the FFmpeg command
    std::string command = "ffmpeg -y -framerate " + std::to_string(frameRate) + " -i frames/frame_%08d.png -c:v libx264 -pix_fmt yuv420p gbs_data_video.mp4";
    // std::string command = "ffmpeg -y -framerate " + std::to_string(frameRate) + 
    //                   " -i frames/frame_%08d.png -c:v libx264 -pix_fmt yuv420p "
    //                   "-preset veryslow -crf 18 -b:v 5M gt_data_video.mp4";

    // Call the command
    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "FFmpeg command failed with code " << result << std::endl;
    } else {
        std::cout << "Video created successfully." << std::endl;
    }
}

void Visualizer::cleanupFrames(int totalFrames) {
    for (int i = 0; i < totalFrames; ++i) {
        std::ostringstream filename;
        std::string framesDir = "frames/";
        filename << framesDir << "frame_" << std::setw(8) << std::setfill('0') << i << ".png";

        if (std::remove(filename.str().c_str()) != 0) {
            std::cerr << "Error deleting file " << filename.str() << std::endl;
        }
    }
}

void Visualizer::run() {

    // Create chrono high quality timer
    std::chrono::high_resolution_clock timer;
    std::chrono::time_point start = timer.now();
    std::chrono::duration<float> timeStep(1.0f / sensors[0].frameRate);
    std::chrono::duration<float> totalFrameTime(0.0f);
    int frameNumber = 0;
    
    while (window.isOpen() && !frameStorage.empty()) {

        // Start timer
        auto startTime = timer.now();

        // Event handling
        while (window.pollEvent(event)) {
            handleEvents(event);
        }

        if(!paused) {

            // Update the window
            update();

            // Render the window
            render();

            // Capture frame
            if(makeVideo) {
                captureFrame(frameNumber);
            }
            auto preFrameTime = timer.now() - startTime;

            // Increment frame number
            frameNumber++;

            // Calculate remaining sleep time to meet the target frame rate
            auto remainingTime = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<float>(0)), std::chrono::duration_cast<std::chrono::milliseconds>(timeStep) - std::chrono::duration_cast<std::chrono::milliseconds>(preFrameTime));
            std::this_thread::sleep_for(remainingTime);
            auto frameTime = timer.now() - startTime;
            // totalFrameTime += std::chrono::duration<float>(frameTime);
        }
    }

    if(makeVideo) {
        // After capturing all frames, call FFmpeg
        createVideoFromFrames(frameNumber);

        // Clean up frame images
        cleanupFrames(frameNumber);
    }

    // Calculate the average frame time
    std::cout << "Average frame time: " << totalFrameTime.count() / numFrames << " seconds" << std::endl;
}