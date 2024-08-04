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

// The constructor just launches some amount of workers
ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i)
        workers.emplace_back([this] {
            for(;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    if (this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
}

// Add new work item to the pool
template <class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using returnType = typename std::invoke_result<F, Args...>::type;
    auto task = std::make_shared<std::packaged_task<returnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<returnType> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}

// The destructor joins all threads
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
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
    float radius;
    std::vector<sf::Vector2f>(trajectory);
};

Agent::Agent(){};

void Agent::calculateVelocity(sf::Vector2f waypoint) {
    // Calculate velocity based on heading

    float angle = std::atan2(waypoint.y - position.y, waypoint.x - position.x);
    sf::Vector2f heading;
    heading.x = std::cos(angle);
    heading.y = std::sin(angle);

    velocity = heading * velocityMagnitude;
}

void Agent::updatePosition(float timeStep) {
    position.x += velocity.x * timeStep;
    position.y += velocity.y * timeStep;
}


void Agent::calculateTrajectory(float waypointDistance) {

    trajectory.clear();
    trajectory.push_back(initialPosition);

    sf::Vector2f currentPosition = initialPosition;

    double totalDistance = sqrt(pow((targetPosition.x - initialPosition.x), 2) + pow((targetPosition.y - initialPosition.y), 2));
    int numWaypoints = floor(totalDistance / waypointDistance);
    
    if (numWaypoints < 1) {
        trajectory.push_back(targetPosition);
        return;
    }
    
    double angle = atan2(targetPosition.y - initialPosition.y, targetPosition.x - initialPosition.x); // in radians
    
    double deltaX = waypointDistance * cos(angle);
    double deltaY = waypointDistance * sin(angle);

    for (int i = 0; i < numWaypoints; ++i) {
        currentPosition.x += deltaX;
        currentPosition.y += deltaY;
        trajectory.push_back(currentPosition);
    }

    trajectory.push_back(targetPosition);
}

/*****************************************/
/********** SHARED BUFFER CLASS **********/
/*****************************************/

// SharedBuffer class
class SharedBuffer {
public:
    SharedBuffer() : currentBuffer(0) {}

    void setBufferSize(int size) {
        buffers[0].resize(size);
        buffers[1].resize(size);
    }

    void swapBuffers() {
        std::lock_guard<std::mutex> lock(mutex);
        currentBuffer = 1 - currentBuffer; 
    }

    void writeData(int frameIndex, const std::vector<Agent>& frame) {
        std::lock_guard<std::mutex> lock(mutex);
        buffers[currentBuffer][frameIndex] = frame;
    }

    const std::vector<Agent>& readFrame(int frameIndex) const {
        std::lock_guard<std::mutex> lock(mutex);
        return buffers[1 - currentBuffer][frameIndex]; 
    }

    size_t bufferSize() const {
        std::lock_guard<std::mutex> lock(mutex);
        return buffers[0].size();
    }

private:
    std::vector<std::vector<Agent>> buffers[2];  // Store frames (vectors of Agents)
    int currentBuffer;
    mutable std::mutex mutex;
};

/**************************************/
/********** SIMULATION CLASS **********/
/**************************************/

// Simulation class
class Simulation {
public:
    Simulation(SharedBuffer& buffer, std::atomic<float>& timeStepSim, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config);
    void run();
    void update();
    float getCurrentFrameRate();
    void initializeAgents();
    void loadConfiguration();

private:
    std::vector<Agent> agents;
    SharedBuffer& buffer;
    size_t currentFrame;
    int numAgents;
    std::atomic<int>& currentNumAgents;
    ThreadPool threadPool;
    std::atomic<float>& timeStepSim;
    std::atomic<bool>& stop;
    const YAML::Node& config;

    // Simulation parameters
    float timeStep;
    int maxFrames;  // TODO: Change to size_t
    int numThreads;

    // Window parameters
    int windowWidth;
    int windowHeight;
};

Simulation::Simulation(SharedBuffer& buffer, std::atomic<float>& timeStepSim, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config) 
: buffer(buffer), currentFrame(0), threadPool(std::thread::hardware_concurrency()), timeStepSim(timeStepSim), stop(stop), currentNumAgents(currentNumAgents), config(config) {
   
   loadConfiguration();
   // TODO: set number of threads for thread pool (no lazy initialization) -> initializeThreadPool();
   initializeAgents();
}

void Simulation::loadConfiguration() {

    // Load simulation parameters
    timeStep = config["simulation"]["time_step"].as<float>();
    maxFrames = config["simulation"]["maximum_frames"].as<int>();

    // Load display parameters
    windowHeight = config["display"]["height"].as<int>();
    windowWidth = config["display"]["width"].as<int>();

    // Load number of threads
    if(config["simulation"]["num_threads"].as<int>()) {
        numThreads = config["simulation"]["num_threads"].as<int>();
    } else {
        numThreads = std::thread::hardware_concurrency();
    }

}

void Simulation::initializeAgents() {

    std::vector<std::future<void>> futures;
    int numAgents = 100;
    float waypointDistance = 10.f;

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
        agents.push_back(agent);
    }

    // Calculate the trajectory for each agent in parallel
    for (auto& agent: agents) {
        futures.emplace_back(threadPool.enqueue([this, &agent, waypointDistance] {
            agent.calculateTrajectory(waypointDistance);
            agent.calculateVelocity(agent.trajectory[1]);
        }));
    }
    for (auto& future : futures) {
        future.get();
    }
}

void Simulation::run() {

    sf::Clock clock;
    float totalSimTime = 0.0f;
    timeStepSim = timeStep;
    sf::Clock timeStepSimClock;

    // Main simulation loop
    while (currentFrame < maxFrames && !stop) {

        // Restart the clock
        timeStepSimClock.restart();
        
        // Update the agents
        update();
        
        // Write the data to the buffer
        buffer.writeData(currentFrame, agents);

        // Swap the buffers
        buffer.swapBuffers(); 
        
        // Increment the frame index
        ++currentFrame;

        // Calculate the time step for the simulation
        timeStepSim = timeStepSimClock.getElapsedTime().asSeconds();

        // Calculate the total simulation time
        totalSimTime += timeStepSim;

    }

    // Print simulation statistics
    std::cout << std::endl;
    std::cout << "Total simulation time: " << totalSimTime << " seconds for " << currentFrame << " frames" << std::endl;
    std::cout << "Frame rate: " << 1/(totalSimTime / (currentFrame + 1)) << std::endl;
    std::cout << "Average simulation time step: " << totalSimTime/ (currentFrame + 1) << std::endl;
}

void Simulation::update() {
    std::vector<std::future<void>> futures;
    for (auto& agent : agents) {
        futures.emplace_back(threadPool.enqueue([this, &agent] {
            agent.updatePosition(timeStep);
        }));
    }
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
    Renderer(const SharedBuffer& buffer, std::atomic<float>& timeStepSim, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config);
    void run(); 
    void loadConfiguration();
    void initializeWindow();
    void initializeGUI();
    void handleEvents(sf::Event& event);

    // GUI functions
    void updateFrameCountText();
    void updateFrameRateText();
    void updateAgentCountText();
    void updateTimeText();
    void calculateFrameRate();

private:
    void render();
    const SharedBuffer& buffer;
    sf::RenderWindow window;
    sf::CircleShape agentShape;
    size_t currentFrame = 0;
    std::atomic<float>& timeStepSim;
    std::atomic<bool>& stop;
    bool showTrajectories = true;
    bool showWaypoints = true;
    const YAML::Node& config;

    // Window parameters
    int windowWidth;
    int windowHeight;
    int scale;
    float windowWidthScaled;
    float windowHeightScaled;
    float windowWidthOffsetScaled;
    float windowHeightOffsetScaled;
    sf::Font font;
    std::string title;
    bool showInfo = true;

    // Renderer parameters
    float timeStep;
    float playbackSpeed;
    float previousPlaybackSpeed;
    bool paused = false;

    // GUI Elements
    sf::Text frameText;
    sf::Text frameRateText;
    sf::Text agentCountText;
    sf::Text timeText;
    sf::RectangleShape pauseButton;
    sf::Text pauseButtonText;
    sf::RectangleShape resetButton;
    sf::Text resetButtonText;
    
    // Frame rate calculation
    // std::vector<float> frameRates;
    std::deque<float> frameRates;
    const size_t frameRateBufferSize = 1.0f / timeStep;
    float frameRate;
    float movingAverageFrameRate;
    int frameCount = 0;
    int maxFrames = 0;
    sf::Clock clock;
    float cumulativeSum = 0.0f;
    static const int warmupFrames = 10;

    // Agents
    int currentNumAgents;
};

// Renderer member functions 
Renderer::Renderer(const SharedBuffer& buffer, std::atomic<float>& timeStepSim, std::atomic<bool>& stop, std::atomic<int>& currentNumAgents, const YAML::Node& config) 
    : buffer(buffer), timeStepSim(timeStepSim), currentNumAgents(currentNumAgents), stop(stop), config(config) {
    
    loadConfiguration();
    initializeWindow();
    initializeGUI();
}

void Renderer::loadConfiguration() {
    // Load configuration data -> TODO: Add error handling
    windowWidth = config["display"]["width"].as<int>();
    windowHeight = config["display"]["height"].as<int>();
    scale = config["display"]["pixels_per_meter"].as<int>();
    showInfo = config["display"]["show_info"].as<bool>();
    title = config["display"]["title"].as<std::string>();
    timeStep = config["simulation"]["time_step"].as<float>();
    playbackSpeed = config["simulation"]["playback_speed"].as<float>();
    maxFrames = config["simulation"]["maximum_frames"].as<int>();

    // Load font
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        std::cerr << "Error loading font\n";  // TODO: Add logging
    }

    // Scale the window dimensions
    windowWidthScaled = static_cast<float>(windowWidth) / static_cast<float>(scale);
    windowHeightScaled = static_cast<float>(windowHeight) / static_cast<float>(scale);

    // Centering the render area in the window
    windowWidthOffsetScaled = (windowWidthScaled - windowWidth) / 2.0f;
    windowHeightOffsetScaled = (windowHeightScaled - windowHeight) / 2.0f;
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
    resetButton.setFillColor(sf::Color::Blue);
    resetButton.setPosition(window.getSize().x - 110, window.getSize().y - 120); 

    resetButtonText.setFont(font);
    resetButtonText.setString("Reset");
    resetButtonText.setCharacterSize(20);
    resetButtonText.setFillColor(sf::Color::Black);

    // Center the reset button text
    sf::FloatRect resetButtonTextRect = resetButtonText.getLocalBounds();
    resetButtonText.setOrigin(resetButtonTextRect.left + resetButtonTextRect.width / 2.0f,
                              resetButtonTextRect.top + resetButtonTextRect.height / 2.0f);
    resetButtonText.setPosition(resetButton.getPosition().x + resetButton.getSize().x / 2.0f,
                                resetButton.getPosition().y + resetButton.getSize().y / 2.0f);
}

void Renderer::handleEvents(sf::Event& event) {

    switch (event.type) {
        // Check for window close event
        case sf::Event::Closed:
            window.close();
            stop = true;
            break;

        case sf::Event::MouseButtonPressed:
            if (event.mouseButton.button == sf::Mouse::Left) {
                sf::Vector2i mousePosition(event.mouseButton.x, event.mouseButton.y);

                // Check if the mouse is within the pause button
                if (pauseButton.getGlobalBounds().contains(mousePosition.x, mousePosition.y)) {
                    paused = !paused;
                    pauseButtonText.setString(paused ? "  Play" : "Pause");
                    pauseButton.setFillColor(paused ? sf::Color::Red : sf::Color::Green); 
                }
            }
            break;

        // Check for key press events
        case sf::Event::KeyPressed:
            switch (event.key.code) {
                case sf::Keyboard::Up:
                    playbackSpeed += 1.0f;
                    playbackSpeed = std::floor(playbackSpeed);
                    break;

                case sf::Keyboard::Down:
                    if (playbackSpeed > 1.0f) {
                        playbackSpeed -= 1.0f;
                        playbackSpeed = std::ceil(playbackSpeed);
                    } else if (playbackSpeed <= 1.0f && playbackSpeed > 0.1f) {
                        playbackSpeed -= 0.1f;
                    } else {
                        playbackSpeed = 0.1f;
                    }
                    break;

                case sf::Keyboard::R:
                    currentFrame = 0;
                    paused = false;
                    break;

                case sf::Keyboard::Escape:
                    window.close();
                    break;

                case sf::Keyboard::Space:

                    if (!paused) {
                        paused = true;
                        previousPlaybackSpeed = playbackSpeed;
                        pauseButtonText.setString("  Play");
                        pauseButton.setFillColor(sf::Color::Red);
                    } else {
                        paused = false;
                        playbackSpeed = previousPlaybackSpeed;
                        pauseButton.setFillColor(sf::Color::Green);
                        pauseButtonText.setString("Pause");
                    }
                    break;

                case sf::Keyboard::T:
                    showTrajectories = !showTrajectories; // Toggle trajectories display
                    break;

                case sf::Keyboard::W:
                    showWaypoints = !showWaypoints; // Toggle waypoints display
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
} 

// Function to update the text that displays the current frame count
void Renderer::updateFrameCountText() {

    frameText.setString("Frame " + std::to_string(currentFrame) + "/" + (maxFrames > 0 ? std::to_string(maxFrames) : "∞")); // ∞ for unlimited frames
    sf::FloatRect textRect = frameText.getLocalBounds();
    frameText.setOrigin(textRect.width, 0); // Right-align the text
    frameText.setPosition(window.getSize().x - 10, 40); // Position with padding
}

// Function to update the text that displays the current frame rate
void Renderer::updateFrameRateText() {

    frameRateText.setString("FPS: " + std::to_string(static_cast<int>(frameRate)));
    sf::FloatRect textRect = frameRateText.getLocalBounds();
    frameRateText.setOrigin(textRect.width, 0); // Right-align the text
    frameRateText.setPosition(window.getSize().x - 10, 70); // Position with padding
}

// Function to update the text that displays the current agent count
void Renderer::updateAgentCountText() {

    agentCountText.setString("Agents: " + std::to_string(currentNumAgents));
    sf::FloatRect textRect = agentCountText.getLocalBounds();
    agentCountText.setOrigin(textRect.width, 0); // Right-align the text
    agentCountText.setPosition(window.getSize().x - 10, 100); // Position with padding
}

// Calculate the current frame rate
void Renderer::calculateFrameRate() {

    if (currentFrame > warmupFrames) {
        float frameTime = clock.restart().asSeconds();
        frameRate = 1.0f / frameTime;

        if (frameRates.size() == frameRateBufferSize) {
            cumulativeSum -= frameRates.front();
            frameRates.pop_front();
        }

        frameRates.push_back(frameRate);
        cumulativeSum += frameRate;

        if (frameRates.size() >= 1.0f / timeStep) { // Ensure you have enough samples for the moving average
            movingAverageFrameRate = cumulativeSum / frameRates.size();
            updateFrameRateText();
        }
    } else {
        clock.restart(); // Discard the timing of the warmup frames
    }
}

void Renderer::run() {

    using namespace std::chrono;
    const float targetframeRate = 1.0f / timeStep;  // Calculate original frame rate
    float frameRateSim;
    duration<float> frameTimeSim; // Adjusted frame time
    duration<float> targetFrameTime = duration<float>(1.0f / (targetframeRate * playbackSpeed)); // Adjusted frame time
    sf::Event event;
    while (window.isOpen() && currentFrame < buffer.bufferSize() - 1) {

        while (window.pollEvent(event)) {
            handleEvents(event);
        }

        if(!paused) {

            // Update the text elements
            updateAgentCountText();
            updateFrameCountText();

            // Frame rate calculation
            calculateFrameRate();

            // Render the simulation
            render();

            // Get the frame time and frame rate from the simulation
            frameTimeSim = duration<float>(timeStepSim);
            frameRateSim = 1.0f / timeStepSim;

            // Calculate the target frame time
            targetFrameTime = duration<float>(timeStep / playbackSpeed);

            // Check if the target frame time is doable
            if(targetFrameTime.count() >= frameTimeSim.count()) {
                std::this_thread::sleep_for(duration<float>(targetFrameTime));
            } else {
                std::this_thread::sleep_for(duration<float>(frameTimeSim));
                playbackSpeed = timeStep / timeStepSim;
            }
            currentFrame = (currentFrame + 1) % buffer.bufferSize();
        }
    }
}

void Renderer::render() {
    window.clear(sf::Color::White);

    const std::vector<Agent>& frame = buffer.readFrame(currentFrame);

    // if (!frame.empty()) {
        for (const auto& agent : frame) {

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
        }

        // Draw buttons
        window.draw(pauseButton);
        window.draw(pauseButtonText);
        window.draw(resetButton);
        window.draw(resetButtonText);

        window.display();
    // }
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
        std::cerr << "Error loading config file: " << e.what() << std::endl;
        return 1;
    }

    SharedBuffer sharedBuffer;

    // Load global configuration data
    float timeStep = config["simulation"]["time_step"].as<float>();
    int maxFrames = config["simulation"]["maximum_frames"].as<int>();
    int numAgents = config["agents"]["num_agents"].as<int>();

    // Shared variabes
    std::atomic<float> timeStepSim{timeStep};
    std::atomic<bool> stop{false};
    std::atomic<int> currentNumAgents{numAgents};

    // Set the buffer size appropriately 
    sharedBuffer.setBufferSize(maxFrames);
    
    Simulation simulation(sharedBuffer, timeStepSim, stop, currentNumAgents, config); 
    Renderer renderer(sharedBuffer, timeStepSim, stop, currentNumAgents, config);

    std::thread simulationThread(&Simulation::run, &simulation); 

    renderer.run();

    simulationThread.join();
    return 0;
}
