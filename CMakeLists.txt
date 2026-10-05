cmake_minimum_required(VERSION 3.24)

project(
    QuestHUDTracker
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonLibSSE CONFIG REQUIRED)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR "QuestHUDTracker"
    USE_ADDRESS_LIBRARY
    SOURCES
        src/main.cpp
        src/Tracker.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
