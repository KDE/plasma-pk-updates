cmake_minimum_required(VERSION 3.0)

project(plasma-pk-updates VERSION 0.1.0)
add_definitions(-DPROJECT_VERSION="\\\"${PROJECT_VERSION}\\\"")

find_package(ECM 1.3.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
set(CMAKE_AUTOMOC ON)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMInstallIcons)
include(FeatureSummary)

find_package(Qt5 REQUIRED
    Core
    Gui
    Widgets
    Quick
    DBus
)

find_package(KF5 REQUIRED
    Plasma
    I18n
    CoreAddons # KFormat
    Config # KConfig
    KDELibs4Support #Solid::Power
)

find_package(packagekitqt5 REQUIRED)

add_definitions(
    -DQT_USE_FAST_CONCATENATION
    -DQT_USE_FAST_OPERATOR_PLUS
    -DQT_STRICT_ITERATORS
    -DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if(CMAKE_COMPILER_IS_GNUCXX)
  # more aggressive warnings and C++11 support
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall") 
endif()


add_subdirectory(src)

#install(DIRECTORY data/ DESTINATION ${DATA_INSTALL_DIR}/fedora-plasma-first-login)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
