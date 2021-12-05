// created by lich 12/04/2020, no copyright no copyleft

#include "tdscript/client.h"

#include "gtest/gtest.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <cstdio>


TEST(RandomTest, Create) {
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ("0.1", tdscript::version) << "version";

  auto client = tdscript::Client();
  client.send_request(td::td_api::make_object<td::td_api::getOption>("version"));
  client.loop();

  //std::cout << response.request_id << " " << td::td_api::to_string(update) << std::endl;
}
