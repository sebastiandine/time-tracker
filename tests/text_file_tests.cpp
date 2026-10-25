#include "support/TempDir.hpp"

#include "timetracker/infra/HostTextFile.hpp"

#include <doctest/doctest.h>

using namespace timetracker;

TEST_CASE("replaceText swaps the destination and removes the temp file") {
    test::TempDir dir;
    const auto destination = dir.path / "config.json";
    REQUIRE(replaceHostText(destination, "old\n").succeeded());
    auto first = readHostText(destination);
    REQUIRE(first.succeeded());
    CHECK(first.value() == "old\n");

    REQUIRE(replaceHostText(destination, "new\n").succeeded());
    auto second = readHostText(destination);
    REQUIRE(second.succeeded());
    CHECK(second.value() == "new\n");
    CHECK_FALSE(hostPathExists(destination.generic_string() + ".tmp"));
    CHECK_FALSE(hostPathExists(dir.path / "config.json.tmp"));
}
