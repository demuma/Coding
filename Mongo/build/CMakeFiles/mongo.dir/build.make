# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.30

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.30.0/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.30.0/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo"

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/build"

# Include any dependencies generated for this target.
include CMakeFiles/mongo.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/mongo.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/mongo.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/mongo.dir/flags.make

CMakeFiles/mongo.dir/mongo.cpp.o: CMakeFiles/mongo.dir/flags.make
CMakeFiles/mongo.dir/mongo.cpp.o: /Users/maxdemu/Documents/HAW\ Hamburg/Promotion/Coding/Mongo/mongo.cpp
CMakeFiles/mongo.dir/mongo.cpp.o: CMakeFiles/mongo.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir="/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/mongo.dir/mongo.cpp.o"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/mongo.dir/mongo.cpp.o -MF CMakeFiles/mongo.dir/mongo.cpp.o.d -o CMakeFiles/mongo.dir/mongo.cpp.o -c "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/mongo.cpp"

CMakeFiles/mongo.dir/mongo.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/mongo.dir/mongo.cpp.i"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/mongo.cpp" > CMakeFiles/mongo.dir/mongo.cpp.i

CMakeFiles/mongo.dir/mongo.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/mongo.dir/mongo.cpp.s"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/mongo.cpp" -o CMakeFiles/mongo.dir/mongo.cpp.s

# Object files for target mongo
mongo_OBJECTS = \
"CMakeFiles/mongo.dir/mongo.cpp.o"

# External object files for target mongo
mongo_EXTERNAL_OBJECTS =

mongo: CMakeFiles/mongo.dir/mongo.cpp.o
mongo: CMakeFiles/mongo.dir/build.make
mongo: /usr/local/lib/libmongocxx.3.10.1.dylib
mongo: /usr/local/lib/libbsoncxx.3.10.1.dylib
mongo: CMakeFiles/mongo.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir="/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable mongo"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/mongo.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/mongo.dir/build: mongo
.PHONY : CMakeFiles/mongo.dir/build

CMakeFiles/mongo.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/mongo.dir/cmake_clean.cmake
.PHONY : CMakeFiles/mongo.dir/clean

CMakeFiles/mongo.dir/depend:
	cd "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/build" && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/build" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/build" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Mongo/build/CMakeFiles/mongo.dir/DependInfo.cmake" "--color=$(COLOR)"
.PHONY : CMakeFiles/mongo.dir/depend

