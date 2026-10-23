#include "support/Fixture.hpp"

#include <doctest/doctest.h>

using namespace timetracker;
using test::Fixture;

TEST_CASE("a missing theme setting means system") {
    Fixture fixture;
    fixture.open();
    auto theme = fixture.settings.theme();
    REQUIRE(theme.succeeded());
    CHECK(theme.value() == ThemePreference::System);
}

TEST_CASE("the theme round-trips") {
    Fixture fixture;
    fixture.open();
    REQUIRE(fixture.settings.setTheme(ThemePreference::Dark).succeeded());
    auto dark = fixture.settings.theme();
    REQUIRE(dark.succeeded());
    CHECK(dark.value() == ThemePreference::Dark);

    REQUIRE(fixture.settings.setTheme(ThemePreference::Light).succeeded());
    auto light = fixture.settings.theme();
    REQUIRE(light.succeeded());
    CHECK(light.value() == ThemePreference::Light);

    REQUIRE(fixture.settings.setTheme(ThemePreference::System).succeeded());
    auto system = fixture.settings.theme();
    REQUIRE(system.succeeded());
    CHECK(system.value() == ThemePreference::System);
}

TEST_CASE("an unknown stored theme is an error") {
    Fixture fixture;
    fixture.open();
    REQUIRE(fixture.settingsRepo.set("theme", "blue").succeeded());
    auto theme = fixture.settings.theme();
    CHECK_FALSE(theme.succeeded());
    CHECK(theme.error() == "Unknown theme.");
}

TEST_CASE("an unknown setting key is rejected") {
    Fixture fixture;
    fixture.open();
    auto rejected = fixture.settingsRepo.set("nope", "dark");
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Unknown setting.");
}

TEST_CASE("an empty stored theme is an error") {
    Fixture fixture;
    fixture.open();
    REQUIRE(fixture.settingsRepo.set("theme", "").succeeded());
    auto theme = fixture.settings.theme();
    CHECK_FALSE(theme.succeeded());
    CHECK(theme.error() == "Unknown theme.");
}
