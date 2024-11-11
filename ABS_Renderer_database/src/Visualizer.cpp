#include "../include/Visualizer.hpp"
#include "../include/Logging.hpp"
#include "../include/Utilities.hpp"

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

Visualizer::Visualizer() 
:   gridLinesVertexArray(sf::PrimitiveType::Lines), 
    bufferZonesVertexArray(sf::PrimitiveType::Points), 
    agentBodiesVertexArray(sf::PrimitiveType::Quads), 
    agentArrowBodyVertexArray(sf::PrimitiveType::Lines),
    agentArrowHeadVertexArray(sf::PrimitiveType::Triangles)
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

    // Load collision grid cell size
    gridCellSize *= scale;
    
    // Load database parameters
    std::string dbHost = config["database"]["host"].as<std::string>();
    int dbPort = config["database"]["port"].as<int>();
    databaseName = config["database"]["db_name"].as<std::string>();
    dbUri = "mongodb://" + dbHost + ":" + std::to_string(dbPort);

    // Visualization parameters
    showGrids = config["renderer"]["show_grids"].as<bool>();
    showBufferZones = config["renderer"]["show_buffer"].as<bool>();
    showArrow = config["renderer"]["show_arrow"].as<bool>();

    // Video parameters
    makeVideo = config["simulation"]["make_video"].as<bool>();
}

void Visualizer::initializeDatabase() {

    mongocxx::uri uri(dbUri);
    client = std::make_shared<mongocxx::client>(uri);
    db = (*client)[databaseName];
    collection = db["AB_Sensor_Data"];
}

void Visualizer::testWriteDataBase() {
    // mongocxx::client client{mongocxx::uri{}};
    mongocxx::database db = client->database(databaseName);
    collectionName = "test";
    mongocxx::collection coll = db[collectionName];
    bsoncxx::builder::stream::document document{};
    document << "name" << "MongoDB";
    document << "type" << "database";
    document << "count" << 1;
    bsoncxx::stdx::optional<mongocxx::result::insert_one> result = coll.insert_one(document.view());
}

void Visualizer::testReadDataBase() {
    
    mongocxx::collection coll = db[collectionName];
    bsoncxx::stdx::optional<bsoncxx::document::value> maybe_result = coll.find_one({});
    // if(maybe_result) {
    //     std::cout << bsoncxx::to_json(*maybe_result) << std::endl;
    // }
    Agent agent = createAgentFromDocument(maybe_result->view());
    std::cout << "Agent ID: " << agent.uuid << std::endl;
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

void Visualizer::initializeSensors() {



    
}

void Visualizer::getAgentHeading() {
    for (auto& agent : currentFrame) {
        // Check if velocity is (0,0)
        if (agent.velocity.x == 0 && agent.velocity.y == 0) {
            // Set a default orientation (e.g., pointing right)
            agent.heading = sf::Vector2f(1.0f, 0.0f);
        } 
        else {
            // Calculate the angle from the velocity vector
            float angle = std::atan2(agent.velocity.y, agent.velocity.x);
            agent.heading = sf::Vector2f(std::cos(angle), std::sin(angle)); // Unit vector in the direction of the angle
        }
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

void Visualizer::loadAgentsAttributes() {

    // Extract agent attributes
    if (config["agents"] && config["agents"]["road_user_taxonomy"]) {
        for (const auto& agent : config["agents"]["road_user_taxonomy"]) {
            std::string type = agent["type"].as<std::string>();
            Agent::AgentTypeAttributes attributes;

            attributes.probability = agent["probability"].as<double>();
            attributes.priority = agent["priority"].as<int>();
            attributes.bodyRadius = agent["radius"].as<double>();
            attributes.color = agent["color"].as<std::string>();

            attributes.velocity.min = agent["velocity"]["min"].as<double>();
            attributes.velocity.max = agent["velocity"]["max"].as<double>();
            attributes.velocity.mu = agent["velocity"]["mu"].as<double>();
            attributes.velocity.sigma = agent["velocity"]["sigma"].as<double>();
            attributes.velocity.noiseScale = agent["velocity"]["noise_scale"].as<double>();
            attributes.velocity.noiseFactor = agent["velocity"]["noise_factor"].as<double>();

            attributes.acceleration.min = agent["acceleration"]["min"].as<double>();
            attributes.acceleration.max = agent["acceleration"]["max"].as<double>();

            attributes.lookAheadTime = agent["look_ahead_time"].as<double>();

            // Store in map
            agentTypeAttributes[type] = attributes;
        }
    }
}

// Parse BSON document and create Agent object (agent-based sensor)
Agent Visualizer::createAgentFromDocument(bsoncxx::document::view document) {

    Agent agent(agentTypeAttributes[document["type"].get_string().value.to_string()]);

    agent.uuid = document["agent_id"].get_string().value.to_string();
    agent.sensorID = document["sensor_id"].get_string().value.to_string();
    agent.type = document["type"].get_string().value.to_string();
    agent.position.x = document["position"].get_array().value[0].get_double();
    agent.position.y = document["position"].get_array().value[1].get_double();
    agent.timestamp = document["timestamp"].get_string().value.to_string();
    agent.velocity.x = document["estimated_velocity"].get_array().value[0].get_double();
    agent.velocity.y = document["estimated_velocity"].get_array().value[1].get_double();
    agent.velocityMagnitude = std::sqrt(std::pow(agent.velocity.x, 2) + std::pow(agent.velocity.y, 2));
    agent.bodyRadius = agentTypeAttributes[agent.type].bodyRadius;
    agent.color = stringToColor(agentTypeAttributes[agent.type].color);
    agent.bufferZoneColor = sf::Color::Red;

    return agent;
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

}

// Get agent data from database
void Visualizer::getData() {

    mongocxx::pipeline pipeline;

    // Create empty find query
    mongocxx::cursor cursor = collection.find({}, {});

    // Filter out metadata entries
    pipeline.match(make_document(kvp("data_type", "agent_data")));

    // Group by timestamp and get distinct values
    // pipeline.group(make_document(kvp("_id", "$timestamp")));

    pipeline.group(make_document(kvp("_id", "$timestamp"), kvp("agents", make_document(kvp("$push", "$$ROOT"))))); // Group by timestamp and collect agents in an array
    
    // Sort the results in ascending order of timestamp
    pipeline.sort(make_document(kvp("_id", 1)));
    
    // Execute the aggregation and catch any exceptions
    try {
        cursor = collection.aggregate(pipeline);
    } catch (const mongocxx::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return;
    }

    // Iterate over the documents
    for(auto&& doc : cursor) {
        // Access the timestamps field (referrenced by "_id")
        std::string timestamp = doc["_id"].get_string().value.to_string();

        // Access the 'agents' array
        bsoncxx::array::view agents_array = doc["agents"].get_array().value;

        // Clear the current frame
        currentFrame.clear();

        // Iterate over the agents array
        for(auto&& agentDoc : agents_array) {

            // Create an agent from the document
            auto agentView = agentDoc.get_document().value;
            Agent agent = createAgentFromDocument(agentView);

            // Get previous heading when agent standing still
            if(agent.velocity == sf::Vector2f(0,0)) {
                if(previousHeadings.find(agent.uuid) != previousHeadings.end()) {
                    agent.heading = previousHeadings[agent.uuid];
                }
                DEBUG_MSG("Agent " << agent.uuid << " stands still with previous heading " << agent.heading.x << "," << agent.heading.y);
            }
            else {
                // Calculate heading
                agent.heading = agent.velocity / std::sqrt(agent.velocity.x * agent.velocity.x + agent.velocity.y * agent.velocity.y);
                previousHeadings[agent.uuid] = agent.heading;
                DEBUG_MSG("Agent " << agent.uuid << " moving with previous heading " << agent.heading.x << "," << agent.heading.y);
            }

            // Add the agent to the current frame
            currentFrame.push_back(agent);
        }
        // Save frame to 
        frameStorage.push(currentFrame);
    }

    // Save the number of frames
    numFrames = frameStorage.size();
}

void Visualizer::update() {

    // Get the current frame
    currentFrame = frameStorage.front();
    frameStorage.pop();

    // Update agent buffer radius
    for (auto& agent : currentFrame) {
        float bufferAdaption = (std::sqrt(agent.velocity.x * agent.velocity.x + agent.velocity.y * agent.velocity.y) / agentTypeAttributes.at(agent.type).velocity.max);
        if(bufferAdaption > agent.minBufferZoneRadius) {

            agent.bufferZoneRadius = agent.bodyRadius + (std::sqrt(agent.velocity.x * agent.velocity.x + agent.velocity.y * agent.velocity.y) / agentTypeAttributes.at(agent.type).velocity.max);
        }
        else {
            agent.bufferZoneRadius = agent.minBufferZoneRadius + agent.bodyRadius;
        }
    }
}

// Function to append buffer zones to the vertex array
void Visualizer::appendBufferZones(sf::VertexArray& vertices, const Agent& agent) {

    // Calculate the number of segments depending on the buffer zone radius
    int numSegments = std::max(100, static_cast<int>(agent.bufferZoneRadius * 6.0f));
    
    // Loop through each segment and calculate the position of the vertices
    for (int i = 0; i < numSegments; ++i) {
        float angle = 2 * M_PI * i / numSegments;
        sf::Vector2f radius(agent.bufferZoneRadius * std::cos(angle), agent.bufferZoneRadius * std::sin(angle));
        sf::Vector2f radius2((agent.bufferZoneRadius - 1) * std::cos(angle), agent.bufferZoneRadius * std::sin(angle));
        vertices.append(sf::Vertex(agent.position + offset + radius, agent.bufferZoneColor));
        vertices.append(sf::Vertex(agent.position + offset + radius2, agent.bufferZoneColor));
    }
}

// Function to append agent bodies to the vertex array
void Visualizer::appendAgentBodies(sf::VertexArray& quads, const Agent& agent) {

    // Extract the type of the agent
    std::stringstream ss(agent.type);
    std::string word1, word2;
    ss >> word1 >> word2;

    // Quad vertices
    sf::Vector2f topLeft;
    sf::Vector2f topRight;
    sf::Vector2f bottomRight;
    sf::Vector2f bottomLeft;

    sf::Vector2f position = agent.position + offset;

    // float theta = std::atan2(agent.velocity.y, agent.velocity.x) * 180.0f / M_PI; // Convert to degrees
    float theta = std::atan2(agent.heading.y, agent.heading.x) * 180.0f / M_PI; // Convert to degrees
    float minRadius = std::ceil(sin(M_PI/4) * agent.bodyRadius);
    float maxRadius = std::ceil(agent.bodyRadius);
    float divX, divY;

    // Determine the division factors for the agent body based on its type
    if(word2 == "Cyclist") {
        divX = 1;
        divY = 2;
    }
    else if(word2 == "E-Scooter") {
        divX = 1;
        divY = 3;
    }
    else if(word2 == "Pedestrian") {
        divX = 1;
        divY = 1;
    }
    else {
        divX = 1;
        divY = 1;
    }

    // Calculate the positions of the quad vertices
    topLeft = sf::Vector2f(position.x - minRadius, position.y - minRadius/divY);
    topRight = sf::Vector2f(position.x + minRadius, position.y - minRadius/divY);
    bottomRight = sf::Vector2f(position.x + minRadius, position.y + minRadius/divY);
    bottomLeft = sf::Vector2f(position.x - minRadius, position.y + minRadius/divY);

    // Create the quad and transform it based on the agent's heading
    sf::VertexArray body(sf::Quads, 4);
    body[0] = sf::Vertex(topLeft, agent.color);
    body[1] = sf::Vertex(topRight, agent.color);
    body[2] = sf::Vertex(bottomRight, agent.color);
    body[3] = sf::Vertex(bottomLeft, agent.color);

    sf::Transform transform;
    transform.rotate(theta, position);

    // Apply the transformation to the quad vertices and append them to the vertex array
    for(int i = 0; i < 4; i++) {
        body[i].position = transform.transformPoint(body[i].position);
        quads.append(body[i]);
    }
}

// Main rendering function (using rendering texture instead of window)
void Visualizer::render() {

    // Clear the window
    renderTexture.clear(sf::Color::White);

    // Clear vertex arrays
    gridLinesVertexArray.clear();
    bufferZonesVertexArray.clear();
    agentArrowBodyVertexArray.clear();
    agentBodiesVertexArray.clear();
    agentArrowHeadVertexArray.clear();

    // Draw the collision grid
    for(int x = 0; x <= simulationSize.x / gridCellSize; x++) {
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(x * gridCellSize, 0) + offset, sf::Color(220, 220, 220)),
            sf::Vertex(sf::Vector2f(x * gridCellSize, simulationSize.y) + offset, sf::Color(220, 220, 220))
        };
        gridLinesVertexArray.append(line[0]);
        gridLinesVertexArray.append(line[1]);
    }

    for(int y = 0; y < (simulationSize.y / gridCellSize); y++) {
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(0, y * gridCellSize) + offset, sf::Color(220, 220, 220)),
            sf::Vertex(sf::Vector2f(simulationSize.x, y * gridCellSize) + offset, sf::Color(220, 220, 220))
        };
        gridLinesVertexArray.append(line[0]);
        gridLinesVertexArray.append(line[1]);
    }

    // Draw sensor detection areas
    for(const auto& sensor : sensors) {
        sf::RectangleShape detectionArea(sf::Vector2f(sensor.detectionArea.width, sensor.detectionArea.height));
        detectionArea.setPosition(sf::Vector2f(sensor.detectionArea.left, sensor.detectionArea.top) + offset);
        detectionArea.setFillColor(sf::Color(sensor.color.r, sensor.color.g, sensor.color.b, sensor.alpha));
        detectionArea.setOutlineColor(sf::Color(220, 220, 220));
        detectionArea.setOutlineThickness(1);
        renderTexture.draw(detectionArea);
    }

    // Draw agents
    for(auto& agent : currentFrame) {

        // Scale agent data
        agent.position *= scale;
        agent.bodyRadius *= scale;
        agent.bufferZoneRadius *= scale;
        agent.velocity *= scale;
        agent.velocityMagnitude *= scale;

        // Draw the agent bodies
        appendAgentBodies(agentBodiesVertexArray, agent);

        // Draw the buffer zones
        if(showBufferZones){ 

            appendBufferZones(bufferZonesVertexArray, agent);
        }
        // Draw the arrow (a triangle) representing the velocity vector
        if(showArrow) {

            sf::Vector2f direction = agent.velocity;  // Direction of the arrow from velocity vector
            float theta = std::atan2(agent.heading.y, agent.heading.x) * 180.0f / M_PI; // Convert to degrees
            float arrowLength = 5.0;

            // Normalize the direction vector for consistent arrow length
            sf::Vector2f normalizedDirection = direction / std::sqrt(direction.x * direction.x + direction.y * direction.y);

            // Arrow shape factor
            float arrowHeadLength = 0.4 * scale;
            float arrowHeadWidth = 0.25 * scale;
            int lineThickness = 4;

            // Arrowhead (triangle) - Offset the tip by the agent's radius
            sf::ConvexShape arrow(3);
            arrow.setPoint(0, sf::Vector2f(0, 0)); // Tip of the arrow
            arrow.setPoint(1, sf::Vector2f(-arrowHeadLength, arrowHeadWidth / 2));
            arrow.setPoint(2, sf::Vector2f(-arrowHeadLength , -arrowHeadWidth / 2));
            arrow.setFillColor(sf::Color::Black);

            sf::Vertex line[] =
            {
                sf::Vertex(agent.position + offset, sf::Color::Black),  // Offset start point
                sf::Vertex(line[0].position + normalizedDirection * (agent.bodyRadius + arrowHeadLength + agent.velocityMagnitude / arrowLength), sf::Color::Black) // Ending point (base of arrowhead)
            };

            // Append the arrow lines to the vertex array
            agentArrowBodyVertexArray.append(line[0]);
            agentArrowBodyVertexArray.append(line[1]);

            // Create a triangle shape for the arrowhead
            sf::Vertex triangle[] =
            {
                sf::Vertex(line[1].position, sf::Color::Black),  // Tip of the arrowhead
                sf::Vertex(triangle[0].position + sf::Vector2f(-arrowHeadLength, arrowHeadWidth / 2), sf::Color::Black),  // Left point of the arrowhead
                sf::Vertex(triangle[0].position + sf::Vector2f(-arrowHeadLength, -arrowHeadWidth / 2), sf::Color::Black)  // Right point of the arrowhead
            };

            // Rotate the triangle shape based on the agent's heading
            sf::Transform transform;
            transform.rotate(theta, triangle[0].position);

            // Apply the transformation to the triangle vertices and append them to the vertex array
            for(int i = 0; i < 3; i++) {
                triangle[i].position = transform.transformPoint(triangle[i].position);
                agentArrowHeadVertexArray.append(triangle[i]);
            }
        }
    }

    renderTexture.draw(gridLinesVertexArray);
    renderTexture.draw(bufferZonesVertexArray);
    renderTexture.draw(agentArrowBodyVertexArray);
    renderTexture.draw(agentBodiesVertexArray);
    renderTexture.draw(agentArrowHeadVertexArray);

    // Draw canvas
    sf::RectangleShape canvas(sf::Vector2f(simulationSize.x, simulationSize.y));
    canvas.setOutlineThickness(3);
    canvas.setOutlineColor(sf::Color(sf::Color::Black));
    canvas.setFillColor(sf::Color::Transparent);
    canvas.setPosition(offset);
    renderTexture.draw(canvas);

    // Draw the window
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
//     std::string command = "ffmpeg -y -framerate " + std::to_string(sensors[0].frameRate) + " -i frames/frame_%08d.png -c:v libx264 -pix_fmt yuv420p abs_data_video.mp4";

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
    std::string command = "ffmpeg -y -framerate " + std::to_string(frameRate) + " -i frames/frame_%08d.png -c:v libx264 -pix_fmt yuv420p abs_data_video.mp4";
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

            // Render the window
            update();
            auto updateTime = timer.now();
            render();
            auto renderTime = timer.now() - updateTime;


            // Capture frame if enabled
            if(makeVideo) {
                // Capture frame
                captureFrame(frameNumber);
            }

            // Calculate frame time
            auto preFrameTime = timer.now() - startTime;

            // Increment frame number
            frameNumber++;

            // Calculate remaining sleep time to meet the target frame rate
            auto remainingTime = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<float>(0)), std::chrono::duration_cast<std::chrono::milliseconds>(timeStep) - std::chrono::duration_cast<std::chrono::milliseconds>(renderTime));
            std::this_thread::sleep_for(remainingTime);
            auto frameTime = timer.now();
            totalFrameTime += std::chrono::duration<float>(frameTime - start);
        }
    }

    // Create video from frames if enabled
    if(makeVideo) {

        // Create video from frames
        createVideoFromFrames(frameNumber);

        // Clean up frame images
        cleanupFrames(frameNumber);
    }

    // Calculate the average frame time
    std::cout << "Average frame time: " << totalFrameTime.count() / numFrames << " seconds" << std::endl;
}