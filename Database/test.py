import pandas as pd
from pymongo import MongoClient

# Connect to MongoDB
client = MongoClient('localhost', 27017)
db = client['Simulation']
collection = db['GB_Sensor_Data']

# Load data from MongoDB collection
data = list(collection.find())

# Convert to DataFrame
df = pd.DataFrame(data)

# Ensure cell_index is a tuple for hashing
df['cell_index'] = df['cell_index'].apply(tuple)

# Explode agent_type_count into separate rows
df = df.explode('agent_type_count')

# Normalize the dictionaries within agent_type_count
df = df.join(pd.json_normalize(df.pop('agent_type_count')))

# Now, agent_type_count is normalized into 'type' and 'count' columns
print(df.head())

# Convert lists to tuples in type and count columns for hashing
df['type'] = df['type'].apply(lambda x: tuple(x) if isinstance(x, list) else x)
df['count'] = df['count'].apply(lambda x: tuple(x) if isinstance(x, list) else x)

# Define aggregation functions for the new columns
aggregation_functions = {
    'timestamp': lambda x: list(x),  # Collect all timestamps in a list
    'total_agents': lambda x: tuple(x),  # Collect all total_agents in a list
    'type': lambda x: list(x),  # Collect all types in a list
    'count': lambda x: list(x)  # Collect all counts in a list
}

# Group by cell_index and aggregate data
aggregated_gb_data = df.groupby('cell_index').agg(aggregation_functions).reset_index()

# Print the aggregated data
print(aggregated_gb_data)
