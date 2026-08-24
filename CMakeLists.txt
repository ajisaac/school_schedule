cmake_minimum_required(VERSION 4.4)
project(school_schedule)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED)

add_executable(school_schedule MACOSX_BUNDLE main.cpp)
target_link_libraries(school_schedule
        Qt::Core
        Qt::Gui
        Qt::Widgets
)
