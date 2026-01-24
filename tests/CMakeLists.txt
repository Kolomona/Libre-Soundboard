add_executable(tests_basic
    test_basic.cpp
)

target_link_libraries(tests_basic PRIVATE Catch2::Catch2)
add_test(NAME basic_tests COMMAND tests_basic)

add_executable(tests_audiofile
    test_audiofile.cpp
)
target_link_libraries(tests_audiofile PRIVATE Catch2::Catch2 libresoundboard_core)
add_test(NAME audiofile_tests COMMAND tests_audiofile)

add_executable(tests_waveform_pyramid
    ../tests/test_waveform_pyramid.cpp
)
target_link_libraries(tests_waveform_pyramid PRIVATE Catch2::Catch2 libresoundboard_core)
add_test(NAME waveform_pyramid_tests COMMAND tests_waveform_pyramid)

add_executable(tests_waveform_renderer
    ../tests/test_waveform_renderer.cpp
)
target_link_libraries(tests_waveform_renderer PRIVATE Catch2::Catch2 libresoundboard_core)
add_test(NAME waveform_renderer_tests COMMAND tests_waveform_renderer)

add_executable(tests_waveform_worker
    ../tests/test_waveform_worker.cpp
)
target_link_libraries(tests_waveform_worker PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Core)
add_test(NAME waveform_worker_tests COMMAND tests_waveform_worker)

add_executable(tests_soundcontainer_cache_flow
    ../tests/test_soundcontainer_cache_flow.cpp
)
target_link_libraries(tests_soundcontainer_cache_flow PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Widgets Qt6::Gui Qt6::Core)
add_test(NAME soundcontainer_cache_flow_tests COMMAND tests_soundcontainer_cache_flow)

add_executable(tests_playhead_manager
    ../tests/test_playhead_manager.cpp
)
target_link_libraries(tests_playhead_manager PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Widgets Qt6::Gui Qt6::Core)
add_test(NAME playhead_manager_tests COMMAND tests_playhead_manager)

add_executable(tests_waveform_cache_eviction
    ../tests/test_waveform_cache_eviction.cpp
)
target_link_libraries(tests_waveform_cache_eviction PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Core Qt6::Gui)
add_test(NAME waveform_cache_eviction_tests COMMAND tests_waveform_cache_eviction)

add_executable(tests_keepalive_monitor
    ../tests/test_keepalive_monitor.cpp
)
target_link_libraries(tests_keepalive_monitor PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Core)
add_test(NAME keepalive_monitor_tests COMMAND tests_keepalive_monitor)
add_executable(tests_audioengine_input
    ../tests/test_audioengine_input.cpp
)
target_link_libraries(tests_audioengine_input PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Core)
add_test(NAME audioengine_input_tests COMMAND tests_audioengine_input)

add_executable(tests_mainwindow_keepalive
    ../tests/test_mainwindow_keepalive.cpp
)
target_link_libraries(tests_mainwindow_keepalive PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Core Qt6::Widgets Qt6::Gui)
add_test(NAME mainwindow_keepalive_tests COMMAND tests_mainwindow_keepalive)

add_executable(tests_audioengine_connections
    ../tests/test_audioengine_connections.cpp
)
target_link_libraries(tests_audioengine_connections PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Core)
add_test(NAME audioengine_connections_tests COMMAND tests_audioengine_connections)

add_executable(tests_preferences_dialog
    ../tests/test_preferences_dialog.cpp
)
target_link_libraries(tests_preferences_dialog PRIVATE Catch2::Catch2 libresoundboard_core Qt6::Core Qt6::Widgets Qt6::Gui)
add_test(NAME preferences_dialog_tests COMMAND tests_preferences_dialog)
