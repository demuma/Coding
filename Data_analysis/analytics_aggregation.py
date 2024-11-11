from pymongo import MongoClient
from bson.son import SON
import pandas as pd

from pymongo import MongoClient
import pandas as pd
import matplotlib.pyplot as plt
# import seaborn as sns

# Connect to MongoDB
client = MongoClient('localhost', 27017)  # Adjust connection parameters if needed
db = client['Simulation']

collection = db['AB_Sensor_Data']

# Setup MongoDB pipeline to aggregate data
# pipeline = [
#     {
#         '$group': {
#             '_id': '$agent_id',
#             'timestamps': {'$push': '$timestamp'},
#             'types': {'$push': '$type'},
#             'positions': {'$push': '$position'},
#             'velocities': {'$push': '$estimated_velocity'}
#         }
#     }
# ]

# # Construct the aggregation pipeline
# pipeline = [
#     {   
#         '$match': {
#             'data_type': 'agent_data'
#         }
#     },
#     {
#         "$group": {
#             "_id": "$timestamp",
#             "agents": {
#                 "$push": "$$ROOT"
#             }
#         }
#     },
#     {
#         "$sort": {
#             "_id": 1  # 1 for ascending, -1 for descending
#         }
#     }
# ]

# Working
# quasi_identifiers = ['agent_id']  # Specify the quasi-identifiers here

# # Construct the $group stage dynamically
# group_stage = {
#     "$group": {
#         "_id": {field: f"${field}" for field in quasi_identifiers},
#         "agents": {"$push": "$$ROOT"},
#         "unique_types": {"$addToSet": "$type"}
#     }
# }

# # Create the pipeline
# pipeline = [
#     {
#         '$match': {
#             'data_type': 'agent_data'
#         }
#     },
#     group_stage,
#     {
#         "$project": {
#             "_id": 0,
#             **{field: f"$_id.{field}" for field in quasi_identifiers},
#             "k_anonymity": {"$size": "$agents"},
#             "l_diversity": {"$size": "$unique_types"},
#             "agents": 1
#         }
#     },
#     {
#         "$sort": {
#             "timestamp": 1
#         }
#     }
# ]

# Define the quasi-identifiers
quasi_identifiers = ['timestamp', 'type']

# Construct the $group stage dynamically
group_stage = {
    "$group": {
        "_id": {field: f"${field}" for field in quasi_identifiers},
        "k_anonymity": {"$sum": 1},  # Count the number of entries in each group
        "unique_types": {"$addToSet": "$type"},  # Collect unique types for l-diversity
        "entries": {"$push": "$$ROOT"}  # Collect all entries for delta-presence analysis
    }
}

# Create the pipeline
pipeline = [
    {
        '$match': {
            'data_type': 'agent_data'  # Filter for agent data
        }
    },
    group_stage,
    {
        "$project": {
            "_id": 0,
            "timestamp": "$_id.timestamp",
            "type": "$_id.type",
            "k_anonymity": 1,
            "l_diversity": {"$size": "$unique_types"},  # Calculate l-diversity
            "entries": 1
        }
    },
    {
        "$sort": {
            "timestamp": 1  # Sort by timestamp if desired
        }
    }
]



# Execute the aggregation pipeline
result = collection.aggregate(pipeline)


# Iterate through the results
# for doc in result:
#     print(doc)

# Print the first document
# print(next(result))

# Print the k-anonymity of the first document
# print(next(result)['k_anonymity'])

# Delete the collection "Analysis" first
db['Analysis'].drop()

# Convert the result to a DataFrame for easier manipulation
data = pd.DataFrame(list(result))

# Calculate delta-presence
data['delta_presence'] = data['entries'].apply(lambda x: len(set(entry['agent_id'] for entry in x)))

# Display the results
print(data[['timestamp', 'type', 'k_anonymity', 'l_diversity', 'delta_presence']])

# # Post to database collection "Analysis"
# db['Analysis'].insert_many(result)
exit()


# Load data from collections 
ab_sensor_data = pd.DataFrame(list(db['AB_Sensor_Data'].find()))

# Close the MongoDB connection
client.close()

# Extract relevant fields for analysis
ab_sensor_data = ab_sensor_data[['timestamp', 'sensor_id', 'agent_id', 'type', 'position', 'estimated_velocity']]

# Convert position and velocity arrays to separate columns
ab_sensor_data[['position_x', 'position_y']] = pd.DataFrame(ab_sensor_data['position'].tolist(), index=ab_sensor_data.index)
ab_sensor_data[['estimated_velocity_x', 'estimated_velocity_y']] = pd.DataFrame(ab_sensor_data['estimated_velocity'].tolist(), index=ab_sensor_data.index)

# Define aggregation functions for agent-based sensor data
aggregation_abs_functions = {
    'timestamp': lambda x: list(x),  # Collect all timestamps in a list
    'type': lambda x: list(x),  # Collect all types in a list
    'sensor_id': 'first',  # Take the first sensor_id in a list
    'position_x': lambda x: list(x),  # Collect all positions_x in a list
    'position_y': lambda x: list(x),  # Collect all positions_y in a list
    'estimated_velocity_x': lambda x: list(x),  # Collect all estimated velocities_x in a list
    'estimated_velocity_y': lambda x: list(x)   # Collect all estimated velocities_y in a list
}

# Group by agent_id and aggregate data
aggregated_abs_data = ab_sensor_data.groupby('agent_id').agg(aggregation_abs_functions).reset_index()

# Drop original position and velocity columns as they are now expanded
ab_sensor_data = ab_sensor_data.drop(columns=['position', 'estimated_velocity'])

# Define quasi-identifiers
quasi_identifiers = ['agent_id']

# Function to calculate k-anonymity
def calculate_k_anonymity(df, quasi_identifiers):
    """
    Calculates k-anonymity for agent-based sensor or simulation data.

    This function determines the minimum number of records within any group
    defined by the specified quasi-identifiers (e.g., 'timestamp', 'type', 'position_x', 'position_y').
    A higher k-anonymity value indicates better privacy protection.

    Args:
        df (pandas.DataFrame): The DataFrame containing the agent-based sensor or simulation data.
        quasi_identifiers (list): A list of column names to use as quasi-identifiers.

    Returns:
        int: The k-anonymity value (minimum group size).
    """
    grouped = df.groupby(quasi_identifiers).size()
    return grouped.min()

# Function to calculate l-diversity
def calculate_l_diversity(df, quasi_identifiers, sensitive_attribute):
    """
    Calculates l-diversity for agent-based sensor or simulation data.

    This function determines the minimum number of distinct values for the
    specified sensitive attribute (e.g., 'velocity_x') within any group
    defined by the quasi-identifiers. Higher l-diversity indicates better
    protection against attribute disclosure attacks.

    Args:
        df (pandas.DataFrame): The DataFrame containing the agent-based sensor or simulation data.
        quasi_identifiers (list): A list of column names to use as quasi-identifiers.
        sensitive_attribute (str): The column name of the sensitive attribute.

    Returns:
        int: The l-diversity value (minimum number of unique sensitive attribute values per group).
    """
    def count_unique_elements(elements):
        unique_elements = set()
        for element in elements:
            if isinstance(element, list):
                unique_elements.update(element)
            else:
                unique_elements.add(element)
        return len(unique_elements)

    grouped = df.groupby(quasi_identifiers)[sensitive_attribute].apply(count_unique_elements)
    return grouped.min()

# Calculate k-anonymity with agent_id as the quasi-identifier for the aggregated ground truth data
k_anonymity_gt_aggregated = calculate_k_anonymity(aggregated_agents_data, ['agent_id'])
l_diversity_gt_aggregated = calculate_l_diversity(aggregated_agents_data, ['agent_id'], 'type')
print(f"Aggregated Ground Truth Data k-Anonymity with agent_id: {k_anonymity_gt_aggregated}")
print(f"Aggregated Ground Truth Data l-diversity with agent_id: {l_diversity_gt_aggregated}")

# Calculate k-anonymity with agent_id as the quasi-identifier for the aggregated data
k_anonymity_abs_aggregated = calculate_k_anonymity(aggregated_abs_data, ['agent_id'])
l_diversity_abs_aggregated = calculate_l_diversity(aggregated_abs_data, ['agent_id'], 'type')
print(f"Aggregated Agent-Based Data k-Anonymity with agent_id: {k_anonymity_abs_aggregated}")
print(f"Aggregated Agent-Based Data l-diversity with agent_id: {l_diversity_abs_aggregated}")

k_anonymity_agents_data = calculate_k_anonymity(agents_data, quasi_identifiers)
l_diversity_agents_data = calculate_l_diversity_grid(agents_data, quasi_identifiers, 'type')
print(f"Agent-Based Sensor Data k-Anonymity: {k_anonymity_agents_data}")
print(f"Agent-Based Sensor Data l-Diversity: {l_diversity_agents_data}")

k_anonymity_ab_sensor = calculate_k_anonymity(ab_sensor_data, quasi_identifiers)
l_diversity_ab_sensor = calculate_l_diversity_grid(ab_sensor_data, quasi_identifiers, 'type')
print(f"Agent-Based Sensor Data k-Anonymity: {k_anonymity_ab_sensor}")
print(f"Agent-Based Sensor Data l-Diversity: {l_diversity_ab_sensor}")

