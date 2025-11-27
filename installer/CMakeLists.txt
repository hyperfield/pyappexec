set(INSTALLER_SOURCES
    src/InstallerApp.cpp
    src/InstallerWindow.cpp
    src/ProjectInspector.cpp
    src/IniTemplate.cpp
    src/BinaryPackager.cpp
    src/SettingsModel.cpp
    src/UiWidgets.cpp
    src/main_installer.cpp
    ${CMAKE_SOURCE_DIR}/installer/include/installer/InstallerWindow.hpp
    ${CMAKE_SOURCE_DIR}/resources/qt_resources.qrc
)

# Ensure the Windows icon resource is embedded, mirroring the launcher binary.
if(WIN32)
    set(_installer_icon_rc "${CMAKE_BINARY_DIR}/appicon.rc")
    if(NOT EXISTS "${_installer_icon_rc}")
        configure_file(${CMAKE_SOURCE_DIR}/resources/win/appicon.rc.in "${_installer_icon_rc}" @ONLY)
    endif()
    list(APPEND INSTALLER_SOURCES "${_installer_icon_rc}")
endif()

add_executable(pyappexec_installer ${INSTALLER_SOURCES})
set_common_runtime_output(pyappexec_installer)

# Hide the console window on Windows builds.
if(WIN32)
    set_target_properties(pyappexec_installer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(pyappexec_installer
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/installer/include
)

target_link_libraries(pyappexec_installer
    PRIVATE
        Qt6::Widgets
)
