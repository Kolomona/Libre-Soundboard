#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../src/AudioFile.h"

TEST_CASE("AudioFile load non-existent") {
    AudioFile f;
    REQUIRE(f.load("/no/such/path.wav") == false);
}
