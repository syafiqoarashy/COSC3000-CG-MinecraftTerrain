cmake_minimum_required(VERSION 3.10)
project(MinecraftTerrain)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(include)
include_directories(lib/glfw/include)
include_directories(lib/glad/include)
include_directories(include/glm)

# Source files
file(GLOB SOURCES "src/*.cpp" "lib/glad/glad.c")

# Add the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link GLFW and other libraries
find_package(OpenGL REQUIRED)
add_subdirectory(lib/glfw)

target_link_libraries(${PROJECT_NAME} glfw ${GLFW_LIBRARIES} OpenGL::GL)
