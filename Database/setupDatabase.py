import pymongo
from pymongo import MongoClient

# Connect client to MongoDB server
client = MongoClient("localhost", 27017)
db = client["Simulation"]

db.create_collection("Pedestrians")

db.create_collection(
    "name",
    timeseries= {
        "timeField": "timestamp",
        "metaField": "metadata",
        "granularity": "seconds"
    }
)