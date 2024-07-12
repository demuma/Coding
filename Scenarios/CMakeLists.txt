cmake_minimum_required(VERSION 3.10)  

project(Simulation)            

# Find SFML libraries
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Find the YAML-CPP library
find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIR})

# Add source files to build executable
add_executable(RoadUserSimulation
    Agent.cpp
    Simulation.cpp
    CollisionAvoidance.cpp
    Obstacle.cpp
    Grid.cpp
    Main.cpp
)

# Link SFML and yaml-cpp libraries to the executable
target_link_libraries(RoadUserSimulation sfml-graphics sfml-window sfml-system yaml-cpp::yaml-cpp)

# Additional CMake options for the executable
target_compile_features(RoadUserSimulation PRIVATE cxx_std_17)
