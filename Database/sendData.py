import pymongo
from pymongo import MongoClient
from datetime import datetime
from random import *
import numpy as np
import array
import uuid
import bson
import sys


###############################################################
#                        Data model
###############################################################

# Data struture
# ID, Day/Time, Pose, Class, Height
# ID (Int)
# Day (_date)
# Time (_time)
# Classes (Int):
# Human(Assisted, Elderly, Children, Disabled, Ordinary), Non-Human(Animals, Autonomous Vehicles, Autonomous Vehicles)
# Ordinary(1), Children(2), Elderly(3), Assisted(4), Disabled(5), Animals(6), Autonomous(7), Automotive (8)
# Pose (Vector as numpy array in euler angles):
# Position (X,Y,Z), Orientation(Phi,Theta,Psi)

# Define pedestrian class
class RoadUsers:

    # Constructor
    def __init__(self):
         self.class_name =  'pedestrian'

    class ordinary:
        def __init__(self):
            self.class_name = 'ordinary'

    class child:
        def __init__(self):
         self.class_name =  'child'

    class elderly:
        def __init__(self):
         self.class_name =  'elderly'

    class autonomous:
        def __init__(self):
         self.class_name =  'autonomous'

    class animal:
        def __init__(self):
         self.class_name =  'animal'

    class assisted:
        def __init__(self):
         self.class_name =  'assisted'

    class disabled:
        def __init__(self):
         self.class_name =  'assisted'

    class autonomous:
       pass


# Connect client to MongoDB server
client = MongoClient("localhost", 27017)

# Get list of databases on server
db_list = client.list_database_names()
print(db_list)

# Define database
db = client["Simulation"]
print(db)

# Define data collection within database
collection = db["Pedestrians"]

# Get current date and time
current = datetime.now()

# Define height
height = 1.65

# Parse time and date
time = current.strftime("%H:%M:%S")
date = current.strftime("%Y-%m-%d")
print(time)

# Generate pose in [x,y,z,phi,theta,psi] as numpy array and convert to list in reference to sensor network
pose = np.array([0,0,0,0,0,0]).tolist()
#pose = {'x': 0, 'y': 0, 'z': 0, 'phi': 0, 'theta': 0, 'psi': 0}

# Generate global coordinates
latitude = 53.55703
longitude = 10.02303
#coordinates = {'latitude': latitude, 'longitude': longitude}
coordinates = (latitude, longitude)

# Generate dimensions in [x,y,z]
dimensions = np.array([0,0,height]).tolist()
#dimensions = {'x': 0, 'y': 0, 'z': height}

# Generate data dictionary
post = {
    #"_id": bson.Binary.from_uuid(uuid.uuid4()),
    "_id": bson.ObjectId(),
    "class": "senior",
    "height": height,
    "pose": pose,
    "coordinates": coordinates,
    "dimensions": dimensions,
    "date": date,
    "time": time
}

print(sys.getsizeof(bson.ObjectId()))

# Post data (called documents) to database
collection.insert_one(post)


