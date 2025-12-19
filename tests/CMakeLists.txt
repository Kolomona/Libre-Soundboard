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
