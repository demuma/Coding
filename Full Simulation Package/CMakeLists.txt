cmake_minimum_required(VERSION 3.10)  

# Set the project name
project(RoadUserSimulation)            

# Find SFML libraries
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Find the libraries for yaml-cpp and MongoDB C++ driver
find_package(yaml-cpp REQUIRED)
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

# Add source files to build executable
add_executable(RoadUserSimulation
    Agent.cpp
    Simulation.cpp
    CollisionAvoidance.cpp
    Obstacle.cpp
    Grid.cpp
    Main.cpp
    PerlinNoise.cpp
)

# Link libraries to the executable
target_link_libraries(RoadUserSimulation sfml-graphics sfml-window sfml-system yaml-cpp::yaml-cpp mongo::mongocxx_shared mongo::bsoncxx_shared)

# Additional CMake options for the executable
target_compile_features(RoadUserSimulation PRIVATE cxx_std_17)
