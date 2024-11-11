from pymongo import MongoClient
import pandas as pd

# Connect to MongoDB
client = MongoClient('localhost', 27017)  # Adjust connection parameters if needed
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
gb_sensor_data = gb_sensor_data[['timestamp', 'sensor_id', 'cell_index', 'total_agents', 'agent_type_count']]

# Convert position and velocity arrays to separate columns
agents_data[['position_x', 'position_y']] = pd.DataFrame(agents_data['position'].tolist(), index=agents_data.index)
agents_data[['velocity_x', 'velocity_y']] = pd.DataFrame(agents_data['velocity'].tolist(), index=agents_data.index)
ab_sensor_data[['position_x', 'position_y']] = pd.DataFrame(ab_sensor_data['position'].tolist(), index=ab_sensor_data.index)
ab_sensor_data[['estimated_velocity_x', 'estimated_velocity_y']] = pd.DataFrame(ab_sensor_data['estimated_velocity'].tolist(), index=ab_sensor_data.index)

# Define aggregation functions
aggregation_gt_functions = {
    'timestamp': lambda x: list(x),  # Collect all timestamps in a list
    'type': lambda x: list(x),  # Collect all types in a list
    'position_x': lambda x: list(x),  # Collect all positions_x in a list
    'position_y': lambda x: list(x),  # Collect all positions_y in a list
    'velocity_x': lambda x: list(x),  # Collect all velocities_x in a list
    'velocity_y': lambda x: list(x)   # Collect all velocities_y in a list
}

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

# Define aggregation functions for agent-based sensor data
# aggregation_gbs_functions = {
#     'timestamp': lambda x: list(x),  # Collect all timestamps in a list
#     # 'total_agents': lambda x: list(x),  # Collect all total agents in a list
#     'cell_index_x': lambda x: list(x),  # Collect all positions_x in a list
#     'cell_index_y': lambda x: list(x),  # Collect all positions_y in a list
#     'agent_type_count': lambda x: list(x),  # Collect all estimated velocities_x in a list
# }

aggregation_gbs_functions = {
    'timestamp': lambda x: list(x),  # Collect all timestamps in a list
    'total_agents': lambda x: list(x),  # Collect all total_agents in a list
    'agent_type_count': lambda x: list(x),  # Collect all agent_type_count in a list
    'sensor_id': 'first'  # Take the first sensor_id in a list
}
# Convert cell_index from list to tuple
gb_sensor_data['cell_index'] = gb_sensor_data['cell_index'].apply(tuple)

# Group by agent_id and aggregate data
aggregated_agents_data = agents_data.groupby('agent_id').agg(aggregation_gt_functions).reset_index()
aggregated_abs_data = ab_sensor_data.groupby('agent_id').agg(aggregation_abs_functions).reset_index()
aggregated_gbs_data = gb_sensor_data.groupby('cell_index').agg(aggregation_gbs_functions).reset_index()

# Convert cell_index from list to tuple
aggregated_gbs_data['total_agents'] = aggregated_gbs_data['total_agents'].apply(tuple)
aggregated_gbs_data['timestamp'] = aggregated_gbs_data['timestamp'].apply(tuple)
aggregated_gbs_data['agent_type_count'] = aggregated_gbs_data['agent_type_count'].apply(tuple)

# Print the aggregated data
# print(aggregated_agents_data.head())
# print(aggregated_abs_data.head())
# print(aggregated_gbs_data.head())

# Drop original position and velocity columns as they are now expanded
agents_data = agents_data.drop(columns=['position', 'velocity'])
ab_sensor_data = ab_sensor_data.drop(columns=['position', 'estimated_velocity'])

# Convert cell_index from list to tuple
# gb_sensor_data['cell_index'] = gb_sensor_data['cell_index'].apply(tuple)

# Define quasi-identifiers
quasi_identifiers = ['agent_id']

# Define quasi-identifiers for grid-based sensor data
grid_quasi_identifiers = ['total_agents'] 

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

# Calculate k-anonymity with agent_id as the quasi-identifier for the aggregated ground truth data
# k_anonymity_gbs_aggregated = calculate_k_anonymity(aggregated_gbs_data, ['total_agents'])
# l_diversity_gbs_aggregated = calculate_l_diversity(aggregated_gbs_data, ['total_agents'], 'agent_type_count')
# print(f"Aggregated Grid-Based Data k-Anonymity with agent_type_count: {k_anonymity_gbs_aggregated}")
# print(f"Aggregated Grid-Based Data l-diversity: {l_diversity_gbs_aggregated}")

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

# Function to calculate l-diversity for grid-based sensor data
def calculate_l_diversity_grid(df, quasi_identifiers, sensitive_attribute, include_counts=True):
    """
    Calculates l-diversity for grid-based sensor data.

    This function determines the minimum number of distinct values for the 
    specified sensitive attribute within any group defined by the quasi-identifiers. 
    Higher l-diversity indicates better protection against attribute disclosure attacks.

    Args:
        df (pandas.DataFrame): The DataFrame containing the grid-based sensor data.
        quasi_identifiers (list): A list of column names to use as quasi-identifiers.
        sensitive_attribute (str): The column name of the sensitive attribute.
        include_counts (bool): Whether to include counts in the l-diversity calculation for agent types.

    Returns:
        int: The l-diversity value (minimum number of unique sensitive attribute values per group).
    """
    
    def count_unique_agent_types(agent_type_counts_list):
        # Convert list of dictionaries to a tuple of agent types for hashing
        unique_types = set()
        for item in agent_type_counts_list:
            if isinstance(item, list):
                constellation = tuple(sorted(d["type"] for d in item if isinstance(d, dict)))
                unique_types.add(constellation)
            else:
                raise ValueError("Unexpected data structure in agent_type_counts_list")
        return len(unique_types)

    def count_unique_agent_type_counts(agent_type_counts_list):
        # Convert list of dictionaries to a tuple of tuples for hashing
        unique_constellations = set()
        for item in agent_type_counts_list:
            if isinstance(item, list):
                constellation = tuple(sorted((d["type"], d["count"]) for d in item if isinstance(d, dict)))
                unique_constellations.add(constellation)
            else:
                raise ValueError("Unexpected data structure in agent_type_counts_list")
        return len(unique_constellations)

    def count_unique_values(values_list):
        return len(set(values_list))

    if sensitive_attribute == 'agent_type_count':
        if include_counts:
            grouped = df.groupby(quasi_identifiers)[sensitive_attribute].apply(count_unique_agent_type_counts)
        else:
            grouped = df.groupby(quasi_identifiers)[sensitive_attribute].apply(count_unique_agent_types)
    else:
        grouped = df.groupby(quasi_identifiers)[sensitive_attribute].apply(count_unique_values)
    
    return grouped.min()


k_anonymity_agents_data = calculate_k_anonymity(agents_data, quasi_identifiers)
l_diversity_agents_data = calculate_l_diversity_grid(agents_data, quasi_identifiers, 'type')
print(f"Agent-Based Sensor Data k-Anonymity: {k_anonymity_agents_data}")
print(f"Agent-Based Sensor Data l-Diversity: {l_diversity_agents_data}")

k_anonymity_ab_sensor = calculate_k_anonymity(ab_sensor_data, quasi_identifiers)
l_diversity_ab_sensor = calculate_l_diversity_grid(ab_sensor_data, quasi_identifiers, 'type')
print(f"Agent-Based Sensor Data k-Anonymity: {k_anonymity_ab_sensor}")
print(f"Agent-Based Sensor Data l-Diversity: {l_diversity_ab_sensor}")

# Example usage
k_anonymity_gb_sensor = calculate_k_anonymity_grid(gb_sensor_data, grid_quasi_identifiers)
l_diversity_gb_sensor_total_agents = calculate_l_diversity_grid(gb_sensor_data, grid_quasi_identifiers, 'total_agents')
l_diversity_gb_sensor_agent_type_count_with_counts = calculate_l_diversity_grid(gb_sensor_data, grid_quasi_identifiers, 'agent_type_count', include_counts=True)
l_diversity_gb_sensor_agent_type_count_without_counts = calculate_l_diversity_grid(gb_sensor_data, grid_quasi_identifiers, 'agent_type_count', include_counts=False)

# Print results
print(f"Grid-Based Sensor Data k-Anonymity: {k_anonymity_gb_sensor}")
print(f"Grid-Based Sensor Data l-Diversity (total_agents): {l_diversity_gb_sensor_total_agents}")
print(f"Grid-Based Sensor Data l-Diversity (agent_type_count with counts): {l_diversity_gb_sensor_agent_type_count_with_counts}")
print(f"Grid-Based Sensor Data l-Diversity (agent_type_count without counts): {l_diversity_gb_sensor_agent_type_count_without_counts}")
