// created by lich 12/04/2020, no copyright no copyleft

#include "gtest/gtest.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include "tdscript/version.h"
#include "tdscript/client.h"


TEST(RandomTest, Create) {
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ(TDSCRIPT_VERSION_MAJOR, 0) << "major verion";
  EXPECT_EQ(TDSCRIPT_VERSION_MINOR, 1) << "minor version";

  std::unique_ptr<td::ClientManager> client_manager_ = std::make_unique<td::ClientManager>();
  EXPECT_EQ(1, client_manager_->create_client_id());
}
