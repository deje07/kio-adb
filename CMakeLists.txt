cmake_minimum_required(VERSION 3.22)
project(kio-adb VERSION 0.1.0 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
include(KDEInstallDirs6)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(KF6KIO 6.0 REQUIRED)

add_library(kio_adb MODULE adbworker.cpp)
set_target_properties(kio_adb PROPERTIES AUTOMOC ON PREFIX "" OUTPUT_NAME adb)
target_compile_features(kio_adb PRIVATE cxx_std_17)
target_link_libraries(kio_adb PRIVATE Qt6::Core KF6::KIOCore)
install(TARGETS kio_adb DESTINATION "${KDE_INSTALL_PLUGINDIR}/kf6/kio")
