# Explicit CMakeLists.txt file for MongoDB C++ driver

# cmake_minimum_required(VERSION 3.10)
# project(mongodb_test)

# # Add CMake module path
# list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake/mongocxx-3.10.1" "/usr/local/lib/cmake/bsoncxx-3.10.1")

# # Set CMake prefix path
# set(CMAKE_PREFIX_PATH "/usr/local/lib/cmake/mongocxx-3.10.1" "/usr/local/lib/cmake/bsoncxx-3.10.1")

# # Set CMake library path
# link_directories("/usr/local/lib")

# # Find the MongoDB C++ driver
# find_package(mongocxx REQUIRED)
# find_package(bsoncxx REQUIRED)

# # Add the executable
# add_executable(mongodb_test mongodb_test.cpp)

# # Manually specify include directories
# target_include_directories(mongodb_test PRIVATE 
#     /usr/local/include/bsoncxx/v_noabi
#     /usr/local/include/mongocxx/v_noabi
#     /usr/local/include/bsoncxx/v_noabi/bsoncxx/third_party/mnmlstc
#     /usr/local/include/bsoncxx/v_noabi/bsoncxx/third_party/mnmlstc/core
# )

# # Link libraries to the target
# target_link_libraries(mongodb_test PRIVATE 
#     /usr/local/lib/libmongocxx.dylib 
#     /usr/local/lib/libbsoncxx.dylib
# )

# # Specify C++ standard
# set_target_properties(mongodb_test PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED YES
#     CXX_EXTENSIONS NO
# )


cmake_minimum_required(VERSION 3.10)
project(mongodb_test)

# Find the MongoDB C++ driver
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

# Add the executable
add_executable(mongodb_test mongodb_test.cpp)

# Include directories and link libraries
target_link_libraries(mongodb_test PRIVATE mongo::mongocxx_shared mongo::bsoncxx_shared)

# Specify C++ standard
set_target_properties(mongodb_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)



