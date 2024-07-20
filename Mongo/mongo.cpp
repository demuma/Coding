#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/exception/exception.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include "DataPosterBase.hpp"
#include "DataPosterA.hpp"
#include "DataPosterB.hpp"

int main() {
    std::string connectionString = "mongodb://localhost:27017";  // Replace with your actual connection string

    mongocxx::instance instance{};
    mongocxx::uri uri(connectionString);
    auto clientPtr = std::make_shared<mongocxx::client>(uri);

    const int numPosters = 3;
    std::vector<std::unique_ptr<DataPosterBase>> posters;

    for (int i = 0; i < numPosters; ++i) {
        posters.push_back(std::make_unique<DataPosterA>(clientPtr));
        posters.push_back(std::make_unique<DataPosterB>(clientPtr));
    }

    for (int i = 0; i < posters.size(); ++i) {
        // Build the document
        bsoncxx::builder::stream::document doc{};
        doc << "posterId" << bsoncxx::builder::stream::open_document
            << "idValue" << i
            << bsoncxx::builder::stream::close_document
            << "data" << ("Some unique data for poster " + std::to_string(i));

        // Post data sequentially
        posters[i]->postData("your_database_name", "collection1", doc.view());
        std::cout << bsoncxx::to_json(doc.view()) << std::endl; // Print the document (optional)
    }

    std::cout << "All data posters finished." << std::endl;

    return 0; // Return 0 to indicate successful execution
}
