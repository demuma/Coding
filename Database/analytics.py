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
gb_sensor_data = gb_sensor_data[['timestamp', 'sensor_id', 'cell_index', 'total_agents', 'agent_type_counts']]

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
quasi_identifiers = ['type']

# Define quasi-identifiers
agent_quasi_identifiers = ['sensor_id']

# Define quasi-identifiers for grid-based sensor data
grid_quasi_identifiers = ['sensor_id']

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
    grouped = df.groupby(quasi_identifiers)[sensitive_attribute].nunique()
    return grouped.min()

# Calculate k-anonymity for simulation data
k_anonymity_simulation = calculate_k_anonymity(agents_data, quasi_identifiers)
print(f"Simulation Data k-Anonymity: {k_anonymity_simulation}")

# Calculate l-diversity for simulation data (using velocity_x as a sensitive attribute example)
l_diversity_simulation = calculate_l_diversity(agents_data, quasi_identifiers, 'type')
print(f"Simulation Data l-Diversity: {l_diversity_simulation}")

# Calculate k-anonymity for agent-based sensor data
k_anonymity_ab_sensor = calculate_k_anonymity(ab_sensor_data, agent_quasi_identifiers)
print(f"Agent-Based Sensor Data k-Anonymity: {k_anonymity_ab_sensor}")

# Calculate l-diversity for agent-based sensor data (using estimated_velocity_x as a sensitive attribute example)
l_diversity_ab_sensor = calculate_l_diversity(ab_sensor_data, agent_quasi_identifiers, 'type')
print(f"Agent-Based Sensor Data l-Diversity: {l_diversity_ab_sensor}")


# Function to calculate k-anonymity for grid-based sensor data
def calculate_k_anonymity_grid(df, quasi_identifiers):
    """
    Calculates k-anonymity for grid-based sensor data.

    This function determines the minimum number of records within any group 
    defined by the specified quasi-identifiers (e.g., timestamp, cell_index). 
    A higher k-anonymity value indicates better privacy protection.

    Args:
        df (pandas.DataFrame): The DataFrame containing the grid-based sensor data.
        quasi_identifiers (list): A list of column names to use as quasi-identifiers 
                                (e.g., ['timestamp', 'cell_index']).

    Returns:
        int: The k-anonymity value (minimum group size).
    """
    grouped = df.groupby(quasi_identifiers).size()
    return grouped.min()

# Function to calculate l-diversity for grid-based sensor data (using agent_type_counts as sensitive attribute example)
def calculate_l_diversity_grid(df, quasi_identifiers, sensitive_attribute='total_agents'):
    """
    Calculates l-diversity for grid-based sensor data.

    This function determines the minimum number of distinct values for the 
    specified sensitive attribute (e.g., 'total_agents') within any group
    defined by the quasi-identifiers.  Higher l-diversity indicates better 
    protection against attribute disclosure attacks.

    Args:
        df (pandas.DataFrame): The DataFrame containing the grid-based sensor data.
        quasi_identifiers (list): A list of column names to use as quasi-identifiers.
        sensitive_attribute (str, optional): The column name of the sensitive attribute. Defaults to 'total_agents'.

    Returns:
        int: The l-diversity value (minimum number of unique sensitive attribute values per group).
    """
    grouped = df.groupby(quasi_identifiers)[sensitive_attribute].nunique()
    return grouped.min()

# Calculate k-anonymity for grid-based sensor data
k_anonymity_gb_sensor = calculate_k_anonymity_grid(gb_sensor_data, grid_quasi_identifiers)
print(f"Grid-Based Sensor Data k-Anonymity: {k_anonymity_gb_sensor}")

# Calculate l-diversity for grid-based sensor data
l_diversity_gb_sensor = calculate_l_diversity_grid(gb_sensor_data, grid_quasi_identifiers, 'cell_index')
print(f"Grid-Based Sensor Data l-Diversity: {l_diversity_gb_sensor}")