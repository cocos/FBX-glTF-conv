#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "bee/Converter.Test.h"
#include <doctest/doctest.h>
#include <fbxsdk.h>
#include <fx/gltf.h>
#include <string>

TEST_CASE("Demo") {
  // TODO
  CHECK_EQ(!!&bee::_convert_test, true);
}