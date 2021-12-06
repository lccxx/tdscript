// created by lich 12/05/2020, no copyright

#include "gtest/gtest.h"

#include <td/telegram/td_json_client.h>

#include "tdscript/version.h"
#include "tdscript/client.h"


TEST(RandomTest, Create) {
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ(TDSCRIPT_VERSION_MAJOR, 0) << "major verion";
  EXPECT_EQ(TDSCRIPT_VERSION_MINOR, 1) << "minor version";

  int client_id = td_create_client_id();
  EXPECT_EQ(1, client_id);
}
