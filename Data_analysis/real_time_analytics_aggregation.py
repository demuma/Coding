from pymongo import MongoClient
import pandas as pd

# Connect to MongoDB
client = MongoClient('localhost', 27017)  # Adjust connection parameters if needed
db = client['Simulation']

# Choose the collection to work with
collection = db['AB_Sensor_Data']

# Define bin sizes
speed_bin_size = 0.5  # Adjust as needed (e.g., 5 m/s)
position_bin_size = 10  # Adjust as needed (e.g., 10 units)

# Define the aggregation pipeline
pipeline = [
    {
        '$match': {
            'data_type': 'agent_data'  # Filter for agent data
        }
    },
    # Compute the speed (magnitude of estimated_velocity)
    {
        '$addFields': {
            'speed': {
                '$sqrt': {
                    '$add': [
                        {'$pow': [{'$arrayElemAt': ['$estimated_velocity', 0]}, 2]},
                        {'$pow': [{'$arrayElemAt': ['$estimated_velocity', 1]}, 2]}
                    ]
                }
            }
        }
    },
    # Bin the speed
    {
        '$addFields': {
            'speed_bin': {
                '$multiply': [
                    {'$floor': {'$divide': ['$speed', speed_bin_size]}},
                    speed_bin_size
                ]
            }
        }
    },
    # Bin the position
    {
        '$addFields': {
            'position_bin': [
                {
                    '$multiply': [
                        {'$floor': {'$divide': [{'$arrayElemAt': ['$position', 0]}, position_bin_size]}},
                        position_bin_size
                    ]
                },
                {
                    '$multiply': [
                        {'$floor': {'$divide': [{'$arrayElemAt': ['$position', 1]}, position_bin_size]}},
                        position_bin_size
                    ]
                }
            ]
        }
    },
    # Group by timestamp, speed_bin, and position_bin
    {
        '$group': {
            '_id': {
                'timestamp': '$timestamp',
                'speed_bin': '$speed_bin',
                'position_bin': '$position_bin'
            },
            'k_anonymity': {'$sum': 1},
            'unique_types': {'$addToSet': '$type'},
            'entries': {'$push': '$$ROOT'}
        }
    },
    # Project the required fields
    {
        '$project': {
            '_id': 0,
            'timestamp': '$_id.timestamp',
            'speed_bin': '$_id.speed_bin',
            'position_bin': '$_id.position_bin',
            'k_anonymity': 1,
            'l_diversity': {'$size': '$unique_types'},
            'entries': 1
        }
    },
    # Sort the results
    {
        '$sort': {
            'timestamp': 1
        }
    }
]

# Execute the aggregation pipeline
result = collection.aggregate(pipeline)

# Convert the result to a DataFrame
data = pd.DataFrame(list(result))

# Calculate delta-presence
data['delta_presence'] = data['entries'].apply(lambda x: len(set(entry['agent_id'] for entry in x)))

# Display the results
print(data[['timestamp', 'speed_bin', 'position_bin', 'k_anonymity', 'l_diversity', 'delta_presence']])

# Write the DataFrame to a CSV file
data[['timestamp', 'speed_bin', 'position_bin', 'k_anonymity', 'l_diversity', 'delta_presence']].to_csv('privacy_metrics.csv', index=False)
