cmake_minimum_required(VERSION 3.1.0)

project(matrix_widget)

find_package(Threads)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 14)

find_package(Qt5 COMPONENTS Widgets OpenGLExtensions REQUIRED)


add_executable(helloworld
    glwidget.cpp
    main.cpp
    window.cpp
    mainwindow.cpp
)

target_compile_options(helloworld PRIVATE -Werror -Wextra -Wall)

target_link_libraries(helloworld Qt5::Widgets Qt5::OpenGLExtensions ${CMAKE_THREAD_LIBS_INIT})
