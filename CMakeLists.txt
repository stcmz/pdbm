# Requires cmake v3.20 for Boost 1.75.0
cmake_minimum_required(VERSION 3.20)
project(pdbm LANGUAGES CXX)

# Enforce C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Create the target and add source files
add_executable(${PROJECT_NAME}
  main.cpp
)

# Allow running without .dll or .so
set(Boost_USE_STATIC_LIBS TRUE)
find_package(Boost REQUIRED COMPONENTS program_options)

# Set include path for the target only
target_include_directories(${PROJECT_NAME} PRIVATE
  ${Boost_INCLUDE_DIRS}
)

# Set lib path for the target only
target_link_libraries(${PROJECT_NAME}
  Boost::program_options
)

# Enable cmake --install to copy the binary to system dir
install(
  TARGETS ${PROJECT_NAME}
)