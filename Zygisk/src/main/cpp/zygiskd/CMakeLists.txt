# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("zygiskd")

set(CMAKE_CXX_STANDARD 23)

# configure a header file to pass some of the CMake settings
# to the source code

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h

# add the executable

#include_directories(../common)


#aux_source_directory(. ZYGISKD_SRC_LIST)
#add_executable(zygiskd ${ZYGISKD_SRC_LIST})
add_executable(zygiskd  companion.cpp daemon_parse_args.cpp sqlite3.cpp zygiskd.cpp)
target_link_libraries(zygiskd log common)

