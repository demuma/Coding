#include <SFML/Graphics.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <yaml-cpp/yaml.h>
#include <iostream>

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
    Simulation(SharedBuffer& buffer, float timeStep);
    void run(int maxFrames);
    void update();

private:
    YAML::Node config;
    std::vector<Agent> agents;
    SharedBuffer& buffer;
    float timeStep;
    int frameIndex;
};

Simulation::Simulation(SharedBuffer& buffer, float timeStep) : buffer(buffer), timeStep(timeStep), frameIndex(0) {
    // Initialize two agents 
    agents.push_back(Agent({0.f, 350.f}, {10.f, 0.f}, {100.f, 100.f})); 
    agents.push_back(Agent({0.f, 470.f}, {8.f, 0.f}, {80.f, 80.f}));  
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
    // Update all agents
    for (auto& agent : agents) {
        agent.rect.left += agent.velocity.x * timeStep; 
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

    // Set the buffer size appropriately 
    sharedBuffer.setBufferSize(maxFrames); // Set buffer size at the beginning
    
    Simulation simulation(sharedBuffer, timeStep); 
    Renderer renderer(sharedBuffer, sf::VideoMode(800, 600));

    std::thread simulationThread(&Simulation::run, &simulation, maxFrames); 

    renderer.run(timeStep, playbackSpeed);

    simulationThread.join();
    return 0;
}
