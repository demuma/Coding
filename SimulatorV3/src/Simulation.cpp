#include "../include/Simulation.hpp"
#include "../include/CollisionGrid.hpp"
#include "../include/AgentBasedSensor.hpp"
#include "../include/GridBasedSensor.hpp"
#include "../include/AdaptiveGridBasedSensor.hpp"

// Simulation::Simulation(std::queue<std::vector<Agent>> (&buffers)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer)
Simulation::Simulation(SharedBuffer<std::vector<Agent>>& buffer, std::unordered_map<std::string, std::shared_ptr<SharedBuffer<std::shared_ptr<QuadtreeSnapshot::Node>>>> snapshotBuffers, std::atomic<float>& currentSimulationTimeStep, const YAML::Node& config)
: buffer(buffer), sensorBuffers(sensorBuffers), currentSimulationTimeStep(currentSimulationTimeStep), config(config), grid(0, 0, 0), instance {} {
    
    // Set the initial write buffer (second queue buffer) -> TODO: Make SharedBuffer class and move this logic there
    DEBUG_MSG("Simulation: write buffer: " << buffer.writeBufferIndex);
    
    loadConfiguration();
    loadAgentsAttributes();
    loadObstacles();
    initializeDatabase();
    initializeGrid();
    initializeAgents();
    initializeSensors();
}

void Simulation::loadConfiguration() {

    // Load simulation parameters
    timeStep = config["simulation"]["time_step"].as<float>();

    // Set the maximum number of frames if the duration is specified
    if(config["simulation"]["duration_seconds"]) {
        maxFrames = config["simulation"]["duration_seconds"].as<int>() / timeStep;
        targetSimulationTime = config["simulation"]["duration_seconds"].as<int>();
    } else {
        maxFrames = config["simulation"]["maximum_frames"].as<int>();
        targetSimulationTime = maxFrames * timeStep;
    }

    // Window scaling parameters
    scale = config["display"]["pixels_per_meter"].as<float>();
    simulationWidth = config["simulation"]["width"].as<int>(); // Pixels
    simulationHeight = config["simulation"]["height"].as<int>(); // Pixels

    // Agent parameters
    waypointDistance = config["agents"]["waypoint_distance"].as<float>();
    numAgents = config["agents"]["num_agents"].as<int>();

    // Load number of threads -> TODO: Use this to initialize the thread pool
    if(config["simulation"]["num_threads"].as<int>()) {
        numThreads = config["simulation"]["num_threads"].as<int>();
    } else {
        numThreads = std::thread::hardware_concurrency();
    }

    // If datetime is provided in the configuration, use it, otherwise use the current time
    if(config["simulation"]["datetime"]) {

        // Set time from configuration
        datetime = config["simulation"]["datetime"].as<std::string>();
    } else {

        // Set current time
        datetime = generateISOTimestamp();
    }

    // Collision
    collisionGridCellSize = config["collision"]["grid"]["cell_size"].as<float>();

    // Scenario
    if(config["simulation"]["scenario"]) {
        scenario = config["simulation"]["scenario"].as<std::string>();
    } 
    else {
        scenario = "default";
    }
}

void Simulation::loadAgentsAttributes() {

    // Extract agent data per type
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

        // Set the number of agent types
        numAgentTypes = agentTypeAttributes.size();
    }
}

// Function to initialize the grid based on the YAML configuration
void Simulation::initializeGrid() {

    // Initialize the grid
    grid = Grid(collisionGridCellSize, simulationWidth / collisionGridCellSize, simulationHeight / collisionGridCellSize);
}

// Function to load obstacles from the YAML configuration file
void Simulation::loadObstacles() {

    // Check if the 'obstacles' key exists and is a sequence
    if (config["obstacles"] && config["obstacles"].IsSequence()) {
        for (const auto& obstacleNode : config["obstacles"]) {
            std::string type = obstacleNode["type"] && obstacleNode["type"].IsScalar()
                ? obstacleNode["type"].as<std::string>()
                : "unknown";

            if (type == "rectangle") { // Only handle rectangles
                std::vector<float> position = obstacleNode["position"].as<std::vector<float>>();
                std::vector<float> size = obstacleNode["size"].as<std::vector<float>>();
                obstacles.push_back(Obstacle(
                    sf::FloatRect({position[0], position[1]}, {size[0], size[1]}), 
                    stringToColor(obstacleNode["color"].as<std::string>())
                ));
            } else {
                std::cerr << "Error: Unknown obstacle type '" << type << "' in config file." << std::endl;
            }
        }
    } else {
        std::cerr << "Error: Could not find 'obstacles' key in config file or it is not a sequence." << std::endl;
    }
}

void Simulation::initializeAgents() {

    // Initialize agents based on the scenario
    if (scenario == "random") {

        for (int i = 0; i < numAgents; ++i) {

            std::random_device rd;
            std::mt19937 gen(rd());

            std::uniform_real_distribution<> disPosX(0, simulationWidth);
            std::uniform_real_distribution<> disPosY(0, simulationHeight);

            Agent agent(agentTypeAttributes["Adult Cyclist"]);

            agent.agentId = generateUUID();
            agent.sensorId = "0";
            agent.type = "Adult Cyclist";
            agent.color = stringToColor(agent.attributes.color);
            agent.priority = agent.attributes.priority;
            agent.bodyRadius = agent.attributes.bodyRadius;
            agent.lookAheadTime = agent.attributes.lookAheadTime;

            agent.setBufferZoneSize();
            agent.initialPosition = sf::Vector2f(disPosX(gen), disPosY(gen));
            agent.targetPosition = sf::Vector2f(disPosX(gen), disPosY(gen));
            agent.position = agent.initialPosition;
            agent.waypointDistance = waypointDistance; // -> TODO: Use taxonomy for waypoint distance
            agent.waypointColor = sf::Color::Red;
            agent.calculateTrajectory(agent.waypointDistance);
            agent.timestamp = generateISOTimestamp(simulationTime, datetime);

            agent.velocityMagnitude = generateRandomNumberFromTND(
                agent.attributes.velocity.mu, agent.attributes.velocity.sigma, 
                agent.attributes.velocity.min, agent.attributes.velocity.max
            );
            agent.calculateVelocity(agent.trajectory[1]);
            agent.initialVelocity = agent.velocity;

            agents.push_back(agent);
        }
    }
    else if(scenario == "crossing") {

    }
    else if(scenario == "continuous") {

    }
    // Default scenario
    else {
        // Check if sum of probabilities is 1
        double sum = 0;
        for (const auto& agentType : agentTypeAttributes) {
            sum += agentType.second.probability;
        }
        if (std::abs(sum - 1) > tolerance) {
            ERROR_MSG("Error: Sum of agent probabilities is not equal to 1, but " << (float)sum);
            exit(EXIT_FAILURE);
        }

        // Generate agents based on the probabilities from agent taxonomy
        for (const auto& agentType : agentTypeAttributes) {

            // Calculate the number of agents of each type
            int numAgentsPerType = numAgents * agentType.second.probability;
            int currentNumAgents = 0;
            
            for (int i = 0; i < numAgentsPerType; ++i) {
                
                // Generate random positions
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> disPosX(0, simulationWidth);
                std::uniform_real_distribution<> disPosY(0, simulationHeight);
                
                Agent agent(agentType.second); // Initialize agent with attributes: map(type as string, AgentTypeAttributes)
                
                agent.agentId = generateUUID();
                agent.sensorId = "0";
                agent.type = agentType.first;
                agent.color = stringToColor(agentType.second.color);
                agent.priority = agentType.second.priority;
                agent.bodyRadius = agentType.second.bodyRadius;
                agent.lookAheadTime = agentType.second.lookAheadTime;

                agent.setBufferZoneSize();
                agent.initialPosition = sf::Vector2f(disPosX(gen), disPosY(gen));
                agent.targetPosition = sf::Vector2f(disPosX(gen), disPosY(gen));
                agent.position = agent.initialPosition;
                agent.waypointDistance = waypointDistance; // -> TODO: Use taxonomy for waypoint distance
                agent.waypointColor = sf::Color::Red;
                agent.calculateTrajectory(agent.waypointDistance);
                agent.timestamp = generateISOTimestamp(simulationTime, datetime);

                agent.velocityMagnitude = generateRandomNumberFromTND(
                    agentType.second.velocity.mu, agentType.second.velocity.sigma, 
                    agentType.second.velocity.min, agentType.second.velocity.max
                );

                agent.calculateVelocity(agent.trajectory[1]);
                agent.initialVelocity = agent.velocity;
                agents.push_back(agent);

                // Increment the number of agents
                currentNumAgents++;
            }

            DEBUG_MSG("Number of agents per type " << agentType.first << ": " << currentNumAgents << " in " << agentType.second.color);
        }
        DEBUG_MSG("Total number of agents: " << agents.size());
    }    
}

// Initialize the database connection
void Simulation::initializeDatabase() {

    // Load the database configuration
    std::string dbHost = config["database"]["host"].as<std::string>();
    int dbPort = config["database"]["port"].as<int>();
    databaseName = config["database"]["db_name"].as<std::string>();
    collectionName = config["database"]["collection_name"].as<std::string>();
    dbUri = "mongodb://" + dbHost + ":" + std::to_string(dbPort);
    clearDatabase = config["database"]["clear_database"].as<bool>();

    // MongoDB URI
    mongocxx::uri uri(dbUri);

    // Initialize the MongoDB client
    client = std::make_shared<mongocxx::client>(uri);
    mongocxx::database db = (*client)[databaseName];
    collection = db[collectionName];

    // Clear the database if specified
    if(clearDatabase) {
        collection.delete_many({});
    }

    // Post metadata to the database
    postMetadata();
}

// Initialize sensors with YAML configuration
void Simulation::initializeSensors() {

    // Load sensors from the configuration file
    const YAML::Node& sensorNodes = config["sensors"];

    // Iterate over each sensor node in the configuration
    for (const auto& sensorNode : sensorNodes) {

        // Get the sensor type and frame rate
        std::string type = sensorNode["type"].as<std::string>();
        float frameRate = sensorNode["frame_rate"].as<float>();
        sf::Color color = stringToColor(sensorNode["detection_area"]["color"].as<std::string>());
        int alpha = sensorNode["detection_area"]["alpha"].as<float>() * 255;
        sf::Color colorAlpha = sf::Color(color.r, color.g, color.b, alpha);
        std::string databaseName = sensorNode["database"]["db_name"].as<std::string>();
        std::string collectionName = sensorNode["database"]["collection_name"].as<std::string>();

        // Define the detection area for the sensor
        sf::FloatRect detectionArea(

            {
                sensorNode["detection_area"]["x"].as<float>(),
                sensorNode["detection_area"]["y"].as<float>()
            },
            {
                sensorNode["detection_area"]["width"].as<float>(),
                sensorNode["detection_area"]["height"].as<float>()
            }
        );

        // Create the agent-based sensor
        if (type == "agent-based") {
            
            // Create the agent-based sensor and add to sensors vector
            sensors.push_back(std::make_unique<AgentBasedSensor>(frameRate, detectionArea, databaseName, collectionName, client));
            sensors.back()->scale = scale;
            sensors.back()->timestamp = generateISOTimestamp(simulationTime, datetime);

            // Clear the database if specified
            if(clearDatabase) {
                sensors.back()->clearDatabase();
            }

            // Post metadata to the database
            sensors.back()->postMetadata();

            // Set initial positions for agents in the detection area
            for(const auto& agent : agents) {

                // Check if the agent is within the detection area
                if(sensors.back()->detectionArea.contains(agent.position)) {

                    // Get raw pointer from unique_ptr
                    Sensor* sensor = sensors.back().get();

                    // Try to cast to AgentBasedSensor*
                    auto agentBasedSensor = dynamic_cast<AgentBasedSensor*>(sensor); 

                    // Check if cast succeeded
                    if (agentBasedSensor != nullptr) { 
                        agentBasedSensor->previousPositions[agent.agentId] = agent.position;
                    }
                }
            }
        }

        // Create the grid-based sensor
        else if (type == "grid-based") {

            float cellSize = sensorNode["grid"]["cell_size"].as<float>();
            bool showGrid = sensorNode["grid"]["show_grid"].as<bool>();

            // Create the grid-based sensor and add to sensors vector
            sensors.push_back(std::make_unique<GridBasedSensor>(frameRate, detectionArea, cellSize, databaseName, collectionName, client));
            sensors.back()->scale = scale;
            sensors.back()->timestamp = generateISOTimestamp(simulationTime, datetime);

            // Clear the database if specified
            if(clearDatabase) {
                sensors.back()->clearDatabase();
            }

            // Post metadata to the database
            sensors.back()->postMetadata();
        }

        // Create the grid-based sensor
        else if (type == "adaptive-grid-based") {

            float cellSize = sensorNode["grid"]["cell_size"].as<float>();
            bool showGrid = sensorNode["grid"]["show_grid"].as<bool>();
            int maxDepth = sensorNode["grid"]["max_depth"].as<int>();

            // Create the grid-based sensor and add to sensors vector
            sensors.push_back(std::make_unique<AdaptiveGridBasedSensor>(frameRate, detectionArea, cellSize, maxDepth, databaseName, collectionName, client));
            sensors.back()->scale = scale;
            sensors.back()->timestamp = generateISOTimestamp(simulationTime, datetime);

            // Clear the database if specified
            if(clearDatabase) {
                sensors.back()->clearDatabase();
            }

            // Post metadata to the database
            sensors.back()->postMetadata();
        }
    }
}

// Run the simulation
void Simulation::run() {

    // Initialize the clock
    sf::Clock simulationClock;
    simulationClock.restart();

    // Initialize the simulation timers
    sf::Time writeBufferTime = sf::Time::Zero;
    sf::Time totalWriteBufferTime = sf::Time::Zero;
    sf::Time simulationUpdateTime = sf::Time::Zero;
    sf::Time simulationStepTime = sf::Time::Zero;

    // Set the initial time step
    currentSimulationTimeStep = timeStep;
    simulationRealTime += simulationClock.getElapsedTime();

    // Main simulation loop
    while ((buffer.currentWriteFrameIndex < maxFrames && !buffer.stop.load())) {

        // Restart the clock
        simulationClock.restart();

        // Write the current frame to the buffer
        buffer.write(agents);

        // Swap the read and write buffers if the read buffer is empty
        buffer.swap();

        // Update the buffer write time
        writeBufferTime = simulationClock.getElapsedTime();
        totalWriteBufferTime += writeBufferTime;

        // Increment the time step
        simulationTime += sf::seconds(timeStep);

        // Update the agents
        update();

        // Update the simulation update time
        simulationUpdateTime += simulationClock.getElapsedTime() - writeBufferTime;

        // Calculate the time step for the simulation
        simulationStepTime = simulationClock.getElapsedTime();
        currentSimulationTimeStep = simulationStepTime.asSeconds();

        // Calculate the simulation time
        simulationRealTime += simulationStepTime;
    }

    // Signalize the simulation has finished so that the renderer can swap buffers if needed
    buffer.stop.store(true);

    // End the simulation
    buffer.end();
    DEBUG_MSG("Simulation: finished");

    // Print simulation statistics
    STATS_MSG("Total simulation wall time: " << simulationRealTime.asSeconds() << " seconds for " << buffer.currentWriteFrameIndex << " frames");
    STATS_MSG("Total simulation time: " << simulationTime.asSeconds() << " seconds" << " for " << numAgents << " agents");
    STATS_MSG("Simulation speedup: " << (maxFrames * timeStep) / simulationRealTime.asSeconds());
    STATS_MSG("Frame rate: " << 1/(simulationRealTime.asSeconds() / buffer.currentWriteFrameIndex));
    STATS_MSG("Average simulation update time: " << simulationUpdateTime.asSeconds() / buffer.currentWriteFrameIndex);
    STATS_MSG("Average simulation time step: " << simulationRealTime.asSeconds() / buffer.currentWriteFrameIndex);
    STATS_MSG("Average write buffer time: " << totalWriteBufferTime.asSeconds() / buffer.currentWriteFrameIndex);
}

void Simulation::update() {

    // Store ground truth data in MongoDB
    postData(agents);

    // Clear the grid
    grid.clear();

    // Get the current timestamp once per frame
    std::string currentTimestamp = generateISOTimestamp(simulationTime, datetime);

    // Loop through all agents and update their positions
    for(auto agent = agents.begin(); agent != agents.end();) {

         // Check if agent is out of bounds
        if (agent->position.x > simulationWidth + agent->bodyRadius || agent->position.x < -agent->bodyRadius ||
            agent->position.y > simulationHeight + agent->bodyRadius || agent->position.y < -agent->bodyRadius) {
            
            // Remove the agent from the vector and update the iterator
            agent = agents.erase(agent); // Increases agent iterator if erased
        } 
        else {

            // Assign the agent to the correct grid cell
            grid.addAgent(&(*agent));

            // Reset collision state at the start of each frame for each agent
            agent->resetCollisionState();

            // Update the agent timestamp
            agent->timestamp = currentTimestamp;

            // Update the agent position
            agent->updatePosition(timeStep);

            // Only update velocity if the agent is not stopped
            if(!agent->stopped) {
                agent->updateVelocity(timeStep, simulationRealTime);
            }
            else {
                if(!agent->collisionPredicted) {
                    agent->resume(agents);
                // agent->updateVelocity(timeStep, simulationRealTime);
                }
            }

            ++agent;
        }
    }


    // Update sensors and store sensor data
    for (auto& sensor : sensors) {

        sensor->update(agents, timeStep, simulationTime, datetime);
        // sensor->printData();
        sensor->postData();
    }

    // Collision detection using grid
    grid.checkCollisions();
}

void Simulation::postMetadata() {

    // Prepare a BSON document for the metadata
    bsoncxx::builder::stream::document document{};

    bsoncxx::builder::stream::document simulationArea{};
    simulationArea << "width" << simulationWidth
                   << "height" << simulationHeight;

    // Append the metadata fields to the document
    document << "timestamp" << generateISOTimestamp(simulationTime, datetime)
             << "data_type" << "metadata"
             << "simulation_area" << simulationArea
             << "frame_rate" << 1/timeStep
             << "cell_size" << collisionGridCellSize;

    // Insert the metadata document into the collection
    try {
        collection.insert_one(document << bsoncxx::builder::stream::finalize);
    } catch (const mongocxx::exception& e) {
        // Handle errors
        std::cerr << "Error inserting metadata: " << e.what() << std::endl;
    }
}

// Store agent data in MongoDB
void Simulation::postData(const std::vector<Agent>& agents) {

    // Check if there are agents to store
    if (!agents.empty()) {
    
        // Prepare a vector of documents to insert in bulk
        std::vector<bsoncxx::document::value> documents;
        documents.reserve(agents.size());

        // Iterate over each agent and prepare a document for MongoDB
        for (const auto& agent : agents) {

            // Construct a BSON document for each agent
            bsoncxx::builder::stream::document document{};

            // Append the agent data to the document
            document << "timestamp" << agent.timestamp
                     << "data_type" << "agent_data"
                     << "agent_id" << agent.agentId
                     << "type" << agent.type
                     << "position" 
                        << bsoncxx::builder::stream::open_array
                        << agent.position.x 
                        << agent.position.y
                         << bsoncxx::builder::stream::close_array
                     << "velocity" 
                        << bsoncxx::builder::stream::open_array
                        << agent.velocity.x 
                        << agent.velocity.y
                         << bsoncxx::builder::stream::close_array;

            documents.push_back(document << bsoncxx::builder::stream::finalize);
        }

        // Insert the documents into MongoDB
        try {
            // Insert documents into MongoDB in bulk
            collection.insert_many(documents);
        } catch (const mongocxx::exception& e) {
            std::cerr << "An error occurred while inserting documents: " << e.what() << std::endl;
        }
    }
}