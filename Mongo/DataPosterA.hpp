#ifndef DATAPOSTERA_HPP
#define DATAPOSTERA_HPP

#include "DataPosterBase.hpp"

class DataPosterA : public DataPosterBase {
public:
    using DataPosterBase::DataPosterBase;

    void postData(const std::string& databaseName, const std::string& collectionName, bsoncxx::document::view document) override {
        try {
            auto collection = m_client->database(databaseName)[collectionName];
            collection.insert_one(document);
            std::cout << "DataPosterA posted data successfully!" << std::endl;
        } catch (const mongocxx::exception& e) {
            std::cerr << "An error occurred while inserting documents: " << e.what() << std::endl;
        }
    }
};

#endif // DATAPOSTERA_HPP