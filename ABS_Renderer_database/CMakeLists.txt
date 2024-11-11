cmake_minimum_required(VERSION 3.10)  

# Set the project name
project(Visualizer)            

# Find SFML libraries
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Find the libraries for yaml-cpp and MongoDB C++ driver
find_package(yaml-cpp REQUIRED)
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

# Add the include directories for the libraries
include_directories(include)

# Add the source files to the executable
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create the executable
add_executable(Visualizer ${SOURCES})

# Link libraries to the executable
target_link_libraries(Visualizer sfml-graphics sfml-window sfml-system yaml-cpp::yaml-cpp mongo::mongocxx_shared mongo::bsoncxx_shared)

# Additional CMake options for the executable
target_compile_features(Visualizer PRIVATE cxx_std_17)

# Set the optimization level
target_compile_options(Visualizer PRIVATE -O3)

# Define DEBUG macro when building in debug mode
# target_compile_definitions(Visualizer PRIVATE DEBUG)
target_compile_definitions(Visualizer PRIVATE STATS)
target_compile_definitions(Visualizer PRIVATE ERROR)
# target_compile_definitions(Visualizer PRIVATE TIMING)