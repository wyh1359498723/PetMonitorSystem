cmake_minimum_required(VERSION 3.16)
project(PetMonitorSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- Qt ----
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Charts)

# ---- OpenCV ----
# 指定 OpenCV 安装路径 (预编译 Windows 版本)
set(OpenCV_DIR "D:/opencv/opencv/build/x64/vc16/lib")
find_package(OpenCV REQUIRED)

# ---- Sources ----
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/VideoPlayer.cpp
    src/VideoWidget.cpp
    src/YoloDetector.cpp
    src/PetTracker.cpp
    src/BehaviorAnalyzer.cpp
    src/TrajectoryWidget.cpp
    src/StatisticsWidget.cpp
    src/AlertManager.cpp
    src/FrameProcessor.cpp
)

set(HEADERS
    src/MainWindow.h
    src/VideoPlayer.h
    src/VideoWidget.h
    src/YoloDetector.h
    src/PetTracker.h
    src/BehaviorAnalyzer.h
    src/TrajectoryWidget.h
    src/StatisticsWidget.h
    src/AlertManager.h
    src/DetectionResult.h
    src/FrameProcessor.h
)

set(RESOURCES
    resources/resources.qrc
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    ${OpenCV_LIBS}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
