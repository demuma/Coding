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
    Simulation(SharedBuffer& buffer, float timeStep, size_t numThreads);
    void run(int maxFrames);
    void update();

private:
    YAML::Node config;
    std::vector<Agent> agents;
    SharedBuffer& buffer;
    float timeStep;
    int frameIndex;
    int numAgents;
    ThreadPool threadPool;
};

Simulation::Simulation(SharedBuffer& buffer, float timeStep, size_t numThreads) : buffer(buffer), timeStep(timeStep), frameIndex(0), threadPool(numThreads) {
    // Initialize two agents 
    agents.push_back(Agent({0.f, 350.f}, {10.f, 0.f}, {10.f, 10.f})); 
    agents.push_back(Agent({0.f, 470.f}, {8.f, 0.f}, {8.f, 8.f}));
    agents.push_back(Agent({0.f, 270.f}, {9.f, 0.f}, {4.f, 4.f}));
    agents.push_back(Agent({0.f, 170.f}, {12.f, 0.f}, {4.f, 4.f}));
    agents.push_back(Agent({0.f, 210.f}, {11.f, 0.f}, {4.f, 4.f}));
    agents.push_back(Agent({0.f, 420.f}, {5.f, 0.f}, {7.f, 7.f}));
    agents.push_back(Agent({0.f, 550.f}, {7.f, 0.f}, {8.f, 8.f}));
}

void Simulation::run(int maxFrames) {
    while (frameIndex < maxFrames) {
        update();
        
        // Write the current frame to the buffer
        std::vector<sf::FloatRect> agentRects;
        for(const auto& agent : agents) {
            agentRects.push_back(agent.getRect());
        }
        buffer.writeData(frameIndex, agentRects);

        buffer.swapBuffers(); 
        
        ++frameIndex;
    }
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
    Renderer(const SharedBuffer& buffer, const sf::VideoMode& mode);
    void run(float timeStep, float playbackSpeed); 
    void setWindowSize(int width, int height);

private:
    void render();
    const SharedBuffer& buffer;
    sf::RenderWindow window;
    sf::RectangleShape agentShape;
    size_t currentFrame = 0;
};

// Renderer member functions 
Renderer::Renderer(const SharedBuffer& buffer, const sf::VideoMode& mode) 
    : buffer(buffer), window(mode, "Agent Simulation") {
    agentShape.setFillColor(sf::Color::Red);
}

void Renderer::setWindowSize(int width, int height){
    window.create(sf::VideoMode(width, height), "Urban Mobility Simulator");
}

void Renderer::run(float timeStep, float playbackSpeed) {
    using namespace std::chrono;
    const float frameRate = 1.0f / timeStep;  // Calculate original frame rate
    const duration<float> frameTime(1.0f / (frameRate * playbackSpeed)); // Adjusted frame time
    auto nextFrameTime = steady_clock::now(); 

    while (window.isOpen() && currentFrame < buffer.bufferSize() - 1) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        render();
        std::this_thread::sleep_until(nextFrameTime);
        nextFrameTime += duration_cast<steady_clock::duration>(frameTime); 
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

    // Set the buffer size appropriately 
    sharedBuffer.setBufferSize(maxFrames); // Set buffer size at the beginning
    
    Simulation simulation(sharedBuffer, timeStep, numThreads); 
    Renderer renderer(sharedBuffer, sf::VideoMode(800, 600));

    std::thread simulationThread(&Simulation::run, &simulation, maxFrames); 

    renderer.run(timeStep, playbackSpeed);

    simulationThread.join();
    return 0;
}
