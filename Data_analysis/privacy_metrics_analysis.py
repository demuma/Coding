from pymongo import MongoClient
import pandas as pd

# Connect to MongoDB
client = MongoClient('localhost', 27017)  # Adjust connection parameters if needed
db = client['Simulation']

# Choose the collection to work with
collection1 = db['AB_Sensor_Data']

# k-anonymity
# k = total number of people with the same quasi-identifier values in the dataset

# l-diversity
# 
