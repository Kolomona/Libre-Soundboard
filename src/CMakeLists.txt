# Add icons resource
qt_add_resources(APP_ICONS ../resources/icons/icons.qrc)
add_library(libresoundboard_core STATIC
    MainWindow.cpp
    MainWindow.h
    SoundContainer.cpp
    SoundContainer.h
    AudioEngine.cpp
    AudioEngine.h
    AudioEnginePlay.cpp
    AudioEnginePlay.h
    WaveformWidget.cpp
    WaveformWidget.h
    WaveformWorker.cpp
    WaveformWorker.h
    WaveformPyramid.cpp
    WaveformPyramid.h
    AudioFile.cpp
    AudioFile.h
    SingleInstance.cpp
    SingleInstance.h
    CustomTabBar.cpp
    CustomTabBar.h
    CustomTabWidget.h
)

# Qt
find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(Qt6 COMPONENTS Core REQUIRED)
find_package(Qt6 COMPONENTS Network REQUIRED)
target_link_libraries(libresoundboard_core PUBLIC Qt6::Widgets)
target_link_libraries(libresoundboard_core PUBLIC Qt6::Network)

# Public include directory for headers (build and install)
target_include_directories(libresoundboard_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/libresoundboard>
)

# JACK (prefer CMake target, otherwise use pkg-config variables)
find_package(JACK)
if (JACK_FOUND)
    target_link_libraries(libresoundboard_core PUBLIC JACK::jack)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JACK REQUIRED jack)
    target_link_libraries(libresoundboard_core PUBLIC ${JACK_LIBRARIES})
    target_include_directories(libresoundboard_core PUBLIC ${JACK_INCLUDE_DIRS})
endif()

# libsndfile
find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)
target_link_libraries(libresoundboard_core PUBLIC ${SNDFILE_LIBRARIES})
target_include_directories(libresoundboard_core PUBLIC ${SNDFILE_INCLUDE_DIRS})

# libmpg123
pkg_check_modules(MPG123 REQUIRED libmpg123)
target_link_libraries(libresoundboard_core PUBLIC ${MPG123_LIBRARIES})
target_include_directories(libresoundboard_core PUBLIC ${MPG123_INCLUDE_DIRS})

# libsamplerate
pkg_check_modules(SAMPLERATE REQUIRED samplerate)
target_link_libraries(libresoundboard_core PUBLIC ${SAMPLERATE_LIBRARIES})
target_include_directories(libresoundboard_core PUBLIC ${SAMPLERATE_INCLUDE_DIRS})

add_executable(libresoundboard main.cpp ${APP_ICONS})
target_link_libraries(libresoundboard PRIVATE libresoundboard_core Qt6::Core)

# Install rules
install(TARGETS libresoundboard libresoundboard_core
    EXPORT libresoundboardTargets
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    COMPONENT runtime
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/libresoundboard
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT libresoundboardTargets
    FILE libresoundboard-core-config.cmake
    NAMESPACE LibreSoundboard::
    DESTINATION lib/cmake/libresoundboard
)
