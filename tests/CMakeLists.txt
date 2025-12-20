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
