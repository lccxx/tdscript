// created by lich 12/04/2020, no copyright

#include "tdscript/client.h"

#include "gtest/gtest.h"


TEST(RandomTest, Create) {
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ("0.1", tdscript::VERSION) << "version";

  auto client = tdscript::Client(0);

  tdscript::player_count.clear();

  client.send_http_request("google.com", "/?1", [](std::string res) {
    if (res.find("/?1") != std::string::npos) {
      tdscript::player_count[0] = 1;
    }
  });
  client.send_http_request("google.com", "/?2", [](std::string res) {
    if (res.find("/?2") != std::string::npos) {
      tdscript::player_count[1] = 1;
    }
  });
  client.send_http_request("google.com", "/?33333", [](std::string res) {
    if (res.find("/?33333") != std::string::npos) {
      tdscript::player_count[2] = 1;
    }
  });

  client.send_https_request("google.com", "/?451592", [](std::string res) {
    if (res.find("/?451592") != std::string::npos) {
      tdscript::player_count[3] = 1;
    }
  });

  client.send_https_request("google.com", "/?515926", [](std::string res) {
    if (res.find("/?515926") != std::string::npos) {
      tdscript::player_count[4] = 1;
    }
  });

  client.send_https_request("en.wikipedia.org", "/w/api.php?action=query&format=json&list=random&rnnamespace=0",
        [](std::string res) {
          if (res.find("random") != std::string::npos) {
            tdscript::player_count[5] = 1;
          }

          std::string body = res.substr(res.find("\r\n\r\n"));
          try {
            auto data = nlohmann::json::parse(body);
            if (data.contains("query") && data["query"].contains("random") && data["query"]["random"].size() > 0) {
              std::cout << "random title: " << data["query"]["random"][0]["title"] << '\n';
            }
          } catch (nlohmann::json::parse_error &ex) {
            
          }
      });

  EXPECT_FALSE(tdscript::player_count[0]);
  EXPECT_FALSE(tdscript::player_count[1]);
  EXPECT_FALSE(tdscript::player_count[2]);
  EXPECT_FALSE(tdscript::player_count[3]);
  EXPECT_FALSE(tdscript::player_count[4]);
  EXPECT_FALSE(tdscript::player_count[5]);

  for (int i = 0; i < 99; i++) {
    int event_id = epoll_wait(client.epollfd, client.events, tdscript::MAX_EVENTS, tdscript::SOCKET_TIME_OUT_MS);
    client.process_socket_response(event_id);
  }

  EXPECT_TRUE(tdscript::player_count[0]);
  EXPECT_TRUE(tdscript::player_count[1]);
  EXPECT_TRUE(tdscript::player_count[2]);
  EXPECT_TRUE(tdscript::player_count[3]);
  EXPECT_TRUE(tdscript::player_count[4]);
  EXPECT_TRUE(tdscript::player_count[5]);
}
