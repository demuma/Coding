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
CMAKE_SOURCE_DIR = "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database"

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/build"

# Include any dependencies generated for this target.
include CMakeFiles/Simulator.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/Simulator.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/Simulator.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/Simulator.dir/flags.make

CMakeFiles/Simulator.dir/Main.cpp.o: CMakeFiles/Simulator.dir/flags.make
CMakeFiles/Simulator.dir/Main.cpp.o: /Users/maxdemu/Documents/HAW\ Hamburg/Promotion/Coding/Renderer_database/Main.cpp
CMakeFiles/Simulator.dir/Main.cpp.o: CMakeFiles/Simulator.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir="/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/Simulator.dir/Main.cpp.o"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/Simulator.dir/Main.cpp.o -MF CMakeFiles/Simulator.dir/Main.cpp.o.d -o CMakeFiles/Simulator.dir/Main.cpp.o -c "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/Main.cpp"

CMakeFiles/Simulator.dir/Main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/Simulator.dir/Main.cpp.i"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/Main.cpp" > CMakeFiles/Simulator.dir/Main.cpp.i

CMakeFiles/Simulator.dir/Main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/Simulator.dir/Main.cpp.s"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/Main.cpp" -o CMakeFiles/Simulator.dir/Main.cpp.s

# Object files for target Simulator
Simulator_OBJECTS = \
"CMakeFiles/Simulator.dir/Main.cpp.o"

# External object files for target Simulator
Simulator_EXTERNAL_OBJECTS =

Simulator: CMakeFiles/Simulator.dir/Main.cpp.o
Simulator: CMakeFiles/Simulator.dir/build.make
Simulator: /usr/local/lib/libsfml-graphics.2.6.1.dylib
Simulator: /usr/local/lib/libsfml-window.2.6.1.dylib
Simulator: /usr/local/lib/libsfml-system.2.6.1.dylib
Simulator: /usr/local/lib/libyaml-cpp.0.8.0.dylib
Simulator: /usr/local/lib/libmongocxx.3.10.1.dylib
Simulator: /usr/local/lib/libbsoncxx.3.10.1.dylib
Simulator: CMakeFiles/Simulator.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir="/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable Simulator"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/Simulator.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/Simulator.dir/build: Simulator
.PHONY : CMakeFiles/Simulator.dir/build

CMakeFiles/Simulator.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/Simulator.dir/cmake_clean.cmake
.PHONY : CMakeFiles/Simulator.dir/clean

CMakeFiles/Simulator.dir/depend:
	cd "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/build" && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/build" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/build" "/Users/maxdemu/Documents/HAW Hamburg/Promotion/Coding/Renderer_database/build/CMakeFiles/Simulator.dir/DependInfo.cmake" "--color=$(COLOR)"
.PHONY : CMakeFiles/Simulator.dir/depend
