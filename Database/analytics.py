from pymongo import MongoClient
import pandas as pd

# Connect to MongoDB
client = MongoClient('localhost', 27017)
db = client['Simulation']

# Load data from collections
ab_sensor_data = pd.DataFrame(list(db['AB_Sensor_Data'].find()))
gb_sensor_data = pd.DataFrame(list(db['GB_Sensor_Data'].find()))
agents_data = pd.DataFrame(list(db['Agents'].find()))

# Close the MongoDB connection
client.close()

# Extract relevant fields for analysis
agents_data = agents_data[['timestamp', 'agent_id', 'type', 'position', 'velocity']]
ab_sensor_data = ab_sensor_data[['timestamp', 'sensor_id', 'agent_id', 'type', 'position', 'estimated_velocity']]
gb_sensor_data = gb_sensor_data[['timestamp', 'sensor_id', 'cell_index', 'total_agents', 'agent_counts']]

# Convert position and velocity arrays to separate columns
agents_data[['position_x', 'position_y']] = pd.DataFrame(agents_data['position'].tolist(), index=agents_data.index)
agents_data[['velocity_x', 'velocity_y']] = pd.DataFrame(agents_data['velocity'].tolist(), index=agents_data.index)
ab_sensor_data[['position_x', 'position_y']] = pd.DataFrame(ab_sensor_data['position'].tolist(), index=ab_sensor_data.index)
ab_sensor_data[['estimated_velocity_x', 'estimated_velocity_y']] = pd.DataFrame(ab_sensor_data['estimated_velocity'].tolist(), index=ab_sensor_data.index)

# Drop original position and velocity columns as they are now expanded
agents_data = agents_data.drop(columns=['position', 'velocity'])
ab_sensor_data = ab_sensor_data.drop(columns=['position', 'estimated_velocity'])

# Convert cell_index from list to tuple
gb_sensor_data['cell_index'] = gb_sensor_data['cell_index'].apply(tuple)

# Define quasi-identifiers
quasi_identifiers = ['timestamp', 'type', 'position_x', 'position_y']

# Function to calculate k-anonymity
def calculate_k_anonymity(df, quasi_identifiers):
    grouped = df.groupby(quasi_identifiers).size()
    return grouped.min()

# Function to calculate l-diversity
def calculate_l_diversity(df, quasi_identifiers, sensitive_attribute):
    grouped = df.groupby(quasi_identifiers)[sensitive_attribute].nunique()
    return grouped.min()

# Calculate k-anonymity for simulation data
k_anonymity_simulation = calculate_k_anonymity(agents_data, quasi_identifiers)
print(f"Simulation Data k-Anonymity: {k_anonymity_simulation}")

# Calculate l-diversity for simulation data (using velocity_x as a sensitive attribute example)
l_diversity_simulation = calculate_l_diversity(agents_data, quasi_identifiers, 'type')
print(f"Simulation Data l-Diversity: {l_diversity_simulation}")

# Calculate k-anonymity for agent-based sensor data
k_anonymity_ab_sensor = calculate_k_anonymity(ab_sensor_data, quasi_identifiers)
print(f"Agent-Based Sensor Data k-Anonymity: {k_anonymity_ab_sensor}")

# Calculate l-diversity for agent-based sensor data (using estimated_velocity_x as a sensitive attribute example)
l_diversity_ab_sensor = calculate_l_diversity(ab_sensor_data, quasi_identifiers, 'type')
print(f"Agent-Based Sensor Data l-Diversity: {l_diversity_ab_sensor}")

# Define quasi-identifiers for grid-based sensor data
grid_quasi_identifiers = ['timestamp', 'cell_index']

# Function to calculate k-anonymity for grid-based sensor data
def calculate_k_anonymity_grid(df, quasi_identifiers):
    grouped = df.groupby(quasi_identifiers).size()
    return grouped.min()

# Function to calculate l-diversity for grid-based sensor data (using agent_counts as sensitive attribute example)
def calculate_l_diversity_grid(df, quasi_identifiers, sensitive_attribute):
    def count_unique_agent_types(group):
        all_agent_types = set()
        for agent_counts in group[sensitive_attribute]:
            all_agent_types.update(agent_counts.keys())
        return len(all_agent_types)

    grouped = df.groupby(quasi_identifiers).apply(lambda x: count_unique_agent_types(x), include_groups=False)
    return grouped.min()

# Calculate k-anonymity for grid-based sensor data
k_anonymity_gb_sensor = calculate_k_anonymity_grid(gb_sensor_data, grid_quasi_identifiers)
print(f"Grid-Based Sensor Data k-Anonymity: {k_anonymity_gb_sensor}")

# Calculate l-diversity for grid-based sensor data
l_diversity_gb_sensor = calculate_l_diversity_grid(gb_sensor_data, grid_quasi_identifiers, 'agent_counts')
print(f"Grid-Based Sensor Data l-Diversity: {l_diversity_gb_sensor}")
