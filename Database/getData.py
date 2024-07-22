import pymongo
from pymongo import MongoClient
from datetime import datetime
from random import *
import numpy as np
import array


###############################################################
#                        Data model
###############################################################

# Data struture
# ID, Day/Time, Pose, Class, Height
# ID (Int)
# Day (_date)
# Time (_time)
# Classes (Int):
# Ordinary(1), Children(2), Elderly(3), Assisted(4), Disabled(5), Animals(6), Autonomous(7), Automotive (8)
# Pose (Vector as numpy array in euler angles):
# Position (X,Y,Z), Orientation(Phi,Theta,Psi)


# Connect client to MongoDB server
client = MongoClient("localhost", 27017)

# Get list of databases on server
db_list = client.list_database_names()
#print(db_list)

# Define database
db = client["Simulation"]
#print(db)

# Define data collection within database
collection = db["AB_Sensor_Data"]

# Get data (called documents) from database
results = collection.find({"type":"Adult Cyclist"})

for result in results:
    print(result)