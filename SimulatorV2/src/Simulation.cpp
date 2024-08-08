#include "../include/Simulation.hpp"

// Simulation::Simulation(std::queue<std::vector<Agent>> (&buffers)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer)
Simulation::Simulation(SharedBuffer& buffer, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config)
: buffer(buffer), currentSimulationTimeStep(currentSimulationTimeStep), stop(stop), currentNumAgents(currentNumAgents), config(config) {
    
    // Set the initial write buffer (second queue buffer) -> TODO: Make SharedBuffer class and move this logic there
    DEBUG_MSG("Simulation: write buffer: " << buffer.writeBufferIndex);
    
    loadConfiguration();

    // ThreadPool threadPool(numThreads); instead of threadPool(std::thread::hardware_concurrency())
    // TODO: set number of threads for thread pool (no lazy initialization) -> initializeThreadPool();
    initializeAgents();
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
                    sf::FloatRect(position[0], position[1], size[0], size[1]), 
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

    // std::vector<std::future<void>> futures;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disPos(0, simulationHeight); // Position
    std::uniform_real_distribution<> disVel(2, 5); // Velocity
    
    // Initialize agent vector with random velocity, start and target positions
    for (int i = 0; i < numAgents; ++i) {
        Agent agent;
        agent.velocityMagnitude = static_cast<float>(disVel(gen));
        agent.initialPosition = sf::Vector2f(0.f, static_cast<float>(disPos(gen)));
        agent.targetPosition = sf::Vector2f(simulationWidth, static_cast<float>(disPos(gen)));
        agent.position = agent.initialPosition;
        agent.bodyRadius = 0.5f;
        agent.color = sf::Color::Red;
        agent.waypointDistance = waypointDistance; // -> TODO: Use taxonomy for waypoint distance
        agent.waypointColor = sf::Color::Red;
        agent.calculateTrajectory(agent.waypointDistance);
        agent.calculateVelocity(agent.trajectory[1]);
        agents.push_back(agent);
    }

    // // Calculate the trajectory for each agent in parallel
    // for (auto& agent: agents) {
    //     futures.emplace_back(threadPool.enqueue([this, &agent] {
    //         agent.calculateTrajectory(agent.waypointDistance);
    //         agent.calculateVelocity(agent.trajectory[1]);
    //     }));
    // }
    // for (auto& future : futures) {
    //     future.get();
    // }
}

void Simulation::run() {

    // Initialize the clock
    sf::Clock simulationClock;
    simulationClock.restart();
    
    // Initialize the simulation time step
    sf::Clock timeStepSimClock;
    sf::Time simulationTime = sf::Time::Zero;
    float writeBufferTime = 0.f;

    // Set simulation time step
    sf::Time simulationStepTime = sf::Time::Zero;
    simulationStepTime = sf::seconds(timeStep);

    // Set the initial time step
    currentSimulationTimeStep = timeStep;
    simulationTime += simulationClock.getElapsedTime();

    // Main simulation loop
    while ((buffer.currentWriteFrameIndex < maxFrames && !stop.load())) {

        // Restart the clock
        // timeStepSimClock.restart();
        simulationClock.restart();

        // Update the agents
        update();
        
        // Write the current frame to the buffer
        buffer.write(agents);

        // Swap the read and write buffers if the read buffer is empty
        buffer.swap();

        // Update the buffer write time
        writeBufferTime += simulationClock.getElapsedTime().asSeconds();

        // Calculate the time step for the simulation
        simulationStepTime = simulationClock.getElapsedTime();

        // Calculate the total simulation time
        simulationTime += simulationStepTime;

        // Set atomic time step
        currentSimulationTimeStep = simulationStepTime.asSeconds();
    }

    // Signalize the simulation has finished so that the renderer can swap buffers if needed
    stop.store(true);

    // End the simulation
    buffer.end();

    DEBUG_MSG("Simulation: finished");

    // Print simulation statistics
    STATS_MSG("Total simulation wall time: " << simulationTime.asSeconds() << " seconds for " << buffer.currentWriteFrameIndex << " frames");
    STATS_MSG("Total simulation time: " << maxFrames * timeStep << " seconds" << " for " << numAgents << " agents");
    STATS_MSG("Simulation speedup: " << (maxFrames * timeStep) / simulationTime.asSeconds());
    STATS_MSG("Frame rate: " << 1/(simulationTime.asSeconds() / (buffer.currentWriteFrameIndex + 1)));
    STATS_MSG("Average simulation time step: " << simulationTime.asSeconds() / (buffer.currentWriteFrameIndex + 1));
    STATS_MSG("Average write buffer time: " << writeBufferTime / (buffer.currentWriteFrameIndex + 1));
}

void Simulation::update() {
    // std::vector<std::future<void>> futures;

    // // Loop through all agents and update their positions
    // for (auto& agent : agents) {

    //     // Add agent updates as tasks to the thread pool
    //     futures.emplace_back(threadPool.enqueue([this, &agent] {
    //         agent.updatePosition(timeStep);
    //     }));
    // }

    // // Wait for all agents to finish updating
    // for (auto& future : futures) {
    //     future.get();
    // }

    // Loop through all agents and update their positions
    for (auto& agent : agents) {

        // Add agent updates as tasks to the thread pool
        agent.updatePosition(timeStep);
    }
    // Update the current number of agents
    currentNumAgents = agents.size();
}