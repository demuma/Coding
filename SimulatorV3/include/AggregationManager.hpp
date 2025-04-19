#pragma once

#include <vector>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <chrono>
#include <string>
#include <set>
#include <iostream>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include "Agent.hpp"
#include "Utilities.hpp"
#include "Quadtree.hpp"

class AggregationManager {
public:
    AggregationManager(
        mongocxx::collection& collection,
        std::string sensorId
    ) : collection(collection), sensorId(sensorId) {
    }
    ~AggregationManager();

    struct AggregatedGridData {
        std::string timestamp; // Convert to BSON date
        std::string aggregationStartTime;
        std::string aggregationEndTime;
        std::string aggregationDuration;
        std::string sensorId;
        std::string data_type; // Aggregated adaptive grid data
        int cellId;
        std::string region_type;
        std::vector<int> cell_position;
        std::unordered_map<std::string, int> agentTypeCount;
        int totalAgents;
        float privacyLevel;
        std::unordered_map<std::string, float> privacyMetrics;
    };

    struct AggregatedGridDataBucket {
        std::unordered_map<int, AggregatedGridData> aggregatedData;
        std::string timestamp;  // For when data is sent
        float aggregationDuration; 
        std::string aggregationStartTime;

        AggregatedGridDataBucket(std::string aggregationStartTime)
            : aggregationStartTime(aggregationStartTime) {}

        void reset() {
            aggregatedData.clear();
            timestamp.clear();
        }
    };

private:
    mongocxx::collection& collection;
    std::string sensorId;
    std::unordered_map<int, AggregationManager::AggregatedGridDataBucket> aggregatedGridDataBuckets; // Grid Data: map(cell id, map(agent type, count)
};