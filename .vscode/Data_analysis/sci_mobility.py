from pymongo import MongoClient
import pandas as pd
import skmob

# Connect to MongoDB
client = MongoClient('localhost', 27017)  # Adjust connection parameters if needed
db = client['Simulation']
collection = db['Agents']

# Fetch data from MongoDB
data = list(collection.find())

# Convert to DataFrame
df = pd.DataFrame(data)

# Extract relevant columns and rename for skmob compatibility
df = df[['timestamp', 'agent_id', 'type', 'position', 'velocity']]

# Convert position and velocity arrays to separate columns
df[['lng', 'lat']] = pd.DataFrame(df['position'].tolist(), index=df.index)

# Convert timestamp to datetime format if it's not already
df['timestamp'] = pd.to_datetime(df['timestamp'])

# Create TrajDataFrame
tdf = skmob.TrajDataFrame(df, latitude='lat', longitude='lng', datetime='timestamp', user_id='agent_id')

# Privacy risk assessment
privacy_report = skmob.privacy.compute_privacy_metrics(tdf, ['timestamp', 'type'])
print(privacy_report)

# Close MongoDB connection
client.close()