# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.21

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
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.21.3_1/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.21.3_1/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/luickklippel/Documents/Projekte/tempCacheDb

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/luickklippel/Documents/Projekte/tempCacheDb/build

# Include any dependencies generated for this target.
include test/CMakeFiles/testPullClient.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/CMakeFiles/testPullClient.dir/compiler_depend.make

# Include the progress variables for this target.
include test/CMakeFiles/testPullClient.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/testPullClient.dir/flags.make

test/CMakeFiles/testPullClient.dir/testPullClient.c.o: test/CMakeFiles/testPullClient.dir/flags.make
test/CMakeFiles/testPullClient.dir/testPullClient.c.o: ../test/testPullClient.c
test/CMakeFiles/testPullClient.dir/testPullClient.c.o: test/CMakeFiles/testPullClient.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/luickklippel/Documents/Projekte/tempCacheDb/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object test/CMakeFiles/testPullClient.dir/testPullClient.c.o"
	cd /Users/luickklippel/Documents/Projekte/tempCacheDb/build/test && /Library/Developer/CommandLineTools/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT test/CMakeFiles/testPullClient.dir/testPullClient.c.o -MF CMakeFiles/testPullClient.dir/testPullClient.c.o.d -o CMakeFiles/testPullClient.dir/testPullClient.c.o -c /Users/luickklippel/Documents/Projekte/tempCacheDb/test/testPullClient.c

test/CMakeFiles/testPullClient.dir/testPullClient.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/testPullClient.dir/testPullClient.c.i"
	cd /Users/luickklippel/Documents/Projekte/tempCacheDb/build/test && /Library/Developer/CommandLineTools/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/luickklippel/Documents/Projekte/tempCacheDb/test/testPullClient.c > CMakeFiles/testPullClient.dir/testPullClient.c.i

test/CMakeFiles/testPullClient.dir/testPullClient.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/testPullClient.dir/testPullClient.c.s"
	cd /Users/luickklippel/Documents/Projekte/tempCacheDb/build/test && /Library/Developer/CommandLineTools/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/luickklippel/Documents/Projekte/tempCacheDb/test/testPullClient.c -o CMakeFiles/testPullClient.dir/testPullClient.c.s

# Object files for target testPullClient
testPullClient_OBJECTS = \
"CMakeFiles/testPullClient.dir/testPullClient.c.o"

# External object files for target testPullClient
testPullClient_EXTERNAL_OBJECTS =

test/testPullClient: test/CMakeFiles/testPullClient.dir/testPullClient.c.o
test/testPullClient: test/CMakeFiles/testPullClient.dir/build.make
test/testPullClient: libtempCacheDb.a
test/testPullClient: test/CMakeFiles/testPullClient.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/luickklippel/Documents/Projekte/tempCacheDb/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable testPullClient"
	cd /Users/luickklippel/Documents/Projekte/tempCacheDb/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/testPullClient.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/testPullClient.dir/build: test/testPullClient
.PHONY : test/CMakeFiles/testPullClient.dir/build

test/CMakeFiles/testPullClient.dir/clean:
	cd /Users/luickklippel/Documents/Projekte/tempCacheDb/build/test && $(CMAKE_COMMAND) -P CMakeFiles/testPullClient.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/testPullClient.dir/clean

test/CMakeFiles/testPullClient.dir/depend:
	cd /Users/luickklippel/Documents/Projekte/tempCacheDb/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/luickklippel/Documents/Projekte/tempCacheDb /Users/luickklippel/Documents/Projekte/tempCacheDb/test /Users/luickklippel/Documents/Projekte/tempCacheDb/build /Users/luickklippel/Documents/Projekte/tempCacheDb/build/test /Users/luickklippel/Documents/Projekte/tempCacheDb/build/test/CMakeFiles/testPullClient.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/testPullClient.dir/depend

