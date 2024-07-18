from meteostat import Point, Hourly, Daily
import pymongo as pm
import datetime as dt
import numpy as np

"""
Meteostat
"""

# Weather condition codes from meteostat
weather_conditions = [
    "",
    "Clear",
    "Fair",
    "Cloudy",
    "Overcast",
    "Fog",
    "Freezing Fog",
    "Light Rain",
    "Rain",
    "Heavy Rain",
    "Freezing Rain",
    "Heavy Freezing Rain",
    "Sleet",
    "Heavy Sleet",
    "Light Snowfall",
    "Snowfall",
    "Heavy Snowfall",
    "Rain Shower",
    "Heavy Rain Shower",
    "Sleet Shower",
    "Heavy Sleet Shower",
    "Snow Shower",
    "Heavy Snow Shower",
    "Lightning",
    "Hail",
    "Thunderstorm",
    "Heavy Thunderstorm",
    "Storm"
]

# Hamburg Airport Coordinates
latitude = 53.55703
longitude = 10.02303
altitude = 12
weather = {'time_utc': 0.0,
           'temperature': 0.0,
           'relative_humidity': 0.0,
           'wind_speed': 0.0,
           'air_pressure': 0.0,
           'weather_condition': ""
           }


# Fetch weather data from Meteostat
def get_current_weather():
    location = Point(latitude, longitude, altitude)
    current_date = dt.datetime.today()
    current_day = current_date.date()
    rounded_utc_hour = dt.time(dt.datetime.now(dt.timezone.utc).hour, 0, 0)
    date = dt.datetime.combine(current_day, rounded_utc_hour)
    data = Hourly(location, date, date)
    data = data.fetch()
    weather['time_utc'] = date
    weather['temperature'] = float(data.iloc[:, 0])  # °C
    weather['relative_humidity'] = float(data.iloc[:, 2])  # %
    weather['wind_speed'] = float(data.iloc[:, 6])  # km/h
    weather['air_pressure'] = float(data.iloc[:, 8])  # hPa
    weather['weather_condition'] = weather_conditions[int(data.iloc[:, 10])]

    return weather


def send_weather_data():
    client = pm.MongoClient("localhost", 27017)
    db = client["Weather"]
    collection = db["Current"]
    get_current_weather()
    post = {
        'time_utc': weather['time_utc'],
        'temperature': weather['temperature'],
        'relative_humidity': weather['relative_humidity'],
        'wind_speed': weather['wind_speed'],
        'air_pressure': weather['air_pressure'],
        'weather_condition': weather['weather_condition']
    }
    collection.insert_one(post)

    return 0


if __name__ == '__main__':
    send_weather_data()

