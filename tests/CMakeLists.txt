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
