# Install guide for Linux

## Install dependencies (CMake 3.31.6, SFML 2.5)
- sudo apt-get install cmake uuid uuid-dev libssl-dev libsfml-dev libyaml-cpp-dev

## Install MongoDB C++-driver 3.10.1
- curl -OL https://github.com/mongodb/mongo-cxx-driver/releases/download/r3.10.1/mongo-cxx-driver-r3.10.1.tar.gz 
- tar -xzf mongo-cxx-driver-r3.10.1.tar.gz 
- cd mongo-cxx-driver-r3.10.1/build 

## Configure and build driver (in /usr/local)
- cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=/usr/local -DBSONCXX_POLY_USE_IMPLS=ON
- cmake --build .
- sudo cmake --build . --target install

## Build project
- cd ./SimulatorV2
- cmake -S . -B build
- cmake --build build -j4

## Start simulator
- ./build/Simulator

**NOTE:** Comment Line 26 and uncomment Line 27 in CMakeLists.txt for Linux.

## Install mongoDB server on Ubuntu 22.04
- curl -fsSL https://www.mongodb.org/static/pgp/server-8.0.asc | sudo gpg -o /usr/share/keyrings/mongodb-server-8.0.gpg --dearmor
- echo "deb [ arch=amd64,arm64 signed-by=/usr/share/keyrings/mongodb-server-8.0.gpg ] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/8.0 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-8.0.list
- sudo apt-get update
- sudo apt-get install -y mongodb-org
- sudo systemctl start mongod.service
- sudo chown -R mongodb:mongodb /var/lib/mongodb
- sudo chown mongodb:mongodb /tmp/mongodb-27017.sock
- sudo service mongod restart

# Install guide for macOS 15.4

## Download and install CMake 3.31.6
- cd ~/Downloads
- curl -L -O https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-macos-universal.tar.gz
- tar xvf cmake-3.31.6-macos-universal.tar.gz
- cd cmake-3.31.6-macos-universal
- mv CMake.app /Applications/
- sudo "/Applications/CMake.app/Contents/bin/cmake-gui" --install=/usr/local/bin (Intel Chip)
- sudo "/Applications/CMake.app/Contents/bin/cmake-gui" --install=/usr/local (M Chip)

## Download and install MongoDB C++-driver 3.10.1
- curl -OL https://github.com/mongodb/mongo-cxx-driver/releases/download/r3.10.1/mongo-cxx-driver-r3.10.1.tar.gz
- Alternatively download patched version (libbson/macOS 15.4 SDK bug)
- tar xvf mongo-cxx-driver-r3.10.1.tar.gz
- cd mongo-cxx-driver-r3.10.1
- mkdir build
- cd build
- cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=/usr/local -DBSONCXX_POLY_USE_IMPLS=ON
- cmake --build .
- sudo cmake --build . --target install
- brew update
- brew tap mongodb/brew
- brew install mongodb-community
- brew services start mongodb/brew/mongodb-community

## Install SFML 3.0.0
brew install sfml@3

## Build project
- cd ./SimulatorV3
- cmake -S . -B build
- cmake --build build -j$(sysctl -n hw.ncpu)
