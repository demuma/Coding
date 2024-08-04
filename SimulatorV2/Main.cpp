#include <SFML/Graphics.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <queue>
#include <functional>
#include <condition_variable>
#include <future>
#include <type_traits>
#include <random>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <iomanip>
#include <sstream>

// Utilities header
#include <uuid/uuid.h>
// #include <random>
#include <cmath>
// #include <chrono>
// #include <iomanip>
// #include <sstream>
// #include <iostream>

#include "PerlinNoise.hpp"

// #define DEBUG
// #define STATS
// #define ERROR

#ifdef DEBUG
#define DEBUG_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define DEBUG_MSG(str) do { } while ( false )
#endif

#ifdef STATS
#define STATS_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define STATS_MSG(str) do { } while ( false )
#endif

#ifdef ERROR
#define ERROR_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define ERROR_MSG(str) do { } while ( false )
#endif

#ifdef TIMING
#define TIMING_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define TIMING_MSG(str) do { } while ( false )
#endif


/***************************************/
/********** THREAD POOL CLASS **********/
/***************************************/

class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();
    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;
    
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

// Constructor for the ThreadPool class
// This constructor initializes a thread pool with the specified number of worker threads.
ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    // Launch numThreads worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            // Each worker thread runs an infinite loop to process tasks
            for (;;) {
                std::function<void()> task;
                {
                    // Lock the queue mutex to safely access the task queue
                    std::unique_lock<std::mutex> lock(this->queueMutex);

                    // Wait until there is a task to execute or the thread pool is stopped
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

                    // If the thread pool is stopped and there are no remaining tasks, exit the loop
                    if (this->stop && this->tasks.empty())
                        return;

                    // Retrieve the next task from the task queue
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                // Execute the task outside of the lock to avoid blocking other threads
                task();
            }
        });
    }
}

// Enqueue a task into the thread pool and return a future to obtain the result
template <class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    
    // Determine the return type of the callable object
    using returnType = typename std::invoke_result<F, Args...>::type;
    
    // Create a shared pointer to a packaged_task that wraps the callable object
    // The packaged_task allows the task to be executed asynchronously and provides
    // a future to retrieve the result.
    auto task = std::make_shared<std::packaged_task<returnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    
    // Get the future associated with the packaged task to return to the caller.
    std::future<returnType> res = task->get_future();
    
    // Add the task to the task queue
    {
        // Lock the mutex to ensure exclusive access to the task queue
        std::unique_lock<std::mutex> lock(queueMutex);

        // Check if the thread pool has been stopped; if so, throw an exception
        // to prevent adding new tasks to a stopped thread pool.
        if (stop)
            throw std::runtime_error("Enqueue on stopped ThreadPool");

        // Add the task to the queue as a lambda that will invoke the packaged_task
        tasks.emplace([task]() { (*task)(); });
    }
    
    // Notify one of the waiting threads that a new task is available
    condition.notify_one();
    
    // Return the future to the caller so they can obtain the result later
    return res;
}

// The destructor joins all threads
ThreadPool::~ThreadPool() {

    // Stop all threads
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }

    // Notify all threads
    condition.notify_all();
    for (std::thread& worker : workers)
        worker.join();
}

/*******************************/
/********** UTILITIES **********/
/*******************************/

// Generate a unique identifier for the agent
std::string generateUUID() {

    uuid_t uuid;
    uuid_generate(uuid);
    char uuidStr[37];
    uuid_unparse(uuid, uuidStr);

    return std::string(uuidStr);
}

// Generate a unique identifier for the agent
std::string generateISOTimestamp() {

    auto now = std::chrono::system_clock::now();
    std::time_t time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%FT%TZ");

    return ss.str();
}

// Generate velocity from truncated normal distribution
float generateRandomNumberFromTND(float mean, float stddev, float min, float max) {

    // Generate normal distribution
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::normal_distribution<float> generateNormal(mean, stddev);

    // Generate random number until it falls within the specified range
    float value;
    do {

        value = generateNormal(gen);

    } while (value < min || value > max);

    return value;
}

// Generate a random velocity vector from a truncated normal distribution
sf::Vector2f generateRandomVelocityVector(float mu, float sigma, float min, float max) {
    
    // Generate distribution for angle
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> disAngle(0.0, 2 * M_PI);
    sf::Vector2f velocity;

    // Generate random velocity magnitude
    float velocityMagnitude = generateRandomNumberFromTND(mu, sigma, min, max);
    float angle = disAngle(gen);
    velocity = sf::Vector2f(velocityMagnitude * std::cos(angle), velocityMagnitude * std::sin(angle));
    
    return velocity;
}

// Generate an ISO 8601 timestamp from total elapsed time
std::string generateISOTimestamp(sf::Time totalElapsedTime) {

    // Convert totalElapsedTime to seconds
    auto totalSeconds = totalElapsedTime.asSeconds();
    
    // Convert seconds to a time_point since epoch
    auto tp = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(totalSeconds));

    // Get time_t from time_point
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);

    // Format into ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&tt), "%FT%TZ"); // Use gmtime for UTC timezone

    return ss.str();
}

// Generate an ISO 8601 timestamp from a start date string and total elapsed time
std::string generateISOTimestamp(sf::Time totalElapsedTime, const std::string& dateTimeString = "") {
    std::tm startTime{}; // Initialize to all zeros
    std::time_t start_time_t = 0;
    bool useCurrentTime = false;

    // Parse the start date string
    if (!dateTimeString.empty()) {
        std::istringstream ss(dateTimeString);
        ss >> std::get_time(&startTime, "%Y-%m-%dT%H:%M:%SZ"); // Parse with Z for UTC

        if (ss.fail()) {
            std::cerr << "Error parsing datetime string from config.yaml: '" << dateTimeString << "'. Using current time instead!" << std::endl;
            useCurrentTime = true;
        } else {
            start_time_t = timegm(&startTime); // Convert to time_t as UTC time
        }
    } else {
        useCurrentTime = true;
    }

    // Get current time if parsing failed or no date string provided
    if (useCurrentTime) {
        auto now = std::chrono::system_clock::now();
        start_time_t = std::chrono::system_clock::to_time_t(now);
    }

    // Convert totalElapsedTime to duration since epoch
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::microseconds(totalElapsedTime.asMicroseconds()));

    // Convert start time to time_point
    auto startTimePoint = std::chrono::system_clock::from_time_t(start_time_t);

    // Add the duration to the start time_point
    auto timestampTp = startTimePoint + duration;

    // Convert the new time_point back to time_t
    std::time_t tt = std::chrono::system_clock::to_time_t(timestampTp);

    // Format into ISO 8601 string
    std::stringstream outputSS;
    std::tm localTime; // Local time structure

    // Get local time with DST information
    localtime_r(&tt, &localTime);

    if (!dateTimeString.empty() && dateTimeString.back() == 'Z') { // Check if original timestamp was in UTC
        outputSS << std::put_time(std::gmtime(&tt), "%FT%TZ"); // UTC timezone
    } else {
        outputSS << std::put_time(&localTime, "%FT%T%z"); // Local timezone with offset (using localTime)
    }

    return outputSS.str();
}

// Structure to hash a 2D vector for use in unordered_map
struct Vector2iHash {
    std::size_t operator()(const sf::Vector2i& v) const {
        std::hash<int> hasher;
        return hasher(v.x) ^ (hasher(v.y) << 1); // Combine hash values of x and y
    }
};

// Convert a string to an sf::Color object
sf::Color stringToColor(std::string colorStr) {

    // Mapping of color names to sf::Color objects
    static const std::unordered_map<std::string, sf::Color> colorMap = {

        {"red", sf::Color::Red},
        {"green", sf::Color::Green},
        {"blue", sf::Color::Blue},
        {"black", sf::Color::Black},
        {"white", sf::Color::White},
        {"yellow", sf::Color::Yellow},
        {"magenta", sf::Color::Magenta},
        {"cyan", sf::Color::Cyan},
        {"pink", sf::Color(255, 192, 203)},
        {"brown", sf::Color(165, 42, 42)},
        {"turquoise", sf::Color(64, 224, 208)},
        {"gray", sf::Color(128, 128, 128)},
        {"purple", sf::Color(128, 0, 128)},
        {"violet", sf::Color(238, 130, 238)},
        {"orange", sf::Color(198, 81, 2)},
        {"indigo", sf::Color(75, 0, 130)},
        {"grey", sf::Color(128, 128, 128)}
    };

    // Convert to lowercase directly for faster comparison
    std::transform(colorStr.begin(), colorStr.end(), colorStr.begin(),
                [](unsigned char c){ return std::tolower(c); }); 

    // Fast lookup using a hash map
    auto it = colorMap.find(colorStr);
    if (it != colorMap.end()) {
        return it->second;
    }

    // Handle hex codes (same as your original implementation)
    if (colorStr.length() == 7 && colorStr[0] == '#') {
        int r, g, b;
        if (sscanf(colorStr.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
            return sf::Color(r, g, b);
        }
    }

    std::cerr << "Warning: Unrecognized color string '" << colorStr << "'. Using black instead." << std::endl;
    return sf::Color::Black;
}

/*************************************/
/********** OBSTACLES CLASS **********/
/*************************************/

// Obstacle class
class Obstacle {
public:
    Obstacle(sf::FloatRect bounds, sf::Color color = sf::Color::Black);

    sf::FloatRect getBounds() const { return bounds; }
    sf::Color getColor() const { return color; }

private:
    sf::FloatRect bounds;
    sf::Color color;
};

// Constructor for the Obstacle class
Obstacle::Obstacle(sf::FloatRect bounds, sf::Color color)
    : bounds(bounds), color(color) {}

/*********************************/
/********** AGENT CLASS **********/
/*********************************/

// Agent class
class Agent {
public:
    Agent();

    void calculateTrajectory(float waypointDistance);
    void getNextWaypoint();

    void updatePosition(float deltaTime);
    sf::Vector2f getFuturePositionAtTime(float time) const;

    void calculateVelocity(sf::Vector2f waypoint);
    void updateVelocity(float deltaTime, sf::Time totalElapsedTime);

    void stop();
    bool canResume(const std::vector<Agent>& agents);
    void resume(const std::vector<Agent>& agents);
    void resetCollisionState();
    void setBufferZoneSize();

    // Agent features
    std::string uuid;
    std::string sensor_id;
    std::string type;
    sf::Color color;
    sf::Color initialColor;
    int priority;
    float bodyRadius;

    // Positions
    sf::Vector2f position;
    sf::Vector2f initialPosition;
    sf::Vector2f targetPosition;
    sf::Vector2f heading;

    // Velocity
    sf::Vector2f velocity;
    sf::Vector2f initialVelocity;
    float velocityMagnitude;
    float minVelocity;
    float maxVelocity;
    float velocityMu;
    float velocitySigma;
    float velocityNoiseFactor;
    float velocityNoiseScale;

    // Acceleration
    sf::Vector2f acceleration;
    sf::Vector2f initialAcceleration;
    float accelerationMagnitude;
    float minAcceleration;
    float maxAcceleration;

    // Trajectory
    std::vector<sf::Vector2f>(trajectory);
    float waypointDistance;
    sf::Color waypointColor;
    int nextWaypointIndex = -1;

    // Visuals
    float bufferZoneRadius;
    float minBufferZoneRadius;
    sf::Color bufferZoneColor;

    // States
    bool collisionPredicted;
    bool stopped;
    bool isActive;
    int stoppedFrameCounter;

    // Perlin noise
    unsigned int noiseSeed;
    PerlinNoise perlinNoise;
};

// Default constructor for the Agent class
Agent::Agent(){

    collisionPredicted = false;
    stopped = false;
    isActive = true;
    stoppedFrameCounter = 0;
    minBufferZoneRadius = 0.5f;
    bufferZoneRadius = minBufferZoneRadius;
    bufferZoneColor = sf::Color::Green;
};

// Initialize the agent with default values and calculate buffer radius
void Agent::setBufferZoneSize() {

    float velocityMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    bufferZoneRadius = minBufferZoneRadius + bodyRadius + (velocityMagnitude / maxVelocity); 
}

// Calculate the velocity based on the next waypoint
void Agent::calculateVelocity(sf::Vector2f waypoint) {
    
    // Calculate velocity based on heading to the next waypoint
    float angle = std::atan2(waypoint.y - position.y, waypoint.x - position.x);
    
    // Calculate the heading vector
    sf::Vector2f heading;
    heading.x = std::cos(angle);
    heading.y = std::sin(angle);

    // Set the velocity vector based on the heading
    velocity = heading * velocityMagnitude;
}

// Update the agent's velocity based on Perlin noise
void Agent::updateVelocity(float deltaTime, sf::Time totalElapsedTime) {
    
    // Use the agent's own Perlin noise generator to fluctuate velocity
    float noiseX = perlinNoise.noise(position.x * velocityNoiseScale, position.y * velocityNoiseScale, totalElapsedTime.asSeconds()) * 2.0f - 1.0f;
    float noiseY = perlinNoise.noise(position.x * velocityNoiseScale, position.y * velocityNoiseScale, totalElapsedTime.asSeconds() + 1000.0f) * 2.0f - 1.0f;

    // Apply noise to velocity
    velocity.x = initialVelocity.x + noiseX / 3.6 * velocityNoiseFactor;
    velocity.y = initialVelocity.y + noiseY / 3.6 * velocityNoiseFactor;
}

// Update the agent's position based on velocity
void Agent::updatePosition(float timeStep) {

    // Update the position based on the velocity
    position += velocity * timeStep;
}

// Get the future position of the agent at a given time
sf::Vector2f Agent::getFuturePositionAtTime(float time) const {

    return position + velocity * time;
}

// Calculate the trajectory based on the target position and waypoint distance
void Agent::calculateTrajectory(float waypointDistance) {
    
    // Clear existing trajectory data
    trajectory.clear();

    // Add the initial position as the starting waypoint
    trajectory.push_back(initialPosition);

    // Save the initial position
    sf::Vector2f currentPosition = initialPosition;

    // Calculate total distance to target
    double totalDistance = sqrt(pow((targetPosition.x - initialPosition.x), 2) + pow((targetPosition.y - initialPosition.y), 2));
    
    // Round down to the nearest integer
    int numWaypoints = floor(totalDistance / waypointDistance);
    
    // If there are no waypoints, go directly to the target
    if (numWaypoints < 1) {
        trajectory.push_back(targetPosition);
        return;
    }
    
    // Calculate angle to the target
    double angle = atan2(targetPosition.y - initialPosition.y, targetPosition.x - initialPosition.x); // in radians
    
    // Compute delta positions
    double deltaX = waypointDistance * cos(angle);
    double deltaY = waypointDistance * sin(angle);

    // Add intermediate waypoints
    for (int i = 0; i < numWaypoints; ++i) {
        currentPosition.x += deltaX;
        currentPosition.y += deltaY;
        trajectory.push_back(currentPosition);
    }

    // Add the final target position as the last waypoint
    trajectory.push_back(targetPosition);
}

// Get the next waypoint based on the agent's trajectory
void Agent::getNextWaypoint() {

    // Get the next waypoint based on trajectory, position, and velocity
    for (int i = 0; i < trajectory.size(); ++i) {
        sf::Vector2f directionToWaypoint = trajectory[i] - position;
        float dotProduct = directionToWaypoint.x * velocity.x + directionToWaypoint.y * velocity.y;
        if (dotProduct > 0) {
            nextWaypointIndex = i;
            break;
        }
    }
}

// Reset the collision state of the agent
void Agent::resetCollisionState() {

    bufferZoneColor = sf::Color::Green;
    collisionPredicted = false;
}

// Stop the agent
void Agent::stop() {

    // Stop the agent if it is not already stopped
    if (!stopped) {
        initialVelocity = velocity;
        velocity = sf::Vector2f(0.0f, 0.0f);
        stopped = true;
        stoppedFrameCounter = 0;
    }
}

// Check if the agent can resume movement without colliding with other agents
bool Agent::canResume(const std::vector<Agent>& agents) {

    for (const auto& other : agents) {
        if (&other == this) continue;

        float dx = position.x - other.position.x;
        float dy = position.y - other.position.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        if (distance < bodyRadius + other.bodyRadius) {
            
            return false;
        }
    }
    return true;
}

// Resume the agent's movement if there are no collisions
void Agent::resume(const std::vector<Agent>& agents) {

    // Resume the agent if it is stopped and has been stopped for at least 20 frames
    if (stopped) {
        if (canResume(agents)) {
            velocity = initialVelocity;
            stopped = false;
        }
    }
}

/*****************************************/
/********** SHARED BUFFER CLASS **********/
/*****************************************/

class SharedBuffer {
public:
    SharedBuffer();
    void write(const std::vector<Agent>& frame);
    std::vector<Agent> read();
    void swap();
    void end();
    std::atomic<std::queue<std::vector<Agent>>*> currentReadBuffer;
    std::atomic<std::queue<std::vector<Agent>>*> currentWriteBuffer;
    std::atomic<int> writeBufferIndex;
    std::atomic<int> readBufferIndex;
    size_t currentReadFrameIndex;
    size_t currentWriteFrameIndex;

private:
    std::queue<std::vector<Agent>> buffers[2];
    std::mutex queueMutex;
    std::condition_variable queueCond;
    std::vector<Agent> currentFrame;
    std::atomic<bool> stopped;
};

SharedBuffer::SharedBuffer() :  writeBufferIndex(0), readBufferIndex(1), currentReadFrameIndex(0), currentWriteFrameIndex(0) {
    
        // Initialize the current read and write buffers
        currentReadBuffer.store(&buffers[readBufferIndex]);
        currentWriteBuffer.store(&buffers[writeBufferIndex]);
        stopped.store(false);

        DEBUG_MSG("Shared buffer: write buffer: " << writeBufferIndex);
        DEBUG_MSG("Shared buffer: read buffer: " << readBufferIndex);
}

// Write a frame to the current write buffer
void SharedBuffer::write(const std::vector<Agent>& frame) {

    // Lock the queue
    std::lock_guard<std::mutex> lock(queueMutex);
    DEBUG_MSG("Simulation: writing frame: " << currentWriteFrameIndex << " on buffer " << writeBufferIndex);
    
    // Write to the current write buffer
    currentWriteBuffer.load()->push(frame);

    // Increment the current write frame index
    ++currentWriteFrameIndex;
}

// Read a frame from the current read buffer
std::vector<Agent> SharedBuffer::read() {

    // If read buffer empty, wait for a buffer swap to get the next frame
    std::unique_lock<std::mutex> lock(queueMutex);
    if(currentReadBuffer.load()->empty()) {
        if(!stopped.load()){

            DEBUG_MSG("Renderer: waiting for frame: " << currentReadFrameIndex << " on buffer " << readBufferIndex);
            queueCond.wait(lock, [this] { return !currentReadBuffer.load()->empty(); });
            readBufferIndex = currentReadBuffer.load() - buffers;
            DEBUG_MSG("Renderer: read buffer swapped to " << readBufferIndex);
        } else {

            // Swap the read and write buffers
            std::queue<std::vector<Agent>>* currentWriteBuffer = &buffers[1 - readBufferIndex];
            currentReadBuffer.store(currentWriteBuffer);
        }
    }
    
    // Read current frame from the current read buffer 
    readBufferIndex = currentReadBuffer.load() - buffers; // Get the read index
    DEBUG_MSG("Renderer: rendering frame: " << currentReadFrameIndex << " in buffer " << readBufferIndex);
    
    // Read and delete the frame from the current read buffer
    currentFrame = currentReadBuffer.load()->front();
    currentReadBuffer.load()->pop();

    // Increment the current read frame index
    ++currentReadFrameIndex;

    return currentFrame;
}

// Swap the read and write buffers
void SharedBuffer::swap() {

    // Lock the queue
    std::lock_guard<std::mutex> lock(queueMutex);

    // Swap buffers if the read buffer is empty
    if (currentReadBuffer.load()->empty()) {

        DEBUG_MSG("Simulation: read buffer " << readBufferIndex.load() << " is empty");

        // Swap the read and write buffers
        currentReadBuffer.store(currentWriteBuffer);
        currentWriteBuffer.store(&buffers[readBufferIndex]);

        // Update the buffer indices
        writeBufferIndex.store(currentWriteBuffer - buffers);
        readBufferIndex.store(currentReadBuffer.load() - buffers);
        DEBUG_MSG("Simulation: swapping write buffer to " << writeBufferIndex.load());

        // Notify the renderer to continue reading from the new buffer
        queueCond.notify_one();
    }
}

// Signalize the simulation has finished so that the renderer can swap buffers if needed
void SharedBuffer::end() {
        stopped.store(true);
}

/**************************************/
/********** SIMULATION CLASS **********/
/**************************************/

// Simulation class
class Simulation {
public:
    // Simulation(std::queue<std::vector<Agent>> (&buffer)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer);
    Simulation(SharedBuffer& buffer, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config);
    void run();
    void update();
    float getCurrentFrameRate();
    void initializeAgents();
    void loadConfiguration();
    void loadObstacles();

private:
    
    // Simulation parameters
    ThreadPool threadPool;
    std::atomic<float>& currentSimulationTimeStep;
    std::atomic<bool>& stop;
    const YAML::Node& config;
    int numThreads;

    // Timing parameters
    float timeStep;
    size_t maxFrames;
    int targetSimulationTime;

    // Window parameters
    float simulationWindowWidth; // Meters
    float simulationWindowHeight; // Meters

    // Agent parameters
    int numAgents;
    std::vector<Agent> agents;
    std::atomic<int>& currentNumAgents;
    float waypointDistance;

    // Shared buffer reference
    SharedBuffer& buffer;
    size_t currentFrameIndex = 0;

    // Obstacles
    std::vector<Obstacle> obstacles;
};

// Simulation::Simulation(std::queue<std::vector<Agent>> (&buffers)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer)
Simulation::Simulation(SharedBuffer& buffer, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config)
: buffer(buffer), threadPool(std::thread::hardware_concurrency()), currentSimulationTimeStep(currentSimulationTimeStep), stop(stop), currentNumAgents(currentNumAgents), config(config) {
    
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

    // Load display parameters
    simulationWindowWidth = config["display"]["width"].as<int>() / config["display"]["pixels_per_meter"].as<float>(); // Meters
    simulationWindowHeight = config["display"]["height"].as<int>() / config["display"]["pixels_per_meter"].as<float>(); // Meters

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

    std::vector<std::future<void>> futures;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disPos(0, simulationWindowHeight); // Position
    std::uniform_real_distribution<> disVel(2, 5); // Velocity
    
    // Initialize agent vector with random velocity, start and target positions
    for (int i = 0; i < numAgents; ++i) {
        Agent agent;
        agent.velocityMagnitude = static_cast<float>(disVel(gen));
        agent.initialPosition = sf::Vector2f(0.f, static_cast<float>(disPos(gen)));
        agent.targetPosition = sf::Vector2f(simulationWindowWidth, static_cast<float>(disPos(gen)));
        agent.position = agent.initialPosition;
        agent.bodyRadius = 0.5f;
        agent.color = sf::Color::Red;
        agent.waypointDistance = waypointDistance; // -> TODO: Use taxonomy for waypoint distance
        agent.waypointColor = sf::Color::Red;
        agents.push_back(agent);
    }

    // Calculate the trajectory for each agent in parallel
    for (auto& agent: agents) {
        futures.emplace_back(threadPool.enqueue([this, &agent] {
            agent.calculateTrajectory(agent.waypointDistance);
            agent.calculateVelocity(agent.trajectory[1]);
        }));
    }
    for (auto& future : futures) {
        future.get();
    }
}

void Simulation::run() {

    // Initialize the clock
    sf::Clock simulationClock;
    simulationClock.restart();
    
    // Initialize the simulation time step
    sf::Clock timeStepSimClock;
    sf::Time simulationTime = sf::Time::Zero;

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
    STATS_MSG("Frame rate: " << 1/(simulationTime.asSeconds() / (buffer.currentWriteFrameIndex + 1)));
    STATS_MSG("Average simulation time step: " << simulationTime.asSeconds() / (buffer.currentWriteFrameIndex + 1));
}

void Simulation::update() {
    std::vector<std::future<void>> futures;

    // Loop through all agents and update their positions
    for (auto& agent : agents) {

        // Add agent updates as tasks to the thread pool
        futures.emplace_back(threadPool.enqueue([this, &agent] {
            agent.updatePosition(timeStep);
        }));
    }

    // Wait for all agents to finish updating
    for (auto& future : futures) {
        future.get();
    }

    // Update the current number of agents
    currentNumAgents = agents.size();
}

/************************************/
/********** RENDERER CLASS **********/
/************************************/

// Renderer class
class Renderer {
public:
    Renderer(SharedBuffer& buffer, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config);
    void run();
    void loadConfiguration();
    void initializeWindow();
    void initializeGUI();
    void updateFrameCountText();
    void updateFrameRateText();
    void updateAgentCountText();
    void updateTimeText();
    void updatePlayBackSpeedText();
    void handleEvents(sf::Event& event);
    sf::Time calculateSleepTime();

private:
    void render();

    // Renderer parameters
    sf::RenderWindow window;
    std::atomic<bool>& stop;
    const YAML::Node& config;

    // Display parameters
    int windowWidth;  // Pixels
    int windowHeight; // Pixels
    sf::Font font;
    std::string title;
    bool showInfo = true;

    // Window scaling
    float scale;
    float windowWidthScaled;
    float windowHeightScaled;
    float windowWidthOffsetScaled;
    float windowHeightOffsetScaled;


    // Renderer parameters
    float timeStep;
    float playbackSpeed;
    float previousPlaybackSpeed;
    bool paused = false;
    bool live_mode;

    // Visualization elements
    sf::Text frameText;
    sf::Text frameRateText;
    sf::Text agentCountText;
    sf::Text timeText;
    sf::Text playbackSpeedText;
    sf::RectangleShape pauseButton;
    sf::Text pauseButtonText;
    sf::RectangleShape resetButton;
    sf::Text resetButtonText;

    // Frame rate calculation
    std::deque<float> frameRates;
    size_t frameRateBufferSize;
    float frameRate;
    float movingAverageFrameRate;
    int maxFrames = 0;
    sf::Clock clock;
    static const int warmupFrames = 10;

    // Timing
    float targetFrameRate;
    float targetRenderTime;
    sf::Clock rendererClock;
    sf::Time rendererRealTime;
    sf::Time renderSimulationTime;
    sf::Time rendererFrameTime;
    sf::Time targetFrameTime;
    sf::Time currentSimulationFrameTime;
    std::atomic<float>& currentSimulationTimeStep;

    // Helper
    int frameEmptyCount = 0;

    // Shared buffer reference
    SharedBuffer& buffer;

    // Agents
    std::vector<Agent> currentFrame;
    std::atomic<int>& currentNumAgents;
    bool showTrajectories = true;
    bool showWaypoints = true;
};

// Renderer member functions
Renderer::Renderer(SharedBuffer& buffer, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config)
: buffer(buffer), currentSimulationTimeStep(currentSimulationTimeStep), currentNumAgents(currentNumAgents), stop(stop), config(config) {

    DEBUG_MSG("Renderer: read buffer: " << buffer.currentReadFrameIndex);
    loadConfiguration();
    initializeWindow();
    initializeGUI();
}

void Renderer::loadConfiguration() {

    // Load configuration data -> TODO: Add error handling
    windowWidth = config["display"]["width"].as<int>();
    windowHeight = config["display"]["height"].as<int>();
    title = config["display"]["title"].as<std::string>();
    timeStep = config["simulation"]["time_step"].as<float>();
    playbackSpeed = config["simulation"]["playback_speed"].as<float>();
    
    // Set the maximum number of frames if the duration is specified
    if(config["simulation"]["duration_seconds"]) {
        maxFrames = config["simulation"]["duration_seconds"].as<int>() / timeStep;
        targetRenderTime = config["simulation"]["duration_seconds"].as<int>();
    } else {
        maxFrames = config["simulation"]["maximum_frames"].as<int>();
        targetRenderTime = maxFrames * timeStep;
    }

    // Load display parameters
    showInfo = config["display"]["show_info"].as<bool>();
    showTrajectories = config["agents"]["show_trajectories"].as<bool>();
    showWaypoints = config["agents"]["show_waypoints"].as<bool>();

    // Load font
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        ERROR_MSG("Error loading font\n"); // TODO: Add logging
    }

    // Set the frame rate buffer size
    frameRateBufferSize = 1.0f / timeStep;

    // Scale the window size
    scale = config["display"]["pixels_per_meter"].as<float>();
    windowWidthScaled = windowWidth * scale;
    windowHeightScaled = windowHeight * scale;

    // Calculate the window offset for centering the window
    windowWidthOffsetScaled = (windowWidth - windowWidthScaled) / 2.0f;
    windowHeightOffsetScaled = (windowHeight - windowHeightScaled) / 2.0f;
}

void Renderer::initializeWindow() {

    // Create window
    window.create(sf::VideoMode(windowWidth, windowHeight), title);
}

void Renderer::initializeGUI() {

    // Frame count text
    frameText.setFont(font);
    frameText.setCharacterSize(24);
    frameText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop

    // Frame rate text
    frameRateText.setFont(font);
    frameRateText.setCharacterSize(24);
    frameRateText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop

    // Frame count text
    agentCountText.setFont(font);
    agentCountText.setCharacterSize(24);
    agentCountText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop

    // Frame count text
    timeText.setFont(font);
    timeText.setCharacterSize(24);
    timeText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop

    // Frame count text
    playbackSpeedText.setFont(font);
    playbackSpeedText.setCharacterSize(24);
    playbackSpeedText.setFillColor(sf::Color::Black);
    // Initial position will be updated in the main loop
    
    // Pause button
    pauseButton.setSize(sf::Vector2f(100, 50)); 
    pauseButton.setFillColor(sf::Color::Green);
    pauseButton.setPosition(window.getSize().x - 110, window.getSize().y - 60);

    pauseButtonText.setFont(font);
    pauseButtonText.setString("Pause");
    pauseButtonText.setCharacterSize(20);
    pauseButtonText.setFillColor(sf::Color::Black);
    
    // Center the pause button text
    sf::FloatRect pauseButtonTextRect = pauseButtonText.getLocalBounds();
    pauseButtonText.setOrigin(pauseButtonTextRect.left + pauseButtonTextRect.width / 2.0f,
                              pauseButtonTextRect.top + pauseButtonTextRect.height / 2.0f);
    pauseButtonText.setPosition(pauseButton.getPosition().x + pauseButton.getSize().x / 2.0f,
                                pauseButton.getPosition().y + pauseButton.getSize().y / 2.0f);

    // Reset button
    resetButton.setSize(sf::Vector2f(100, 50));
    resetButton.setFillColor(sf::Color::Cyan);
    resetButton.setPosition(window.getSize().x - 110, window.getSize().y - 120); 

    resetButtonText.setFont(font);
    resetButtonText.setString("Infos");
    resetButtonText.setCharacterSize(20);
    resetButtonText.setFillColor(sf::Color::Black);

    // Center the reset button text
    sf::FloatRect resetButtonTextRect = resetButtonText.getLocalBounds();
    resetButtonText.setOrigin(resetButtonTextRect.left + resetButtonTextRect.width / 2.0f,
                              resetButtonTextRect.top + resetButtonTextRect.height / 2.0f);
    resetButtonText.setPosition(resetButton.getPosition().x + resetButton.getSize().x / 2.0f,
                                resetButton.getPosition().y + resetButton.getSize().y / 2.0f);

}

// Function to update the text that displays the current frame count
void Renderer::updateFrameCountText() {

    frameText.setString("Frame " + std::to_string(buffer.currentReadFrameIndex) + " / " + (maxFrames > 0 ? std::to_string(maxFrames) : "∞")); // ∞ for unlimited frames
    sf::FloatRect textRect = frameText.getLocalBounds();
    frameText.setOrigin(textRect.width, 0); // Right-align the text
    frameText.setPosition(window.getSize().x - 10, 40); // Position with padding
}

// Function to update the text that displays the current agent count
void Renderer::updateAgentCountText() {

    agentCountText.setString("Agents: " + std::to_string(currentNumAgents));
    sf::FloatRect textRect = agentCountText.getLocalBounds();
    agentCountText.setOrigin(textRect.width, 0); // Right-align the text
    agentCountText.setPosition(window.getSize().x - 6, 100); // Position with padding
}

// Function to update the text that displays the current elapsed time
void Renderer::updateTimeText() {

    // Convert seconds to HH:MM:SS for totalElapsedTime
    // auto totalElapsedTime = rendererClock.getElapsedTime();
    auto totalElapsedTime = renderSimulationTime;
    int elapsedHours = static_cast<int>(totalElapsedTime.asSeconds()) / 3600;
    int elapsedMinutes = (static_cast<int>(totalElapsedTime.asSeconds()) % 3600) / 60;
    int elapsedSeconds = static_cast<int>(totalElapsedTime.asSeconds()) % 60;
    int elapsedMilliseconds = (totalElapsedTime.asSeconds() - static_cast<int>(totalElapsedTime.asSeconds())) * 1000;

    std::string targetRenderTimeString = "∞";  // Default to infinity symbol
    if(targetRenderTime > 0) {
        int targetHours = static_cast<int>(targetRenderTime) / 3600;
        int targetMinutes = (static_cast<int>(targetRenderTime)) % 3600 / 60;
        int targetSeconds = static_cast<int>(targetRenderTime) % 60;
        int targetMilliseconds = (targetRenderTime - static_cast<int>(targetRenderTime)) * 1000;

        // Format the duration string using stringstream
        std::ostringstream targetRenderTimeStream;
        targetRenderTimeStream << std::setfill('0') << std::setw(2) << targetHours << ":"
                       << std::setw(2) << targetMinutes << ":"
                       << std::setw(2) << targetSeconds << ":"
                       << std::setw(3) << targetMilliseconds;
        targetRenderTimeString = targetRenderTimeStream.str();
    }

    // Format the time string using stringstream
    std::ostringstream timeStream;
    timeStream << "Time: " << std::setfill('0') << std::setw(2) << elapsedHours << ":"
               << std::setw(2) << elapsedMinutes << ":"
               << std::setw(2) << elapsedSeconds << ":"
               << std::setw(3) << elapsedMilliseconds << " / " << targetRenderTimeString;
    timeText.setString(timeStream.str()); 

    // Right-align and position the text
    sf::FloatRect textRect = timeText.getLocalBounds();
    timeText.setOrigin(textRect.width, 0); 
    timeText.setPosition(window.getSize().x - 10, 10); 
}

// Function to calculate the moving average frame rate and update the text after warmup
void Renderer::updateFrameRateText() {

    // Wait for the warmup frames to pass
    if(buffer.currentReadFrameIndex < warmupFrames) {
        return;
    }
    
    // Wait for frame buffer to fill up
    if(frameRates.size() != frameRateBufferSize) {
        frameRates.push_back(frameRate);
        
    } else {

        // Calculate the moving average frame rate
        float sum = std::accumulate(frameRates.begin(), frameRates.end(), 0.0f);
        movingAverageFrameRate = sum / frameRates.size();
        
        // Update the frame rate text
        frameRateText.setString("FPS: " + std::to_string(static_cast<int>(movingAverageFrameRate)));
        sf::FloatRect textRect = frameRateText.getLocalBounds();
        frameRateText.setOrigin(textRect.width, 0); // Right-align the text
        frameRateText.setPosition(window.getSize().x - 10, 70); // Position with padding

        // Update the frame rate buffer
        frameRates.pop_front();
        frameRates.push_back(frameRate);
    }
}

// Function to update the text that displays the current frame count
void Renderer::updatePlayBackSpeedText() {

    std::ostringstream playbackSpeedStream;
    playbackSpeedStream << std::fixed << std::setprecision(1) << "Playback Speed: " << playbackSpeed << std::setw(2) << "x";

    // playbackSpeedText.setString("Playback Speed: " + std::to_string(playbackSpeed));
    playbackSpeedText.setString(playbackSpeedStream.str());
    sf::FloatRect textRect = playbackSpeedText.getLocalBounds();
    playbackSpeedText.setOrigin(textRect.width, 0); // Right-align the text
    playbackSpeedText.setPosition(window.getSize().x - 6, 130); // Position with padding
}

// Function to handle events
void Renderer::handleEvents(sf::Event& event) {

    if (event.type == sf::Event::Closed) {
        window.close();
        stop.store(true);
        return;
    }
    else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Space) {
            paused = !paused;
            pauseButtonText.setString(paused ? "  Play" : "Pause");
            pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green);
            render();
            return;
        }
        else if (event.key.code == sf::Keyboard::T) {
            showTrajectories = !showTrajectories;
            return;
        }
        else if (event.key.code == sf::Keyboard::W) {
            showWaypoints = !showWaypoints;
            return;
        }
        else if (event.key.code == sf::Keyboard::Escape) {
            window.close();
            stop.store(true);
            return;
        }
        else if (event.key.code == sf::Keyboard::R) {
            playbackSpeed = 1.0f;
            DEBUG_MSG("Playback speed reset to: " << playbackSpeed);
            return;
        }
        else if (event.key.code == sf::Keyboard::Up) {
            if (playbackSpeed >= 1.0f) {
                playbackSpeed += 1.0f;
                playbackSpeed = std::floor(playbackSpeed);
            } else if (playbackSpeed < 1.0f && playbackSpeed > 0.1f) {
                playbackSpeed += 0.1f;
            } else {
                playbackSpeed = 0.1f;
            }
            DEBUG_MSG("Playback speed increased to: " << playbackSpeed);
            return;
        }
        else if (event.key.code == sf::Keyboard::Down) {
            if (playbackSpeed > 1.0f) {
                playbackSpeed -= 1.0f;
                playbackSpeed = std::ceil(playbackSpeed);
            } else if (playbackSpeed <= 1.0f && playbackSpeed > 0.1f) {
                playbackSpeed -= 0.1f;
            } else {
                playbackSpeed = 0.1f;
            }
            DEBUG_MSG("Playback speed decreased to: " << playbackSpeed);
            return;
        }
    } else if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
            if (pauseButton.getGlobalBounds().contains(mousePosition.x, mousePosition.y)) {
                paused = !paused;
                pauseButtonText.setString(paused ? "  Play" : "Pause");
                pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green);
                render();
            } else if (resetButton.getGlobalBounds().contains(mousePosition.x, mousePosition.y)) {
                showInfo = !showInfo;
            }
        }
    }
}

void Renderer::run() {

    // Set the target frame rate
    targetFrameRate = playbackSpeed * (1.0f / timeStep);
    targetFrameTime = sf::seconds(1.0f / targetFrameRate);

    // Initialize the event
    sf::Event event;

    // Timing variables
    sf::Clock rendererFrameClock;
    
    // Start the clock
    rendererClock.restart();
    rendererRealTime = sf::Time::Zero;
    renderSimulationTime = sf::Time::Zero;
    sf::Time timeStepTime = sf::seconds(timeStep);
    sf::Time rendererTotalFrameTime; // Total time taken to render the frame

    // Frame rate calculation variables
    sf::Clock frameTimeClock;
    float rendererFrameRate;

    // Add the setup time to the frame rate buffer
    rendererRealTime += rendererClock.restart();

    // Main rendering loop
    while (window.isOpen() && buffer.currentReadFrameIndex < maxFrames) {

        // Reset the frame time clock
        rendererFrameClock.restart();

        // Handle events
        while (window.pollEvent(event)) {
            handleEvents(event);
        }

        // Run the renderer if not paused
        if(!paused) {

            // Read the current frame from the buffer
            currentFrame = buffer.read();

            // Get the current simulation time step
            currentSimulationFrameTime = sf::seconds(currentSimulationTimeStep.load());

            // Update the text elements
            updateAgentCountText();
            updateFrameCountText();
            updateTimeText();
            updateFrameRateText();
            updatePlayBackSpeedText();

            // Render
            render();

            // Get the time taken to render the frame
            rendererFrameTime = rendererFrameClock.getElapsedTime();

            // Calculate the sleep time
            auto sleep_time = calculateSleepTime();
            std::this_thread::sleep_for(std::chrono::duration<float>(sleep_time.asSeconds()));

            // Get the total time taken to render the frame
            rendererTotalFrameTime = rendererFrameClock.getElapsedTime();
            rendererFrameRate = 1.0f / rendererTotalFrameTime.asSeconds();
            frameRate = rendererFrameRate;

            // Update the render time
            rendererRealTime += rendererTotalFrameTime;
            renderSimulationTime += timeStepTime;
        }
    }
    STATS_MSG("Total render wall time: " << rendererRealTime.asSeconds() << " seconds for " << buffer.currentReadFrameIndex << " frames");
}

sf::Time Renderer::calculateSleepTime() {

    // Calculate the target frame rate based on the playback speed
    targetFrameRate = playbackSpeed * (1.0f / timeStep);
    targetFrameTime = sf::seconds(1.0f / targetFrameRate);

    // Check whether target frame time with the current playback speed is greater than the current frame time
    if(targetFrameTime < currentSimulationFrameTime){
        targetFrameTime = currentSimulationFrameTime;
        playbackSpeed = timeStep / currentSimulationFrameTime.asSeconds();
        DEBUG_MSG("Playback speed adjusted to: " << playbackSpeed);
        
    }

    if (rendererFrameTime >= targetFrameTime) {
        // The rendering frame time is already slower than the target frame rate; do not sleep.
        return sf::Time::Zero;
    }

    if (currentSimulationFrameTime >= targetFrameTime) {
        // Simulation frame time is greater than or equal to target rendering time.
        return sf::Time::Zero;  // No sleep needed, as we can't render faster than the simulation allows.
    }

    // Calculate the remaining time to meet the target rendering frame time
    sf::Time remainingTime = targetFrameTime - rendererFrameTime;;

    // Ensure the sleep time is non-negative
    return std::max(remainingTime, sf::Time::Zero);
}

void Renderer::render() {  // Input: meters, Output: pixels
    window.clear(sf::Color::White);

    if (currentFrame.size() != 0) {
        for (auto& agent : currentFrame) {

            // Scale agent data (window offset missing!)
            agent.position *= scale;
            agent.initialPosition *= scale;
            agent.targetPosition *= scale;
            agent.bodyRadius *= scale;
            agent.velocity *= scale;

            // Scale the trajectory waypoints
            for(auto& waypoint : agent.trajectory) {
                waypoint *= scale;
            }

            // Determine the next waypoint index that is ahead of the agent
            if(showWaypoints) {
  
                // Draw the trajectory waypoints ahead of the agent
                sf::VertexArray waypoints(sf::Points);

                // Calculate the next waypoint
                agent.getNextWaypoint();

                // Draw the waypoints
                if (agent.nextWaypointIndex != -1) {
                    for (size_t i = agent.nextWaypointIndex; i < agent.trajectory.size(); ++i) {
                        
                        // Append the scaled waypoint to the vertex array
                        waypoints.append(sf::Vertex(agent.trajectory[i], agent.waypointColor));
                    }
                }
                window.draw(waypoints);
            }   

            // Draw the trajectory line from initial position to current position
            if(showTrajectories) {
                sf::Vertex trajectory[] = {sf::Vertex(agent.initialPosition, sf::Color::Blue), sf::Vertex(agent.position, sf::Color::Blue)};
                window.draw(trajectory, 2, sf::Lines);
            }

            // Draw the agent
            sf::CircleShape agentShape;
            agentShape.setPosition(agent.position.x - agent.bodyRadius, agent.position.y - agent.bodyRadius);
            agentShape.setRadius(agent.bodyRadius);
            agentShape.setFillColor(agent.color);
            window.draw(agentShape);
        }

        if(showInfo) {
            window.draw(frameText);
            window.draw(frameRateText);
            window.draw(agentCountText);
            window.draw(timeText);
            window.draw(playbackSpeedText);
        }

        // Draw buttons
        window.draw(pauseButton);
        window.draw(pauseButtonText);
        window.draw(resetButton);
        window.draw(resetButtonText);

        window.display();

    } else {
        ++frameEmptyCount;
        ERROR_MSG("Frame is empty: " << frameEmptyCount);
    }
}

/**************************/
/********** MAIN **********/
/**************************/

// Main function
int main() {

    // Configuration Loading
    YAML::Node config;
    try {
        config = YAML::LoadFile("config.yaml");
    } catch (const YAML::Exception& e) {
        ERROR_MSG("Error loading config file: " << e.what());
        return 1;
    }

    // Buffer for shared data
    SharedBuffer buffer;

    // Load global configuration data
    float timeStep = config["simulation"]["time_step"].as<float>();
    int maxFrames = config["simulation"]["maximum_frames"].as<int>();
    int numAgents = config["agents"]["num_agents"].as<int>();

    // Shared variabes
    std::atomic<float> currentSimulationTimeStep{timeStep};
    std::atomic<int> numSimulationFrames{maxFrames};
    std::atomic<bool> stop{false};
    std::atomic<int> currentNumAgents{numAgents};

    Simulation simulation(buffer, currentSimulationTimeStep, stop, currentNumAgents, config);
    Renderer renderer(buffer, currentSimulationTimeStep, stop, currentNumAgents, config);

    std::thread simulationThread(&Simulation::run, &simulation);

    renderer.run();
    simulationThread.join();

    return 0;
}