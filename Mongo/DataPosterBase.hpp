#ifndef DATAPOSTERBASE_HPP
#define DATAPOSTERBASE_HPP

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/exception.hpp>
#include <memory>

class DataPosterBase {
public:
    explicit DataPosterBase(std::shared_ptr<mongocxx::client> client) : m_client(std::move(client)) {}
    virtual ~DataPosterBase() = default;

    virtual void postData(const std::string& databaseName, const std::string& collectionName, bsoncxx::document::view document) = 0;

protected:
    std::shared_ptr<mongocxx::client> m_client;
};

#endif // DATAPOSTERBASE_HPP
