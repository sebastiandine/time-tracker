#include "support/Fixture.hpp"

#include <doctest/doctest.h>

#include <string>

using namespace timetracker;
using test::Fixture;

TEST_CASE("create category trims the name and notes") {
    Fixture fixture;
    fixture.open();
    auto created = fixture.catalog.createCategory("  Work  ", "  hello  ");
    REQUIRE(created.succeeded());
    CHECK(created.value().name == "Work");
    CHECK(created.value().notes == "hello");
    CHECK(created.value().id > 0);
}

TEST_CASE("internal spaces in a name are kept") {
    Fixture fixture;
    fixture.open();
    auto created = fixture.catalog.createCategory("Design API", "a  b");
    REQUIRE(created.succeeded());
    CHECK(created.value().name == "Design API");
    CHECK(created.value().notes == "a  b");
}

TEST_CASE("an empty category name is rejected") {
    Fixture fixture;
    fixture.open();
    auto created = fixture.catalog.createCategory("   ", "notes");
    CHECK_FALSE(created.succeeded());
    CHECK(created.error() == "Name is empty.");
}

TEST_CASE("a name of 200 code points is stored and 201 is rejected") {
    Fixture fixture;
    fixture.open();
    const std::string ok(200, 'a');
    const std::string tooLong(201, 'b');
    REQUIRE(fixture.catalog.createCategory(ok, "").succeeded());
    auto rejected = fixture.catalog.createCategory(tooLong, "");
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Name is too long.");
}

TEST_CASE("notes of 20000 code points are stored and 20001 are rejected") {
    Fixture fixture;
    fixture.open();
    REQUIRE(fixture.catalog.createCategory("Work", std::string(20000, 'n')).succeeded());
    auto rejected = fixture.catalog.createCategory("Other", std::string(20001, 'n'));
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Notes are too long.");
}

TEST_CASE("trimming happens before the note length check") {
    Fixture fixture;
    fixture.open();
    auto created = fixture.catalog.createCategory("Work", " " + std::string(20000, 'n') + " ");
    REQUIRE(created.succeeded());
    CHECK(created.value().notes.size() == 20000);
}

TEST_CASE("duplicate category names are rejected ignoring ASCII case") {
    Fixture fixture;
    fixture.open();
    REQUIRE(fixture.catalog.createCategory("Work", "").succeeded());
    auto duplicate = fixture.catalog.createCategory("work", "");
    CHECK_FALSE(duplicate.succeeded());
    CHECK(duplicate.error() == "A category with that name already exists.");
}

TEST_CASE("categories sort without regard to ASCII case") {
    Fixture fixture;
    fixture.open();
    REQUIRE(fixture.catalog.createCategory("b", "").succeeded());
    REQUIRE(fixture.catalog.createCategory("C", "").succeeded());
    auto listed = fixture.catalog.loadAll();
    REQUIRE(listed.succeeded());
    REQUIRE(listed.value().categories.size() == 2);
    CHECK(listed.value().categories[0].name == "b");
    CHECK(listed.value().categories[1].name == "C");
}

TEST_CASE("a category note can hold text that looks like SQL") {
    Fixture fixture;
    fixture.open();
    const std::string notes = "'; DROP TABLE Category; --";
    auto created = fixture.catalog.createCategory("Safe", notes);
    REQUIRE(created.succeeded());
    auto listed = fixture.catalog.loadAll();
    REQUIRE(listed.succeeded());
    REQUIRE(listed.value().categories.size() == 1);
    CHECK(listed.value().categories[0].notes == notes);
}

TEST_CASE("a category with a main topic cannot be deleted") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto rejected = fixture.catalog.deleteCategory(seeded.category.id);
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Delete the main topics in this category first.");
}

TEST_CASE("an empty category can be deleted") {
    Fixture fixture;
    fixture.open();
    auto created = fixture.catalog.createCategory("Work", "");
    REQUIRE(created.succeeded());
    REQUIRE(fixture.catalog.deleteCategory(created.value().id).succeeded());
    auto missing = fixture.catalog.updateCategory(created.value().id, "Work", "");
    CHECK_FALSE(missing.succeeded());
    CHECK(missing.error() == "Category not found.");
}

TEST_CASE("a missing category cannot be deleted") {
    Fixture fixture;
    fixture.open();
    auto rejected = fixture.catalog.deleteCategory(404);
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Category not found.");
}

TEST_CASE("a main topic requires a category") {
    Fixture fixture;
    fixture.open();
    auto rejected = fixture.catalog.createMainTopic(404, "Design", "");
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Category not found.");
}

TEST_CASE("the same main topic name is allowed in another category") {
    Fixture fixture;
    fixture.open();
    auto first = fixture.catalog.createCategory("Work", "");
    auto second = fixture.catalog.createCategory("Personal", "");
    REQUIRE(first.succeeded());
    REQUIRE(second.succeeded());
    REQUIRE(fixture.catalog.createMainTopic(first.value().id, "Plan", "").succeeded());
    CHECK(fixture.catalog.createMainTopic(second.value().id, "Plan", "").succeeded());
    auto duplicate = fixture.catalog.createMainTopic(first.value().id, "plan", "");
    CHECK_FALSE(duplicate.succeeded());
    CHECK(duplicate.error() == "A main topic with that name already exists.");
}

TEST_CASE("a main topic with a subtopic cannot be deleted") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto rejected = fixture.catalog.deleteMainTopic(seeded.mainTopic.id);
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Delete the subtopics in this topic first.");
}

TEST_CASE("a main topic with a time entry cannot be deleted") {
    Fixture fixture;
    fixture.open();
    auto category = fixture.catalog.createCategory("Work", "");
    REQUIRE(category.succeeded());
    auto mainTopic = fixture.catalog.createMainTopic(category.value().id, "Design", "");
    REQUIRE(mainTopic.succeeded());
    auto started = fixture.tracking.start(TimeTarget{TimeTargetKind::MainTopic, mainTopic.value().id}, "");
    REQUIRE(started.succeeded());
    auto rejected = fixture.catalog.deleteMainTopic(mainTopic.value().id);
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Delete the time entries on this topic first.");
}

TEST_CASE("a subtopic with a time entry cannot be deleted") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto started =
        fixture.tracking.start(TimeTarget{TimeTargetKind::SubTopic, seeded.subTopic.id}, "");
    REQUIRE(started.succeeded());
    auto rejected = fixture.catalog.deleteSubTopic(seeded.subTopic.id);
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Delete the time entries on this subtopic first.");
}

TEST_CASE("an empty subtopic and then its main topic and category can be deleted") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    REQUIRE(fixture.catalog.deleteSubTopic(seeded.subTopic.id).succeeded());
    REQUIRE(fixture.catalog.deleteMainTopic(seeded.mainTopic.id).succeeded());
    REQUIRE(fixture.catalog.deleteCategory(seeded.category.id).succeeded());
    auto loaded = fixture.catalog.loadAll();
    REQUIRE(loaded.succeeded());
    CHECK(loaded.value().categories.empty());
}

TEST_CASE("renaming a subtopic keeps its parent") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto renamed = fixture.catalog.updateSubTopic(seeded.subTopic.id, "Review", "more");
    REQUIRE(renamed.succeeded());
    CHECK(renamed.value().name == "Review");
    CHECK(renamed.value().notes == "more");
    CHECK(renamed.value().mainTopicId == seeded.mainTopic.id);
}

TEST_CASE("the same subtopic name is allowed under another main topic") {
    Fixture fixture;
    fixture.open();
    auto seeded = fixture.seed();
    auto other = fixture.catalog.createMainTopic(seeded.category.id, "Research", "");
    REQUIRE(other.succeeded());
    CHECK(fixture.catalog.createSubTopic(other.value().id, "API", "").succeeded());
    auto duplicate = fixture.catalog.createSubTopic(seeded.mainTopic.id, "api", "");
    CHECK_FALSE(duplicate.succeeded());
    CHECK(duplicate.error() == "A subtopic with that name already exists.");
}

TEST_CASE("a subtopic requires a main topic") {
    Fixture fixture;
    fixture.open();
    auto rejected = fixture.catalog.createSubTopic(404, "API", "");
    CHECK_FALSE(rejected.succeeded());
    CHECK(rejected.error() == "Main topic not found.");
}
