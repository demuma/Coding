#include <SFML/Graphics.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <yaml-cpp/yaml.h>

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

    void swapBuffers() {
        std::lock_guard<std::mutex> lock(mutex);
        currentBuffer = 1 - currentBuffer; // Toggle between 0 and 1
    }

    void writeData(const std::vector<sf::FloatRect>& agentRects) {
        std::lock_guard<std::mutex> lock(mutex);
        buffers[currentBuffer] = agentRects;
    }

    const std::vector<sf::FloatRect>& readData() const {
        std::lock_guard<std::mutex> lock(mutex);
        return buffers[1 - currentBuffer]; // Read from the other buffer
    }

private:
    std::vector<sf::FloatRect> buffers[2];
    int currentBuffer;
    mutable std::mutex mutex;
};


// Simulation class
class Simulation {
public:
    Simulation(SharedBuffer& buffer, float timeStep);
    void run();
    void update();

private:
    YAML::Node config;
    std::vector<Agent> agents;
    SharedBuffer& buffer;
    float timeStep;
};

Simulation::Simulation(SharedBuffer& buffer, float timeStep) : buffer(buffer), timeStep(timeStep) {
    // Initialize two agents 
    agents.push_back(Agent({0.f, 350.f}, {10.f, 0.f}, {100.f, 100.f})); 
    agents.push_back(Agent({0.f, 470.f}, {8.f, 0.f}, {80.f, 80.f}));  
}

void Simulation::run() {
    while (true) { 
        update(); 
        
        // Write all agent rectangles to the buffer
        std::vector<sf::FloatRect> agentRects;
        for(const auto& agent : agents) {
            agentRects.push_back(agent.getRect());
        }
        buffer.writeData(agentRects);
        buffer.swapBuffers(); 

        std::this_thread::sleep_for(std::chrono::duration<float>(timeStep));
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
    void run();
    void setWindowSize(int width, int height);

private:
    void render();
    const SharedBuffer& buffer;
    sf::RenderWindow window;
    sf::RectangleShape agentShape;
};

Renderer::Renderer(const SharedBuffer& buffer, const sf::VideoMode& mode) 
    : buffer(buffer), window(mode, "Agent Simulation") {
    agentShape.setFillColor(sf::Color::Red);
}

void Renderer::setWindowSize(int width, int height){
    window.create(sf::VideoMode(width, height), "Urban Mobility Simulator");
}

void Renderer::run() {
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        render();
    }
}

void Renderer::render() {
    window.clear(sf::Color::Black);

    // Read from the buffer
    const std::vector<sf::FloatRect>& agentRects = buffer.readData();

    // Draw all agent rectangles from the buffer
    for (const auto& agentRect : agentRects) {
        agentShape.setPosition(agentRect.left, agentRect.top);
        agentShape.setSize({agentRect.width, agentRect.height});
        window.draw(agentShape); 
    }

    window.display();
}

int main() {
    SharedBuffer sharedBuffer;
    
    YAML::Node config = YAML::LoadFile("config.yaml");
    float timeStep = config["timestep"].as<float>();

    Simulation simulation(sharedBuffer, timeStep); 
    Renderer renderer(sharedBuffer, sf::VideoMode(800, 600));

    std::thread simulationThread(&Simulation::run, &simulation); // Simulation thread

    renderer.run(); // Renderer thread
    
    simulationThread.join();
    return 0;
}

