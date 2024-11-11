from pymongo import MongoClient
import pandas as pd
import numpy as np

# Connect to MongoDB
client = MongoClient('localhost', 27017)
db = client['Simulation']
collection = db['AB_Sensor_Data']

# Define bin sizes
speed_bin_size = 0.5  # Adjust as needed
position_bin_size = 10  # Adjust as needed

# Privacy thresholds
k_threshold = 5
l_threshold = 2

# Function to perform aggregation with a specified temporal window
def aggregate_data(time_window):
    pipeline = [
        {
            '$match': {
                'data_type': 'agent_data'
            }
        },
        # Compute the speed (magnitude of estimated_velocity)
        {
            '$addFields': {
                'speed': {
                    '$sqrt': {
                        '$add': [
                            { '$pow': [ { '$arrayElemAt': [ '$estimated_velocity', 0 ] }, 2 ] },
                            { '$pow': [ { '$arrayElemAt': [ '$estimated_velocity', 1 ] }, 2 ] }
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
                        { '$floor': { '$divide': [ '$speed', speed_bin_size ] } },
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
                            { '$floor': { '$divide': [ { '$arrayElemAt': [ '$position', 0 ] }, position_bin_size ] } },
                            position_bin_size
                        ]
                    },
                    {
                        '$multiply': [
                            { '$floor': { '$divide': [ { '$arrayElemAt': [ '$position', 1 ] }, position_bin_size ] } },
                            position_bin_size
                        ]
                    }
                ]
            }
        },
        # Convert 'timestamp' from string to date
        {
            '$addFields': {
                'timestamp_date': {
                    '$dateFromString': {
                        'dateString': '$timestamp',
                        'format': "%Y-%m-%dT%H:%M:%S.%L"
                    }
                }
            }
        },
        # Truncate the timestamp to the specified temporal window
        {
            '$addFields': {
                'rounded_timestamp': {
                    '$dateTrunc': {
                        'date': '$timestamp_date',
                        'unit': 'second',
                        'binSize': time_window
                    }
                }
            }
        },
        # Group and compute privacy metrics
        {
            '$group': {
                '_id': {
                    'timestamp': '$rounded_timestamp',
                    'speed_bin': '$speed_bin',
                    'position_bin': '$position_bin'
                },
                'k_anonymity': { '$sum': 1 },
                'unique_types': { '$addToSet': '$type' },
                'entries': { '$push': '$$ROOT' }
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
                'l_diversity': { '$size': '$unique_types' },
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
    result = list(collection.aggregate(pipeline))
    data = pd.DataFrame(result)

    # Convert 'position_bin' from list to tuple for indexing
    if 'position_bin' in data.columns:
        data['position_bin'] = data['position_bin'].apply(tuple)
    else:
        print("Warning: 'position_bin' not found in data after aggregation.")

    # Verify 'entries' column
    if 'entries' in data.columns:
        # Handle null or non-list entries
        data['entries'] = data['entries'].apply(lambda x: x if isinstance(x, list) else [])
    else:
        print("Error: 'entries' column is missing from the DataFrame.")
        exit()

    return data

# Step 1: Initial aggregation with 1-second window
data = aggregate_data(time_window=1)

# Step 2: Identify bins below privacy thresholds
if 'position_bin' in data.columns:
    bins_to_adjust = data[
        (data['k_anonymity'] < k_threshold) | (data['l_diversity'] < l_threshold)
    ][['speed_bin', 'position_bin']].drop_duplicates()

    # Convert 'position_bin' to tuples
    bins_to_adjust['position_bin'] = bins_to_adjust['position_bin'].apply(tuple)
else:
    print("Error: 'position_bin' not found in data.")
    exit()

# Debugging statements
print("Bins to adjust:", bins_to_adjust)
bins_list = bins_to_adjust.to_dict('records')
print("Bins list:", bins_list)

# Step 3: Adjust temporal aggregation for these bins
if not bins_to_adjust.empty:
    # Prepare conditions for matching bins
    bins_match_condition = {
        '$or': [
            {
                'speed_bin': bin['speed_bin'],
                'position_bin.0': bin['position_bin'][0],
                'position_bin.1': bin['position_bin'][1]
            }
            for bin in bins_list
        ]
    }

    # Print bins_match_condition for debugging
    print("Bins match condition:", bins_match_condition)

    # Adjusted pipeline for bins needing longer temporal aggregation
    adjusted_pipeline = [
        {
            '$match': {
                'data_type': 'agent_data'
            }
        },
        # Compute speed, speed_bin, position_bin, and timestamp_date
        {
            '$addFields': {
                'speed': {
                    '$sqrt': {
                        '$add': [
                            { '$pow': [ { '$arrayElemAt': [ '$estimated_velocity', 0 ] }, 2 ] },
                            { '$pow': [ { '$arrayElemAt': [ '$estimated_velocity', 1 ] }, 2 ] }
                        ]
                    }
                },
                'speed_bin': {
                    '$multiply': [
                        { '$floor': { '$divide': [ '$speed', speed_bin_size ] } },
                        speed_bin_size
                    ]
                },
                'position_bin': [
                    {
                        '$multiply': [
                            { '$floor': { '$divide': [ { '$arrayElemAt': [ '$position', 0 ] }, position_bin_size ] } },
                            position_bin_size
                        ]
                    },
                    {
                        '$multiply': [
                            { '$floor': { '$divide': [ { '$arrayElemAt': [ '$position', 1 ] }, position_bin_size ] } },
                            position_bin_size
                        ]
                    }
                ],
                'timestamp_date': {
                    '$dateFromString': {
                        'dateString': '$timestamp',
                        'format': "%Y-%m-%dT%H:%M:%S.%L"
                    }
                }
            }
        },
        # Match only the bins that need adjustment
        {
            '$match': {
                '$or': [
                    {
                        'speed_bin': bin['speed_bin'],
                        'position_bin.0': bin['position_bin'][0],
                        'position_bin.1': bin['position_bin'][1]
                    }
                    for bin in bins_list
                ]
            }
        },
        # Use a larger binSize for temporal aggregation (e.g., 5 seconds)
        {
            '$addFields': {
                'rounded_timestamp': {
                    '$dateTrunc': {
                        'date': '$timestamp_date',
                        'unit': 'second',
                        'binSize': 5
                    }
                }
            }
        },
        # Group and compute privacy metrics
        {
            '$group': {
                '_id': {
                    'timestamp': '$rounded_timestamp',
                    'speed_bin': '$speed_bin',
                    'position_bin': '$position_bin'
                },
                'k_anonymity': { '$sum': 1 },
                'unique_types': { '$addToSet': '$type' },
                'entries': { '$push': '$$ROOT' }
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
                'l_diversity': { '$size': '$unique_types' },
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

    adjusted_result = list(collection.aggregate(adjusted_pipeline))
    print("Adjusted result count:", len(adjusted_result))  # Debugging statement
    adjusted_data = pd.DataFrame(adjusted_result)

    if not adjusted_data.empty:
        # Convert 'position_bin' from list to tuple
        adjusted_data['position_bin'] = adjusted_data['position_bin'].apply(tuple)

        # Verify 'entries' column
        adjusted_data['entries'] = adjusted_data['entries'].apply(lambda x: x if isinstance(x, list) else [])

        # Step 4: Combine initial data and adjusted data
        adjusted_bins = bins_to_adjust.set_index(['speed_bin', 'position_bin'])
        data = data.set_index(['speed_bin', 'position_bin'])
        data = data.drop(index=adjusted_bins.index, errors='ignore').reset_index()
        adjusted_data = adjusted_data.reset_index(drop=True)
        data = pd.concat([data, adjusted_data], ignore_index=True)
    else:
        print("Warning: No data returned from adjusted aggregation.")
else:
    # If no bins need adjustment, reset index to ensure consistency
    data = data.reset_index(drop=True)

# Recalculate delta-presence
def compute_delta_presence(x):
    if isinstance(x, list):
        return len(set(entry['agent_id'] for entry in x if 'agent_id' in entry))
    else:
        return 0

data['delta_presence'] = data['entries'].apply(compute_delta_presence)

# Display the results
print(data[['timestamp', 'speed_bin', 'position_bin', 'k_anonymity', 'l_diversity', 'delta_presence']])

# Optionally, write the DataFrame to a CSV file
data[['timestamp', 'speed_bin', 'position_bin', 'k_anonymity', 'l_diversity', 'delta_presence']].to_csv('privacy_metrics.csv', index=False)
