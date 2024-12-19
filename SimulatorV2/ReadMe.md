# Install guide for Linux

## Install dependencies
sudo apt-get install cmake uuid uuid-dev libssl-dev libsfml-dev libyaml-cpp-dev

## Install MongoDB C++-driver
curl -OL https://github.com/mongodb/mongo-cxx-driver/releases/download/r4.0.0/mongo-cxx-driver-r4.0.0.tar.gz \
tar -xzf mongo-cxx-driver-r4.0.0.tar.gz \
cd mongo-cxx-driver-r4.0.0/build 

## Configure and build driver (in /usr/local)
cmake .. \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_CXX_STANDARD=17 \
cmake --build . \
sudo cmake --build . --target install

## Build project
cd ./SimulatorV2 \
cmake -S . -B build \
cmake --build build -j4 

## Start simulator
./build/Simulator

**NOTE:** Comment Line 26 and uncomment Line 27 in CMakeLists.txt for Linux.

## Install mongoDB server on Ubuntu 22.04
curl -fsSL https://www.mongodb.org/static/pgp/server-8.0.asc | \
   sudo gpg -o /usr/share/keyrings/mongodb-server-8.0.gpg \
   --dearmor \
echo "deb [ arch=amd64,arm64 signed-by=/usr/share/keyrings/mongodb-server-8.0.gpg ] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/8.0 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-8.0.list \
sudo apt-get update
sudo apt-get install -y mongodb-org
