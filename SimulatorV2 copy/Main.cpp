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

/*********************************/
/********** AGENT CLASS **********/
/*********************************/

// Agent class
class Agent {
public:
    Agent();

    void calculateTrajectory(float waypointDistance);
    void calculateVelocity(sf::Vector2f waypoint);
    void updatePosition(float deltaTime);

    sf::FloatRect rect;
    sf::Vector2f velocity;
    float velocityMagnitude;
    sf::Vector2f position;
    sf::Vector2f initialPosition;
    sf::Vector2f targetPosition;
    sf::Vector2f heading;
    float waypointDistance;
    float radius;
    std::vector<sf::Vector2f>(trajectory);
};

Agent::Agent(){};

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

void Agent::updatePosition(float timeStep) {

    // Update the position based on the velocity
    position.x += velocity.x * timeStep;
    position.y += velocity.y * timeStep;
}

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

/*****************************************/
/********** SHARED BUFFER CLASS **********/
/*****************************************/

class SharedBuffer {
public:
    SharedBuffer();
    void write(const std::vector<Agent>& frame, size_t currentFrameIndex);
    std::vector<Agent> read(size_t currentFrameIndex);
    void swap(size_t currentFrameIndex);
    void end();

private:
    std::queue<std::vector<Agent>> buffers[2];
    std::mutex queueMutex;
    std::condition_variable queueCond;
    std::atomic<std::queue<std::vector<Agent>>*> currentReadBuffer;
    std::atomic<std::queue<std::vector<Agent>>*> currentWriteBuffer;
    std::atomic<int> writeBufferIndex;
    std::atomic<int> readBufferIndex;
    std::atomic<bool> stopped;
    std::vector<Agent> currentFrame;
};

SharedBuffer::SharedBuffer() :  writeBufferIndex(0), readBufferIndex(1) {
    
        // Initialize the current read and write buffers
        currentReadBuffer.store(&buffers[readBufferIndex]);
        currentWriteBuffer.store(&buffers[writeBufferIndex]);
        stopped.store(false);
}

// Write a frame to the current write buffer
void SharedBuffer::write(const std::vector<Agent>& frame, size_t currentFrameIndex) {

    // Lock the queue
    std::lock_guard<std::mutex> lock(queueMutex);
    DEBUG_MSG("Simulation: writing frame: " << currentFrameIndex << " on buffer " << writeBufferIndex);
    // Write to the current write buffer
    currentWriteBuffer.load()->push(frame);
}

// Read a frame from the current read buffer
std::vector<Agent> SharedBuffer::read(size_t currentFrameIndex) {

    // If read buffer empty, wait for a buffer swap to get the next frame
    std::unique_lock<std::mutex> lock(queueMutex);
    if(currentReadBuffer.load()->empty()) {
        if(!stopped.load()){

            DEBUG_MSG("Renderer: waiting for frame: " << currentFrameIndex << " on buffer " << readBufferIndex);
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
    DEBUG_MSG("Renderer: rendering frame: " << currentFrameIndex << " in buffer " << readBufferIndex);
    
    // Read and delete the frame from the current read buffer
    currentFrame = currentReadBuffer.load()->front();
    currentReadBuffer.load()->pop();

    return currentFrame;
}

// Swap the read and write buffers
void SharedBuffer::swap(size_t currentFrameIndex) {

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

/*
    int main() {
        
        SharedBuffer buffer;

        Renderer renderer(buffer, ...);
        Simulation simulation(buffer, ...);

        std::thread simulationThread(&Simulation::run, &simulation);
        renderer.run();

        simulationThread.join();
    }
*/
    

/**************************************/
/********** SIMULATION CLASS **********/
/**************************************/

// Simulation class
class Simulation {
public:
    Simulation(std::queue<std::vector<Agent>> (&buffer)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer);
    void run();
    void update();
    float getCurrentFrameRate();
    void initializeAgents();
    void loadConfiguration();

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
    int windowWidth;
    int windowHeight;

    // Agent parameters
    int numAgents;
    std::vector<Agent> agents;
    std::atomic<int>& currentNumAgents;
    float waypointDistance;


    // Shared buffer -> TODO: Make SharedBuffer class
    std::queue<std::vector<Agent>> *buffers; // Reference to the queue
    std::mutex &queueMutex;
    std::condition_variable &queueCond;

    // Buffers
    size_t currentFrameIndex;
    std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer;
    std::queue<std::vector<Agent>>* currentWriteBuffer;
};

Simulation::Simulation(std::queue<std::vector<Agent>> (&buffers)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer)
: buffers(buffers), queueMutex(queueMutex), queueCond(queueCond), currentReadBuffer(currentReadBuffer), threadPool(std::thread::hardware_concurrency()), currentSimulationTimeStep(currentSimulationTimeStep), stop(stop), currentNumAgents(currentNumAgents), config(config) {
    
    // Set the initial write buffer (second queue buffer) -> TODO: Make SharedBuffer class and move this logic there
    currentWriteBuffer = &buffers[1];
    DEBUG_MSG("Simulation: write buffer: " << (currentWriteBuffer - buffers));
    
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
    windowHeight = config["display"]["height"].as<int>();
    windowWidth = config["display"]["width"].as<int>();

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

void Simulation::initializeAgents() {

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disPos(0, windowHeight); // Position
    std::uniform_real_distribution<> disVel(10, 50); // Velocity
    float agentRadius = 5.f;
    
    // Initialize agent vector with random velocity, start and target positions
    for (int i = 0; i < numAgents; ++i) {
        Agent agent;
        agent.velocityMagnitude = static_cast<float>(disVel(gen));
        agent.initialPosition = sf::Vector2f(0.f, static_cast<float>(disPos(gen)));
        agent.targetPosition = sf::Vector2f(windowWidth, static_cast<float>(disPos(gen)));
        agent.position = agent.initialPosition;
        agent.radius = agentRadius;
        agent.waypointDistance = waypointDistance; // -> TODO: Use taxonomy for waypoint distance
        agents.push_back(agent);
    }

    // Calculate the trajectory for each agent in parallel
    for (auto& agent: agents) {
        agent.calculateTrajectory(agent.waypointDistance);
        agent.calculateVelocity(agent.trajectory[1]);
    }
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

    // Get indices for both buffers
    int writeBufferIndex = currentWriteBuffer - buffers;
    int readBufferIndex = currentReadBuffer.load() - buffers;
    simulationTime += simulationClock.getElapsedTime();

    // Main simulation loop
    while ((currentFrameIndex < maxFrames && !stop.load())) {

        // Restart the clock
        // timeStepSimClock.restart();
        simulationClock.restart();

        // Update the agents
        update();

        //===================================================== -> TODO: Move this logic to a separate function
        // Write to the current write buffer
        
        {   
            // Lock the queue
            DEBUG_MSG("Simulation: writing frame: " << currentFrameIndex << " in buffer " << writeBufferIndex);
            std::lock_guard<std::mutex> lock(queueMutex);

            // Write to the current write buffer
            currentWriteBuffer->push(agents);

            // Notify the renderer to continue reading from its buffer
            queueCond.notify_one();
        }

        // Swap buffers if the read buffer is empty
        {
            // Lock the queue
            std::lock_guard<std::mutex> lock(queueMutex);
            if (currentReadBuffer.load()->empty()) {

                DEBUG_MSG("Simulation: read buffer " << readBufferIndex << " is empty");

                // Swap the read and write buffers
                currentReadBuffer.store(currentWriteBuffer);
                currentWriteBuffer = &buffers[readBufferIndex];

                // Update the buffer indices
                writeBufferIndex = currentWriteBuffer - buffers;
                readBufferIndex = currentReadBuffer.load() - buffers;
                DEBUG_MSG("Simulation: swapping write buffer to " << writeBufferIndex);

                // Notify the renderer to continue reading from the new buffer
                queueCond.notify_one();
            }
        }
        writeBufferTime += simulationClock.getElapsedTime().asSeconds();

        //====================================================
        
        // Increment the frame index
        ++currentFrameIndex;

        // Calculate the time step for the simulation
        simulationStepTime = simulationClock.getElapsedTime();

        // Calculate the total simulation time
        simulationTime += simulationStepTime;

        // Set atomic time step
        currentSimulationTimeStep = simulationStepTime.asSeconds();
    }

    // Signalize the simulation has finished so that the renderer can swap buffers if needed
    stop.store(true);

    DEBUG_MSG("Simulation: finished");

    // Print simulation statistics
    STATS_MSG("Total simulation wall time: " << simulationTime.asSeconds() << " seconds for " << currentFrameIndex << " frames");
    STATS_MSG("Total simulation time: " << maxFrames * timeStep << " seconds" << " for " << numAgents << " agents");
    STATS_MSG("Simulation speedup: " << (maxFrames * timeStep) / simulationTime.asSeconds());
    STATS_MSG("Frame rate: " << 1/(simulationTime.asSeconds() / (currentFrameIndex + 1)));
    STATS_MSG("Average simulation time step: " << simulationTime.asSeconds() / (currentFrameIndex + 1));
    STATS_MSG("Average write buffer time: " << writeBufferTime / (currentFrameIndex + 1));
}

void Simulation::update() {

    // Loop through all agents and update their positions
    for (auto& agent : agents) {

        // Add agent updates as tasks to the thread pool
        agent.updatePosition(timeStep);
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
    Renderer(std::queue<std::vector<Agent>> (&buffer)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer);
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
    sf::RenderWindow window;
    sf::CircleShape agentShape;
    size_t currentFrameIndex = 0;
    std::atomic<float>& currentSimulationTimeStep;
    std::atomic<bool>& stop;
    const YAML::Node& config;

    // Display parameters
    int windowWidth;
    int windowHeight;
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

    // Timing (NEW)
    float targetFrameRate;
    int targetRenderTime;
    sf::Clock rendererClock;
    sf::Time rendererRealTime;
    sf::Time renderSimulationTime;
    sf::Time rendererFrameTime;
    sf::Time targetFrameTime;
    sf::Time currentSimulationFrameTime;

    // Helper
    int frameEmptyCount = 0;

    std::queue<std::vector<Agent>> *buffers; // Reference to the queue
    std::mutex &queueMutex;
    std::condition_variable &queueCond;
    std::atomic<std::queue<std::vector<Agent>>*> &currentReadBuffer;

    // Agents
    std::vector<Agent> currentFrame;
    std::atomic<int>& currentNumAgents;
    bool showTrajectories = true;
    bool showWaypoints = true;
};

// Renderer member functions
Renderer::Renderer(std::queue<std::vector<Agent>> (&buffers)[2], std::mutex &queueMutex, std::condition_variable &queueCond, std::atomic<float>& currentSimulationTimeStep, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config, std::atomic<std::queue<std::vector<Agent>>*>& currentReadBuffer)
: buffers(buffers), queueMutex(queueMutex), queueCond(queueCond), currentReadBuffer(currentReadBuffer), currentSimulationTimeStep(currentSimulationTimeStep), currentNumAgents(currentNumAgents), stop(stop), config(config) {

    DEBUG_MSG("Renderer constructor read buffer: " << currentReadBuffer.load() - buffers);
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
    scale = config["display"]["pixels_per_meter"].as<float>();

    // Set the maximum number of frames if the duration is specified
    if(config["simulation"]["duration_seconds"]) {
        maxFrames = config["simulation"]["duration_seconds"].as<int>() / timeStep;
        targetRenderTime = config["simulation"]["duration_seconds"].as<int>();
    } else {
        maxFrames = config["simulation"]["maximum_frames"].as<int>();
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

    frameText.setString("Frame " + std::to_string(currentFrameIndex) + " / " + (maxFrames > 0 ? std::to_string(maxFrames) : "∞")); // ∞ for unlimited frames
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

    std::string targetRenderTimeString = "∞";  // Default to infinity symbol
    if(targetRenderTime > 0) {
        int targetHours = targetRenderTime / 3600;
        int targetMinutes = (targetRenderTime) % 3600 / 60;
        int targetSeconds = targetRenderTime % 60;

        // Format the duration string using stringstream
        std::ostringstream targetRenderTimeStream;
        targetRenderTimeStream << std::setfill('0') << std::setw(2) << targetHours << ":"
                       << std::setw(2) << targetMinutes << ":"
                       << std::setw(2) << targetSeconds;
        targetRenderTimeString = targetRenderTimeStream.str();
    }

    // Format the time string using stringstream
    std::ostringstream timeStream;
    timeStream << "Time: " << std::setfill('0') << std::setw(2) << elapsedHours << ":"
               << std::setw(2) << elapsedMinutes << ":"
               << std::setw(2) << elapsedSeconds << " / " << targetRenderTimeString;
    timeText.setString(timeStream.str()); 

    // Right-align and position the text
    sf::FloatRect textRect = timeText.getLocalBounds();
    timeText.setOrigin(textRect.width, 0); 
    timeText.setPosition(window.getSize().x - 10, 10); 
}

// Function to calculate the moving average frame rate and update the text after warmup
void Renderer::updateFrameRateText() {

    // Wait for the warmup frames to pass
    if(currentFrameIndex < warmupFrames) {
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

    DEBUG_MSG("Run renderer read buffer: " << currentReadBuffer.load() - buffers);

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
    sf::Time readBufferTime = sf::Time::Zero;
    sf::Time rendererTotalFrameTime; // Total time taken to render the frame
    float averageRendererFrameTime = 0;
    float averagereadBufferTime = 0;

    // Frame rate calculation variables
    sf::Clock frameTimeClock;
    float rendererFrameRate;

    // Add the setup time to the frame rate buffer
    rendererRealTime += rendererClock.restart();

    // Main rendering loop
    while (window.isOpen() && currentFrameIndex < maxFrames) {

        // Reset the frame time clock
        rendererFrameClock.restart();

        // Handle events
        while (window.pollEvent(event)) {
            handleEvents(event);
        }

        // Run the renderer if not paused
        if(!paused) {


            //===================================================== -> TODO: Move this logic to a separate function
            // Get the read buffer index
            int readBufferIndex = currentReadBuffer.load() - buffers;

            // If read buffer empty, wait for a buffer swap to get the next frame
            {   
                // Lock the queue
                std::unique_lock<std::mutex> lock(queueMutex);
                if(currentReadBuffer.load()->empty()) {
                    if(!stop.load()){

                        DEBUG_MSG("Renderer waiting for frame: " << currentFrameIndex << " on buffer " << readBufferIndex);
                        queueCond.notify_one();

                        // Continue after a timeout of maxTimeStep in case of the simulation being finished
                        queueCond.wait(lock, [this] { return !currentReadBuffer.load()->empty(); });
                        readBufferIndex = currentReadBuffer.load() - buffers;
                        DEBUG_MSG("Renderer: Read buffer swapped to " << readBufferIndex);
                    } else {
                        // Swap the read and write buffers
                        std::queue<std::vector<Agent>>* currentWriteBuffer = &buffers[1 - readBufferIndex];
                        currentReadBuffer.store(currentWriteBuffer);
                    }
                }
            }
            
            // Read current frame from the current read buffer
            {   
                // Lock the queue
                std::lock_guard<std::mutex> lock(queueMutex);
                readBufferIndex = currentReadBuffer.load() - buffers; // Get the read index
                DEBUG_MSG("Rendering frame: " << currentFrameIndex << " in buffer " << readBufferIndex);
                
                // Read and delete the frame from the current read buffer
                currentFrame = currentReadBuffer.load()->front();
                currentReadBuffer.load()->pop();
            }

            //====================================================
            readBufferTime = rendererFrameClock.getElapsedTime();
            averagereadBufferTime += readBufferTime.asSeconds();
            TIMING_MSG("Read buffer time: " << readBufferTime.asSeconds() << " seconds");

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
            averageRendererFrameTime += rendererFrameTime.asSeconds();
            TIMING_MSG("Render time: " << rendererFrameTime.asSeconds() - readBufferTime.asSeconds() << " seconds");

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

            // Frame rate calculation
            ++currentFrameIndex;
        }
    }
    averageRendererFrameTime = averageRendererFrameTime / (currentFrameIndex + 1);
    averagereadBufferTime = averagereadBufferTime / (currentFrameIndex + 1);
    STATS_MSG("Total render wall time: " << rendererRealTime.asSeconds() << " seconds for " << currentFrameIndex << " frames");
    STATS_MSG("Frame rate: " << 1/averageRendererFrameTime);
    STATS_MSG("Average read buffer time: " << averagereadBufferTime << " seconds");
    STATS_MSG("Average render time per frame: " << averageRendererFrameTime << " seconds");
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

void Renderer::render() {
    window.clear(sf::Color::White);

    if (currentFrame.size() != 0) {
        for (const auto& agent : currentFrame) {

            // Determine the next waypoint index that is ahead of the agent
            if(showWaypoints) {
                int nextWaypointIndex = -1;
                for (int i = 0; i < agent.trajectory.size(); ++i) {
                    sf::Vector2f directionToWaypoint = agent.trajectory[i] - agent.position;
                    float dotProduct = directionToWaypoint.x * agent.velocity.x + directionToWaypoint.y * agent.velocity.y;
                    if (dotProduct > 0) {
                        nextWaypointIndex = i;
                        break;
                    }
                }

                // Draw the trajectory waypoints ahead of the agent
                sf::VertexArray waypoints(sf::Points);
                if (nextWaypointIndex != -1) {
                    for (size_t i = nextWaypointIndex; i < agent.trajectory.size(); ++i) {
                        waypoints.append(sf::Vertex(agent.trajectory[i], sf::Color::Black));
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
            agentShape.setPosition(agent.position.x - agent.radius, agent.position.y - agent.radius);
            agentShape.setRadius(agent.radius);
            agentShape.setFillColor(sf::Color::Red);
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

    // Shared queue and mutex
    std::queue<std::vector<Agent>> queueBuffers[2];
    std::mutex queueMutex;
    std::condition_variable queueCond;

    // Load global configuration data
    float timeStep = config["simulation"]["time_step"].as<float>();
    int maxFrames = config["simulation"]["maximum_frames"].as<int>();
    int numAgents = config["agents"]["num_agents"].as<int>();

    // Shared variabes
    std::atomic<float> currentSimulationTimeStep{timeStep};
    std::atomic<int> numSimulationFrames{maxFrames};
    std::atomic<bool> stop{false};
    std::atomic<int> currentNumAgents{numAgents};
    std::atomic<std::queue<std::vector<Agent>>*> currentReadBuffer{&queueBuffers[0]};

    Simulation simulation(queueBuffers, queueMutex, queueCond,currentSimulationTimeStep, stop, currentNumAgents, config, currentReadBuffer);
    Renderer renderer(queueBuffers, queueMutex, queueCond, currentSimulationTimeStep, stop, currentNumAgents, config, currentReadBuffer);

    std::thread simulationThread(&Simulation::run, &simulation);
    // simulation.run();
    renderer.run();
    simulationThread.join();

    return 0;
}