if(NOT DEFINED CMAKE_POLICY_DEFAULT_CMP0167)
    set(CMAKE_POLICY_DEFAULT_CMP0167 OLD)
endif()

cmake_minimum_required(VERSION 3.16)

set(_PYAPPEXEC_VERSION_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/app_version.hpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_PYAPPEXEC_VERSION_HEADER}")
file(READ "${_PYAPPEXEC_VERSION_HEADER}" _PYAPPEXEC_VERSION_HEADER_CONTENTS)
string(REGEX MATCH "kAppVersion[^\"\\n]*\"([^\"]+)\"" _PYAPPEXEC_VERSION_MATCH "${_PYAPPEXEC_VERSION_HEADER_CONTENTS}")
if(NOT _PYAPPEXEC_VERSION_MATCH)
    message(FATAL_ERROR "Unable to extract kAppVersion from ${_PYAPPEXEC_VERSION_HEADER}")
endif()
set(PYAPPEXEC_APP_VERSION "${CMAKE_MATCH_1}")

project(pyappexec VERSION ${PYAPPEXEC_APP_VERSION} LANGUAGES C CXX)

option(BUILD_LAUNCHER "Build the PyAppExec launcher" ON)
option(BUILD_INSTALLER "Build the PyAppExec installer" ON)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

# vcpkg/Qt discovery convenience so developers can just point at a toolchain or VCPKG_ROOT
set(_PYAPPEXEC_VCPKG_ROOT "")
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    get_filename_component(_pyappexec_toolchain_dir "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
    get_filename_component(_pyappexec_scripts_dir "${_pyappexec_toolchain_dir}" DIRECTORY)
    get_filename_component(_pyappexec_candidate_root "${_pyappexec_scripts_dir}" DIRECTORY)
    if(EXISTS "${_pyappexec_candidate_root}/installed")
        set(_PYAPPEXEC_VCPKG_ROOT "${_pyappexec_candidate_root}")
    endif()
endif()
if(NOT _PYAPPEXEC_VCPKG_ROOT AND DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}")
    set(_PYAPPEXEC_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
elseif(NOT _PYAPPEXEC_VCPKG_ROOT AND DEFINED VCPKG_ROOT AND EXISTS "${VCPKG_ROOT}")
    set(_PYAPPEXEC_VCPKG_ROOT "${VCPKG_ROOT}")
endif()
if(_PYAPPEXEC_VCPKG_ROOT)
    file(TO_CMAKE_PATH "${_PYAPPEXEC_VCPKG_ROOT}" _PYAPPEXEC_VCPKG_ROOT)
    if(NOT DEFINED ENV{VCPKG_ROOT})
        set(ENV{VCPKG_ROOT} "${_PYAPPEXEC_VCPKG_ROOT}")
    endif()
endif()

if(DEFINED VCPKG_TARGET_TRIPLET)
    set(_PYAPPEXEC_VCPKG_TRIPLET "${VCPKG_TARGET_TRIPLET}")
elseif(WIN32)
    set(_PYAPPEXEC_VCPKG_TRIPLET "x64-windows")
endif()

set(_PYAPPEXEC_QT6_DIR "")
set(_PYAPPEXEC_QT6_PLUGINS_DIR "")
if(_PYAPPEXEC_VCPKG_ROOT AND _PYAPPEXEC_VCPKG_TRIPLET)
    set(_PYAPPEXEC_QT6_DIR "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/share/Qt6")
    list(APPEND CMAKE_PREFIX_PATH
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}"
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/share"
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/share/qt"
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/share/Qt"
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/share/qt6"
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/share/Qt6"
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/share/qtbase"
    )
    list(APPEND CMAKE_PROGRAM_PATH
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/tools"
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/tools/pkgconf"
        "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/tools/glib"
    )
    list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    list(REMOVE_DUPLICATES CMAKE_PROGRAM_PATH)
endif()
if(_PYAPPEXEC_QT6_DIR AND EXISTS "${_PYAPPEXEC_QT6_DIR}")
    set(Qt6_DIR "${_PYAPPEXEC_QT6_DIR}" CACHE PATH "Qt6 package directory" FORCE)
endif()
if(Qt6_DIR)
    get_filename_component(_PYAPPEXEC_QT6_PREFIX "${Qt6_DIR}/../.." ABSOLUTE)
    if(EXISTS "${_PYAPPEXEC_QT6_PREFIX}/Qt6/plugins")
        set(_PYAPPEXEC_QT6_PLUGINS_DIR "${_PYAPPEXEC_QT6_PREFIX}/Qt6/plugins")
    endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json for tooling" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

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
    set(QT_MIN_VERSION 6.0)
    find_package(Qt6 ${QT_MIN_VERSION} COMPONENTS Widgets QUIET)
    if(NOT Qt6_FOUND)
        message(FATAL_ERROR
            "Qt6 was not found. Ensure it is installed (e.g. vcpkg install qtbase:${_PYAPPEXEC_VCPKG_TRIPLET}) "
            "or set Qt6_DIR/CMAKE_PREFIX_PATH to its CMake package directory.")
    endif()
endif()

if(BUILD_LAUNCHER)
    find_package(Boost REQUIRED COMPONENTS filesystem)
    find_package(spdlog CONFIG REQUIRED)

    if(NOT PKG_CONFIG_EXECUTABLE)
        set(_PYAPPEXEC_PKGCONF_HINTS)
        if(_PYAPPEXEC_VCPKG_ROOT AND _PYAPPEXEC_VCPKG_TRIPLET)
            list(APPEND _PYAPPEXEC_PKGCONF_HINTS
                "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/tools/pkgconf"
                "${_PYAPPEXEC_VCPKG_ROOT}/downloads/tools/pkgconf/${_PYAPPEXEC_VCPKG_TRIPLET}"
            )
        endif()
        find_program(PKG_CONFIG_EXECUTABLE
            NAMES pkgconf pkg-config
            HINTS ${_PYAPPEXEC_PKGCONF_HINTS}
        )
    endif()

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)

    set(RESOURCE_XML ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.xml)
    set(GENERATED_RESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/resources.c)

    set(_PYAPPEXEC_RESOURCE_SOURCE "${GENERATED_RESOURCE_C}")
    set(_PYAPPEXEC_GLIB_TOOL_DIRS)
    if(_PYAPPEXEC_VCPKG_ROOT AND _PYAPPEXEC_VCPKG_TRIPLET)
        list(APPEND _PYAPPEXEC_GLIB_TOOL_DIRS "${_PYAPPEXEC_VCPKG_ROOT}/installed/${_PYAPPEXEC_VCPKG_TRIPLET}/tools/glib")
    endif()
    find_program(GLIB_COMPILE_RESOURCES glib-compile-resources HINTS ${_PYAPPEXEC_GLIB_TOOL_DIRS})

    if(GLIB_COMPILE_RESOURCES)
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
    else()
        message(STATUS "glib-compile-resources not found; using pre-generated resources/resources.c")
        set(_PYAPPEXEC_RESOURCE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.c")
    endif()

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
        ${_PYAPPEXEC_RESOURCE_SOURCE}
    )

    if(WIN32)
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
            Boost::boost
            Boost::filesystem
            spdlog::spdlog
    )

    if(WIN32 AND _PYAPPEXEC_QT6_PLUGINS_DIR)
        set(_PYAPPEXEC_QT_PLUGIN_PLATFORM "${_PYAPPEXEC_QT6_PLUGINS_DIR}/platforms/qwindows.dll")
        if(EXISTS "${_PYAPPEXEC_QT_PLUGIN_PLATFORM}")
            add_custom_command(TARGET pyappexec POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:pyappexec>/platforms"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_PYAPPEXEC_QT_PLUGIN_PLATFORM}" "$<TARGET_FILE_DIR:pyappexec>/platforms/qwindows.dll"
                COMMENT "Copying Qt platform plugin (qwindows.dll) next to the executable"
            )
        else()
            message(WARNING "Qt platform plugin not found at ${_PYAPPEXEC_QT_PLUGIN_PLATFORM}; GUI build may fail to run.")
        endif()
    endif()
endif()

if(BUILD_INSTALLER)
    add_subdirectory(installer)
endif()
