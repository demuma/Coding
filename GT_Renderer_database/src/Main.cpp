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
#include <type_traits>
#include <random>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <iomanip>
#include <sstream>

#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/types/value.hpp>
#include <mongocxx/exception/exception.hpp>
#include <SFML/Graphics.hpp>

#include "../include/Agent.hpp"
#include "../include/Logging.hpp"
#include "../include/Utilities.hpp"
#include "../include/Visualizer.hpp"

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;


class Renderer {
public:
    // Structure to hold agent attributes
    struct AgentAttributes {    
        double probability;
        int priority;
        double radius;
        std::string color;
        struct Velocity {
            double min;
            double max;
            double mu;
            double sigma;
            double noise_scale;
            double noise_factor;
        } velocity;
        struct Acceleration {
            double min;
            double max;
        } acceleration;
        double look_ahead_time;
    };
    Renderer();
    ~Renderer();
    void run();
    void render();
    void update();
    void getMetadata();
    void getData();
    void loadConfiguration();
    void initializeDatabase();
    void initializeWindow();
    void getTimeStep();
    void testWriteDataBase();
    void testReadDataBase();
    void loadAgentsAttributes();
    mongocxx::v_noabi::cursor findDataWithTimestamp(std::string timestamp);
    Agent createAgentFromDocument(bsoncxx::document::view document);
    YAML::Node config;
    sf::Font font;
    float scale = 10; // How determine?
    int windowWidth;
    int windowHeight;

    // Database
    std::string dbUri;
    // mongocxx::client client;
    std::shared_ptr<mongocxx::client> client;
    mongocxx::database db;
    mongocxx::instance instance;
    mongocxx::collection collection;
    std::string collectionName;
    std::string databaseName;

    // Query
    int batchSize = 100;

    // Agent attributes
    std::map<std::string, Agent::AgentTypeAttributes> agentTypeAttributes;

    // Window
    sf::RenderWindow window;

    // Simulation data
    std::vector<std::vector<Agent>> simulationData;
    sf::Time timeStep;
    int maxFrames;
    int currentFrameIndex = 0;
};

Renderer::~Renderer() {
    simulationData.clear();
}

std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp) {
    std::istringstream ss(timestamp);
    std::tm tm = {};
    char delim;
    int milliseconds;
    
    // Parse the time part
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    // Parse the milliseconds
    ss >> delim >> milliseconds;
    
    // Convert to time_point
    auto time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    // Add milliseconds
    time += std::chrono::milliseconds(milliseconds);
    
    return time;
}

void Renderer::getTimeStep() {
    // timeStep = sf::seconds(1.0f / config["simulation"]["time_step"].as<float>());
    auto time1 = parseTimestamp(simulationData[0][0].timestamp);
    auto time2 = parseTimestamp(simulationData[1][0].timestamp);

    timeStep = sf::seconds(std::chrono::duration_cast<std::chrono::duration<float>>(time2 - time1).count());    

    DEBUG_MSG("Time step: " << timeStep.asSeconds());

}

void Renderer::initializeWindow() {
    window.create(sf::VideoMode(windowWidth, windowHeight), "Agent-based Data Visualizer");
}

void Renderer::loadConfiguration() {
    try {
        config = YAML::LoadFile("config.yaml");
    } catch (const YAML::Exception& e) {
        ERROR_MSG("Error loading config file: " << e.what());
        exit(1);
    }

    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        std::cerr << "Error loading font\n";
        return;
    }

    windowWidth = config["display"]["width"].as<int>();
    windowHeight = config["display"]["height"].as<int>();

    // Database
    std::string dbHost = config["database"]["host"].as<std::string>();
    int dbPort = config["database"]["port"].as<int>();
    databaseName = config["database"]["db_name"].as<std::string>();
    collectionName = config["database"]["collection_name"].as<std::string>();
    dbUri = "mongodb://" + dbHost + ":" + std::to_string(dbPort);

    // Parse agent attributes from config file
    loadAgentsAttributes();
}

void Renderer::loadAgentsAttributes() {

    // Extract agent data
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

void Renderer::render(){

    window.clear(sf::Color::White);

    // Draw agents
    for (auto& agent : simulationData[currentFrameIndex]) {
        sf::CircleShape body(agent.bodyRadius * scale);
        body.setFillColor(agent.color);
        body.setOrigin(body.getRadius(), body.getRadius());
        body.setPosition(agent.position.x * scale, agent.position.y * scale);
        window.draw(body);

        sf::CircleShape buffer(agent.bufferZoneRadius * scale);
        buffer.setOrigin(buffer.getRadius(), buffer.getRadius());
        buffer.setFillColor(sf::Color::Transparent);
        buffer.setOutlineThickness(2.f);
        buffer.setOutlineColor(agent.bufferZoneColor);
        buffer.setPosition(agent.position.x * scale, agent.position.y * scale);
        window.draw(buffer);

        // Draw the arrow (a triangle) representing the velocity vector (check)
        sf::Vector2f direction = agent.velocity;  // Direction of the arrow from velocity vector
        float arrowLength = agent.bodyRadius * scale * 0.5f;
        float arrowAngle = std::atan2(direction.y, direction.x);

        // Normalize the direction vector for consistent arrow length (check)
        sf::Vector2f normalizedDirection = direction / std::sqrt(direction.x * direction.x + direction.y * direction.y);

        // Arrow shape factor (check)
        float arrowHeadLength = 0.4 * scale;
        float arrowHeadWidth = 0.25 * scale;
        int lineThickness = 3;

        // Arrowhead (triangle) - Offset the tip by the agent's bodyRadius (check)
        sf::ConvexShape arrow(3);
        arrow.setPoint(0, sf::Vector2f(0, 0)); // Tip of the arrow
        arrow.setPoint(1, sf::Vector2f(-arrowHeadLength, arrowHeadWidth / 2));
        arrow.setPoint(2, sf::Vector2f(-arrowHeadLength , -arrowHeadWidth / 2));
        arrow.setFillColor(sf::Color::Black);

        // Line (arrow body) - Offset the start by the agent's bodyRadius (check)
        sf::Vertex line[] =
        {
            sf::Vertex((agent.position * scale + normalizedDirection * agent.bodyRadius * scale), sf::Color::Black),  // Offset start point
            sf::Vertex((agent.position * scale + normalizedDirection * agent.bodyRadius * scale + direction * (arrowLength / 2.0f)), sf::Color::Black) // Ending point (base of arrowhead)
        };

        // Set the origin of the arrowhead to the base of the triangle (check)
        arrow.setOrigin(-arrowHeadLength, 0); 
        arrow.setPosition(line[1].position); 
        arrow.setRotation(arrowAngle * 180.0f / M_PI); // Convert radians to degrees for SFML
        window.draw(arrow);
        window.draw(line, 2, sf::Lines);

    }

    window.display();
}

void Renderer::update() {

    // Update agent buffer radius
    for (auto& agent : simulationData[currentFrameIndex]) {
        float bufferAdaption = (std::sqrt(agent.velocity.x * agent.velocity.x + agent.velocity.y * agent.velocity.y) / agentTypeAttributes.at(agent.type).velocity.max);
        if(bufferAdaption > agent.minBufferZoneRadius) {

            agent.bufferZoneRadius = agent.bodyRadius + (std::sqrt(agent.velocity.x * agent.velocity.x + agent.velocity.y * agent.velocity.y) / agentTypeAttributes.at(agent.type).velocity.max);
        }
        else {
            agent.bufferZoneRadius = agent.minBufferZoneRadius + agent.bodyRadius;
        }
    }
}

mongocxx::v_noabi::cursor Renderer::findDataWithTimestamp(std::string timestamp) {

    // Setup connection to database
    auto collection = (*client)[databaseName][collectionName]; // replace "yourDatabaseName" with the actual database name

    // Find distinct counts
    auto cursor = collection.find(make_document(kvp("timestamp", timestamp)));
    
    // Iterate over the documents
    for (auto&& doc : cursor) {
        std::cout << bsoncxx::to_json(doc) << std::endl;
    }

    return cursor;
}

// Get data from the database and store it in the simulation data sorted by timestamp
void Renderer::getData() {

    // Setup connection to database
    auto collection = (*client)[databaseName]["AB_Sensor_Data"];

    // Find all unique timestamps
    std::vector<std::string> uniqueTimestamps;
    auto cursor3 = collection.distinct("timestamp", {});

    // Iterate over the documents
    for (auto&& doc : cursor3) {

        DEBUG_MSG(bsoncxx::to_json(doc));

        // Get the values array
        bsoncxx::document::element values = doc["values"];

        // Iterate over the values array
        for (auto&& value : values.get_array().value) {
            DEBUG_MSG("Unique timestamp: " << value.get_string().value.to_string());

            // Add the timestamp to the vector of unique timestamps
            uniqueTimestamps.push_back(value.get_string().value.to_string());
        }
    }

    // Print the number of unique timestamps 
    DEBUG_MSG("Number of unique timestamps: " << uniqueTimestamps.size());

    // Reserve space for the simulation data
    simulationData.reserve(uniqueTimestamps.size());

    // Find all agents that have the same timestamp
    auto cursor4 = collection.find(make_document(kvp("timestamp", uniqueTimestamps[0])));

    // Iterate over the documents
    // std::vector<Agent> agents;
    // for (auto&& doc : cursor4) {

    //     // Extract agent data from the document
    //     Agent agent = createAgentFromDocument(doc);
    //     agent.color = stringToColor(agentTypeAttributes.find(agent.type)->second.color);
    //     agents.push_back(agent);
    // }

    // Store the agent frames in the simulation data
    for(auto timestampes : uniqueTimestamps) {
        auto cursor5 = collection.find(make_document(kvp("timestamp", timestampes)));
        std::vector<Agent> agents;
        for (auto&& doc : cursor5) {
            Agent agent = createAgentFromDocument(doc);
            agent.color = stringToColor(agentTypeAttributes.at(agent.type).color);
            agent.bodyRadius = agentTypeAttributes.at(agent.type).bodyRadius;
            agent.bufferZoneRadius = agent.minBufferZoneRadius + agent.bodyRadius;
            agents.push_back(agent);
        }
        simulationData.push_back(agents);
    }

    // Set the maximum number of frames
    maxFrames = simulationData.size();

    // // Filter unique agent ids that are of type "Adult Pedestrian"
    // auto cursor3 = collection.distinct("agent_id",make_document(kvp("type", "Adult Pedestrian")));

    // // Iterate over the documents
    // for (auto&& doc : cursor3) {
    //     std::cout << bsoncxx::to_json(doc) << std::endl;
    //     // std::cout << "Agent ID: " << doc["agent_id"].get_string().value.to_string() << std::endl;
    // }

    // // Find distinct counts
    // auto cursor2 = collection.distinct("type", {});

    // // Iterate over the type of agents
    // for (auto&& doc : cursor2) {
    //     std::cout << bsoncxx::to_json(doc) << std::endl;
    //     bsoncxx::document::element values = doc["values"];
    //     for (auto&& value : values.get_array().value) {
    //         std::cout << "Type: " << value.get_string().value.to_string() << std::endl;
    //     }
    // }
}

void Renderer::initializeDatabase() {

    mongocxx::uri uri(dbUri);
    client = std::make_shared<mongocxx::client>(mongocxx::uri(dbUri));
}

void Renderer::getMetadata() {

    auto collection = (*client)[databaseName]["AB_Sensor_Data"]; 

    // 1. Read Metadata Entry
    auto metadataCursor = collection.find_one(make_document(kvp("data_type", "metadata")));
    if (!metadataCursor) {
        std::cerr << "Error: Metadata not found." << std::endl;
        return; 
    }

    std::cout << bsoncxx::to_json(*metadataCursor) << std::endl;

    // Extract metadata (adjust based on your actual metadata structure)
    bsoncxx::document::view document = metadataCursor->view();
    float detectionWidth = document["detection_area"]["width"].get_double().value;
    float detectionHeight = document["detection_area"]["height"].get_double().value;
    float frameRate = document["frame_rate"].get_double().value;
    sf::Vector2f position = {static_cast<float>(document["position"]["x"].get_double().value), static_cast<float>(document["position"]["y"].get_double().value)};

    STATS_MSG("Detection width: " << detectionWidth);
    STATS_MSG("Detection height: " << detectionHeight);
    STATS_MSG("Frame rate: " << frameRate);
    STATS_MSG("Position: " << position.x << ", " << position.y);
    exit(EXIT_SUCCESS);
}



// // Parse BSON document and create Agent object (ground truth)
// Agent Renderer::createAgentFromDocument(bsoncxx::document::view document) {

//     Agent agent;

//     agent.agent_id = document["agent_id"].get_string().value.to_string();
//     agent.type = document["type"].get_string().value.to_string();
//     agent.position.x = document["position"].get_array().value[0].get_double();
//     agent.position.y = document["position"].get_array().value[1].get_double();
//     agent.timestamp = document["timestamp"].get_string().value.to_string();
//     agent.velocity.x = document["velocity"].get_array().value[0].get_double();
//     agent.velocity.y = document["velocity"].get_array().value[1].get_double();

//     return agent;
// }

// Parse BSON document and create Agent object (agent-based sensor)
Agent Renderer::createAgentFromDocument(bsoncxx::document::view document) {

    Agent agent(agentTypeAttributes["Adult Cyclist"]);

    agent.uuid = document["agent_id"].get_string().value.to_string();
    agent.sensorID = document["sensor_id"].get_string().value.to_string();
    agent.type = document["type"].get_string().value.to_string();
    agent.position.x = document["position"].get_array().value[0].get_double();
    agent.position.y = document["position"].get_array().value[1].get_double();
    agent.timestamp = document["timestamp"].get_string().value.to_string();
    agent.velocity.x = document["estimated_velocity"].get_array().value[0].get_double();
    agent.velocity.y = document["estimated_velocity"].get_array().value[1].get_double();

    return agent;
}

void Renderer::run() {

    // Event handling
    sf::Event event;
    sf::Time renderTotalFrameTime = sf::Time::Zero;
    sf::Time renderTime = sf::Time::Zero;
    sf::Time renderFrameTime = sf::Time::Zero;
    sf::Time eventHandlingTime = sf::Time::Zero;
    sf::Clock renderClock;

    while (window.isOpen() && currentFrameIndex < maxFrames) {
        
        renderClock.restart();

        while (window.pollEvent(event)) {
        }

        eventHandlingTime = renderClock.getElapsedTime();
        
        update();
        render();

        renderFrameTime = renderClock.getElapsedTime();

        // Sleep -> TODO: Add compensation for processing time
        std::this_thread::sleep_for(std::chrono::milliseconds(timeStep.asMilliseconds()));
       
        renderTotalFrameTime = renderClock.getElapsedTime();
        renderTime += timeStep;
        ++currentFrameIndex;
    }
}


Renderer::Renderer() {

    loadConfiguration();
    initializeDatabase();
    getMetadata();
    // initializeWindow();
    // getData();
    // getTimeStep();
}



/**************************/
/********** MAIN **********/
/**************************/

// Main function
int main() {

    // Renderer renderer;
    Visualizer visualizer;

    // renderer.testReadDataBase();
    // renderer.testWriteDataBase();

    visualizer.run();

    // renderer.run();

    return 0;
}