cmake_minimum_required(VERSION 3.22)
project(kio-adb VERSION 0.4.0 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
include(KDEInstallDirs6)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(KF6KIO 6.0 REQUIRED)

add_library(kio_adb MODULE src/adbworker.cpp)
set_target_properties(kio_adb PROPERTIES AUTOMOC ON PREFIX "" OUTPUT_NAME adb)
target_compile_features(kio_adb PRIVATE cxx_std_17)
target_link_libraries(kio_adb PRIVATE Qt6::Core KF6::KIOCore)
install(TARGETS kio_adb DESTINATION "${KDE_INSTALL_PLUGINDIR}/kf6/kio")
install(FILES src/adb.desktop DESTINATION "${KDE_INSTALL_DATADIR}/remoteview")

option(KIO_ADB_EMULATOR_TEST "Enable the Android emulator integration test" OFF)
if(KIO_ADB_EMULATOR_TEST)
    enable_testing()
    set_target_properties(kio_adb PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins/kf6/kio"
    )
    add_executable(adb-test-action tests/actions.cpp)
    target_link_libraries(adb-test-action PRIVATE Qt6::Core KF6::KIOCore)
    add_dependencies(adb-test-action kio_adb)
    add_test(NAME adb-symlinks
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/symlinks.sh"
            "$<TARGET_FILE:adb-test-action>" "$<TARGET_FILE_DIR:kio_adb>/../.."
    )
    set_tests_properties(adb-symlinks PROPERTIES TIMEOUT 20)
    foreach(action IN ITEMS upload-multiple delete-multiple list-devices)
        add_test(NAME adb-${action}
            COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/multi-file.sh"
                "$<TARGET_FILE:adb-test-action>" "$<TARGET_FILE_DIR:kio_adb>/../.." "${action}"
        )
        set_tests_properties(adb-${action} PROPERTIES TIMEOUT 20)
    endforeach()
    foreach(action IN ITEMS download upload upload-space list list-directory mkdir rmdir)
        add_test(NAME adb-emulator-${action}
            COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/emulator.sh"
                "$<TARGET_FILE:adb-test-action>" "$<TARGET_FILE_DIR:kio_adb>/../.." "${action}"
        )
        set_tests_properties(adb-emulator-${action} PROPERTIES TIMEOUT 420 RUN_SERIAL TRUE)
    endforeach()
endif()
