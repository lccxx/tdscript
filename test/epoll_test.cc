// created by lich 12/04/2020, no copyright

#include "tdscript/client.h"

#include "gtest/gtest.h"


TEST(RandomTest, Create) {
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ("0.1", tdscript::VERSION) << "version";

  auto client = tdscript::Client(0);

  tdscript::player_count.clear();

  client.send_http_request("google.com", 80, "GET", "/?1", [](std::string res) {
    if (res.find("/?1") != std::string::npos) {
      tdscript::player_count[0] = 1;
    }
  });
  client.send_http_request("google.com", 80, "GET", "/?2", [](std::string res) {
    if (res.find("/?2") != std::string::npos) {
      tdscript::player_count[1] = 1;
    }
  });
  client.send_http_request("google.com", 80, "GET", "/?33333", [](std::string res) {
    if (res.find("/?33333") != std::string::npos) {
      tdscript::player_count[2] = 1;
    }
  });

  EXPECT_FALSE(tdscript::player_count[0]);
  EXPECT_FALSE(tdscript::player_count[1]);
  EXPECT_FALSE(tdscript::player_count[2]);

  for (int i = 0; i < 99; i++) {
    int event_id = epoll_wait(client.epollfd, client.events, tdscript::MAX_EVENTS, tdscript::SOCKET_TIME_OUT_MS);
    client.process_socket_response(event_id);
  }

  EXPECT_TRUE(tdscript::player_count[0]);
  EXPECT_TRUE(tdscript::player_count[1]);
  EXPECT_TRUE(tdscript::player_count[2]);
}
