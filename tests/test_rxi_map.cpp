#include "map.h"
#include "list.h"

#include "gtest/gtest.h"

typedef map_t(list_t *) list_map_t;

class RxiMapTest : public ::testing::Test {
 protected:
  list_map_t list_map;

  RxiMapTest() = default;

  void SetUp() override {
    map_init(&list_map);
  }

  void TearDown() override {
    map_deinit(&list_map);
  }
};

TEST_F(RxiMapTest, MapGet) {
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
