cmake_minimum_required(VERSION 3.16)

project(pyappexec LANGUAGES C CXX)

option(BUILD_LAUNCHER "Build the PyAppExec launcher" ON)
option(BUILD_INSTALLER "Build the PyAppExec installer" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json for tooling" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_compile_definitions(BOOST_PROCESS_VERSION=1)

function(set_common_runtime_output target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()
    if(CMAKE_CONFIGURATION_TYPES)
        foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${config}" CONFIG_UPPER)
            set_target_properties(${target_name}
                PROPERTIES
                    "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/${config}")
        endforeach()
    else()
        set_target_properties(${target_name}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    endif()
endfunction()

if(BUILD_LAUNCHER OR BUILD_INSTALLER)
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
endif()

if(BUILD_LAUNCHER)
    find_package(spdlog CONFIG REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
endif()

if(BUILD_LAUNCHER)
    find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
endif()

if(BUILD_LAUNCHER)
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
        lib/CliParser.cpp
        lib/ConfigLoader.cpp
        lib/Launcher.cpp
        lib/LoggerConfigurator.cpp
        lib/INIReader.cpp
        lib/AppBootstrapper.cpp
        lib/PythonManager.cpp
        lib/SpecConfig.cpp
        lib/Utils.cpp
        lib/Logger.cpp
        gui/MainWindow.cpp
        gui/GuiRunner.cpp
        include/gui/MainWindow.hpp
        resources/qt_resources.qrc
        ${GENERATED_RESOURCE_C}
    )

    if (WIN32)
        configure_file(${CMAKE_SOURCE_DIR}/resources/win/appicon.rc.in ${CMAKE_BINARY_DIR}/appicon.rc @ONLY)
        list(APPEND SOURCES ${CMAKE_BINARY_DIR}/appicon.rc)
    endif()

    add_executable(pyappexec ${SOURCES})
    set_common_runtime_output(pyappexec)

    target_include_directories(pyappexec
        PRIVATE
            include
    )

    target_link_libraries(pyappexec
        PRIVATE
            PkgConfig::GIO
            Qt6::Widgets
            spdlog::spdlog
    )
endif()

if(BUILD_INSTALLER)
    add_subdirectory(installer)
endif()
