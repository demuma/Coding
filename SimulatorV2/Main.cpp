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

// Agent class
class Agent {
public:
    Agent(const sf::Vector2f& position, const sf::Vector2f& velocity, const sf::Vector2f& size) 
        : rect({position, size}), velocity(velocity) {}

    sf::FloatRect getRect() const {
        return rect;
    }
    sf::FloatRect rect;
    sf::Vector2f velocity;
};

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

    void writeData(int frameIndex, const std::vector<sf::FloatRect>& frame) {
        std::lock_guard<std::mutex> lock(mutex);
        buffers[currentBuffer][frameIndex] = frame;
    }

    const std::vector<sf::FloatRect>& readFrame(int frameIndex) const {
        std::lock_guard<std::mutex> lock(mutex);
        return buffers[1 - currentBuffer][frameIndex]; 
    }

    size_t bufferSize() const {
        std::lock_guard<std::mutex> lock(mutex);
        return buffers[0].size();
    }

private:
    std::vector<std::vector<sf::FloatRect>> buffers[2];  // Store frames (vectors of FloatRects)
    int currentBuffer;
    mutable std::mutex mutex;
};

// Simulation class
class Simulation {
public:
    Simulation(SharedBuffer& buffer, float timeStep, size_t numThreads, std::atomic<float>& timeStepSim, std::atomic<bool>& stop);
    void run(int maxFrames);
    void update();
    float getCurrentFrameRate();

private:
    YAML::Node config;
    std::vector<Agent> agents;
    SharedBuffer& buffer;
    float timeStep;
    int frameIndex;
    int numAgents;
    ThreadPool threadPool;
    std::atomic<float>& timeStepSim;
    std::atomic<bool>& stop;
};

Simulation::Simulation(SharedBuffer& buffer, float timeStep, size_t numThreads, std::atomic<float>& timeStepSim, std::atomic<bool>& stop) : buffer(buffer), timeStep(timeStep), frameIndex(0), threadPool(numThreads), timeStepSim(timeStepSim), stop(stop) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 600);
    long number;

    // Initialize two agents 
    for (int i = 0; i < 1000; ++i) {
        number = dis(gen);
        agents.push_back(Agent({0.f, static_cast<float>(number)}, {3.f, 0.f}, {5.f, 5.f}));
    }
}

void Simulation::run(int maxFrames) {

    sf::Clock clock;
    float totalSimTime = 0.0f;
    // sf::Time timeStepSim;
    timeStepSim = timeStep;
    sf::Clock timeStepSimClock;

    while (frameIndex < maxFrames && !stop) {

        timeStepSimClock.restart();
        
        update();
        
        // Write the current frame to the buffer
        std::vector<sf::FloatRect> agentRects;
        for(const auto& agent : agents) {
            agentRects.push_back(agent.getRect());
        }
        buffer.writeData(frameIndex, agentRects);

        buffer.swapBuffers(); 
        
        ++frameIndex;

        timeStepSim = timeStepSimClock.getElapsedTime().asSeconds();
        // std::cout << '\r' << "Current simulation time step: " << timeStepSim << std::flush;

        totalSimTime += timeStepSim;

    }
    std::cout << std::endl;
    std::cout << "Total simulation time: " << totalSimTime << " seconds for " << frameIndex + 1 << " frames" << std::endl;
    std::cout << "Frame rate: " << 1/(totalSimTime / (frameIndex + 1)) << std::endl;
    std::cout << "Average simulation time step: " << totalSimTime/ (frameIndex + 1) << std::endl;
}

void Simulation::update() {
    std::vector<std::future<void>> futures;
    for (auto& agent : agents) {
        futures.emplace_back(threadPool.enqueue([this, &agent] {
            agent.rect.left += agent.velocity.x * timeStep;
            agent.rect.top += agent.velocity.y * timeStep;
        }));
    }
    for (auto& future : futures) {
        future.get();
    }
}

// Renderer class
class Renderer {
public:
    Renderer(const SharedBuffer& buffer, const sf::VideoMode& mode, std::atomic<float>& timeStepSim, std::atomic<bool>& stop);
    void run(float timeStep, float playbackSpeed); 
    void setWindowSize(int width, int height);

private:
    void render();
    const SharedBuffer& buffer;
    sf::RenderWindow window;
    sf::RectangleShape agentShape;
    size_t currentFrame = 0;
    std::atomic<float>& timeStepSim;
    std::atomic<bool>& stop;
};

// Renderer member functions 
Renderer::Renderer(const SharedBuffer& buffer, const sf::VideoMode& mode, std::atomic<float>& timeStepSim, std::atomic<bool>& stop) 
    : buffer(buffer), window(mode, "Agent Simulation"), timeStepSim(timeStepSim), stop(stop) {
    agentShape.setFillColor(sf::Color::Red);
}

void Renderer::setWindowSize(int width, int height){
    window.create(sf::VideoMode(width, height), "Urban Mobility Simulator");
}

void Renderer::run(float timeStep, float playbackSpeed) {

    using namespace std::chrono;
    const float targetframeRate = 1.0f / timeStep;  // Calculate original frame rate
    float frameRateSim;
    duration<float> frameTimeSim; // Adjusted frame time
    duration<float> targetFrameTime = duration<float>(1.0f / (targetframeRate * playbackSpeed)); // Adjusted frame time

    while (window.isOpen() && currentFrame < buffer.bufferSize() - 1) {

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                stop = true;
            } else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Up) {
                playbackSpeed += 1.0f;
                playbackSpeed = std::ceil(playbackSpeed);
            } else if(event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Down) {
                if(playbackSpeed > 1.0f) {
                    playbackSpeed -= 1.0f;
                    playbackSpeed = std::ceil(playbackSpeed);
                } else if (playbackSpeed <= 1.0f && playbackSpeed > 0.1f) {
                    playbackSpeed -= 0.1f;
                } else {
                    playbackSpeed = 0.1f;
                }
            }
        }
        
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
            std::cout << "Playback speed: " << playbackSpeed << std::endl;
        }
        currentFrame = (currentFrame + 1) % buffer.bufferSize();
    }
}

void Renderer::render() {
    window.clear(sf::Color::Black);

    const std::vector<sf::FloatRect>& frame = buffer.readFrame(currentFrame);

    if(!frame.empty()) {

        for (const auto& agentRect : frame) {
            agentShape.setPosition(agentRect.left, agentRect.top);
            agentShape.setSize({agentRect.width, agentRect.height});
            window.draw(agentShape);
        }

        window.display();
    }
}

// Main function 
int main() {
    SharedBuffer sharedBuffer;
    
    YAML::Node config = YAML::LoadFile("config.yaml");
    float timeStep = config["time_step"].as<float>();
    int maxFrames = config["max_frames"].as<int>();
    float playbackSpeed = config["playback_speed"].as<float>();
    size_t numThreads = std::thread::hardware_concurrency();
    std::atomic<float> timeStepSim{timeStep};
    std::atomic<bool> stop{false};

    // Set the buffer size appropriately 
    sharedBuffer.setBufferSize(maxFrames); // Set buffer size at the beginning
    
    Simulation simulation(sharedBuffer, timeStep, numThreads, timeStepSim, stop); 
    Renderer renderer(sharedBuffer, sf::VideoMode(800, 600), timeStepSim, stop);

    std::thread simulationThread(&Simulation::run, &simulation, maxFrames); 

    renderer.run(timeStep, playbackSpeed);

    simulationThread.join();
    return 0;
}
