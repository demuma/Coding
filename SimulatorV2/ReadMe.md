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
./build/Simulator \

**NOTE:** Comment Line 26 and uncomment Line 27 in CMakeLists.txt for Linux.
