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

add_executable(pyappexec_installer ${INSTALLER_SOURCES})
set_common_runtime_output(pyappexec_installer)

target_include_directories(pyappexec_installer
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/installer/include
)

target_link_libraries(pyappexec_installer
    PRIVATE
        Qt6::Widgets
)
