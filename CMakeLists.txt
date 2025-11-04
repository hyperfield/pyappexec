cmake_minimum_required(VERSION 3.16)

project(pyappexec LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)

find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

set(RESOURCE_XML ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.xml)
set(GENERATED_RESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/resources.c)

add_custom_command(
    OUTPUT ${GENERATED_RESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES}
            --target=${GENERATED_RESOURCE_C}
            --generate-source
            resources.xml
    DEPENDS ${RESOURCE_XML} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/get_python_version.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources
    COMMENT "Generating GResource source from ${RESOURCE_XML}"
)

set_source_files_properties(${GENERATED_RESOURCE_C} PROPERTIES GENERATED TRUE)

set(SOURCES
    main.cpp
    lib/AppBootstrapper.cpp
    lib/PythonManager.cpp
    lib/SpecConfig.cpp
    lib/Utils.cpp
    gui/MainWindow.cpp
    gui/GuiRunner.cpp
    include/gui/MainWindow.hpp
    ${GENERATED_RESOURCE_C}
)

add_executable(pyappexec ${SOURCES})

target_include_directories(pyappexec
    PRIVATE
        include
)

target_link_libraries(pyappexec
    PRIVATE
        PkgConfig::GIO
        INIReader
        Qt6::Widgets
)
