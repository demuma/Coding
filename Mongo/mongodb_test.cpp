// #include <iostream>
// #include <bsoncxx/builder/stream/document.hpp>
// #include <mongocxx/client.hpp>
// #include <mongocxx/instance.hpp>
// #include <mongocxx/exception/exception.hpp> // Include this header for mongocxx exceptions

// using bsoncxx::builder::stream::document;
// using bsoncxx::builder::stream::open_document;
// using bsoncxx::builder::stream::close_document;

// int main() {
//     // Required for MongoDB C++ Driver initialization
//     mongocxx::instance inst{}; 

//     try {
//         // Connect to the database
//         mongocxx::client client{mongocxx::uri{}}; // Connect to localhost:27017 by default

//         // Get a handle to the "test" database and the "restaurants" collection
//         auto collection = client["Simulation"]["Agents"];

//         // Create a sample document
//         document builder{};
//         builder << "name" << "Central Perk"
//                << "cuisine" << "Coffee"
//                << "borough" << "Manhattan";

//         // Insert the document into the collection
//         collection.insert_one(builder.view());
        
//         // Print a confirmation message
//         std::cout << "Document inserted successfully!" << std::endl;
//     } catch (const mongocxx::exception& e) { // Use mongocxx::exception
//         std::cerr << "Error: " << e.what() << std::endl;
//         return 1; // Indicate failure
//     }

//     return 0; // Indicate success
// }

#include <iostream>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/exception/exception.hpp>
#include <ctime>
#include <chrono>
#include <vector>
#include <uuid/uuid.h>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::finalize;

class RoadUser {
public:
    std::string class_name = "pedestrian";
    class ordinary {
    public: 
        std::string class_name = "ordinary";
    };
    class child {
    public: 
        std::string class_name = "child";
    };
    class elderly {
    public: 
        std::string class_name = "elderly";
    };
    class autonomous {
    public: 
        std::string class_name = "autonomous";
    };
    class animal {
    public: 
        std::string class_name = "animal";
    };
    class assisted {
    public: 
        std::string class_name = "assisted";
    };
    class disabled {
    public: 
        std::string class_name = "disabled";
    };
};

int main() {
    // Initialize the MongoDB C++ driver
    mongocxx::instance inst{};
    mongocxx::client conn{mongocxx::uri{}}; // Connect to localhost

    try {
        // Get a handle to the database and collection
        mongocxx::database db = conn["Simulation"];
        mongocxx::collection collection = db["Agents"];

        // Get current date and time (C++ style)
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        char timeBuffer[80];
        char dateBuffer[80];
        std::strftime(timeBuffer, 80, "%H:%M:%S", std::localtime(&now_c));
        std::strftime(dateBuffer, 80, "%Y-%m-%d", std::localtime(&now_c));

        // Define pedestrian data
        double height = 1.65;
        std::vector<double> pose = {0, 0, 0, 0, 0, 0};
        std::vector<double> coordinates = {53.55703, 10.02303};
        std::vector<double> dimensions = {0, 0, height};

        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);

        // Create BSON document to insert
        document builder{};
        builder
            << "_id" << bsoncxx::oid{}
            << "uuid" << uuid_str
            << "class" << "senior"
            << "height" << height
            << "pose" << bsoncxx::builder::stream::open_array
                << pose[0] << pose[1] << pose[2] << pose[3] << pose[4] << pose[5]
            << bsoncxx::builder::stream::close_array
            << "coordinates" << bsoncxx::builder::stream::open_array
                << coordinates[0] << coordinates[1]
            << bsoncxx::builder::stream::close_array
            << "dimensions" << bsoncxx::builder::stream::open_array
                << dimensions[0] << dimensions[1] << dimensions[2]
            << bsoncxx::builder::stream::close_array
            << "date" << dateBuffer
            << "time" << timeBuffer;

        // Insert document
        collection.insert_one(builder.view());

        std::cout << "Data inserted successfully" << std::endl;
    } catch (const mongocxx::exception& e) {
        std::cerr << "MongoDB Exception: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown Exception" << std::endl;
        return 1;
    }

    return 0;
}