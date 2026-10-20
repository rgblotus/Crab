#include "crab/crab.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("engine version data is populated", "[core][version]") {
  CHECK(crab::core::kEngineName == "Crab");
  CHECK(!crab::core::kEngineVersion.empty());
  CHECK(crab::core::kVersionMajor >= 0);
  CHECK(crab::core::kVersionMinor >= 0);
  CHECK(crab::core::kVersionPatch >= 0);
  CHECK(!crab::core::kBuildType.empty());
  CHECK(!crab::core::kCompiler.empty());
}
