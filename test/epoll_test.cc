// created by lccc 12/04/2020, no copyright

#include "tdscript/client.h"

#include "gtest/gtest.h"

#include <regex>

TEST(RandomTest, Create) {  // NOLINT(cert-err58-cpp)
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ("0.1", tdscript::VERSION) << "version";

  ASSERT_EQ("abc+123+%E4%B8%AD%E6%96%87", tdscript::urlencode("abc 123 中文"));

  auto client = tdscript::Client(0);

  tdscript::player_count.clear();

  client.send_http_request("google.com", "/?1", [](const std::string& res) {
    if (res.find("/?1") != std::string::npos) {
      tdscript::player_count[0] = 1;
    }
  });
  client.send_http_request("google.com", "/?2", [](const std::string& res) {
    if (res.find("/?2") != std::string::npos) {
      tdscript::player_count[1] = 1;
    }
  });
  client.send_http_request("google.com", "/?33333", [](const std::string& res) {
    if (res.find("/?33333") != std::string::npos) {
      tdscript::player_count[2] = 1;
    }
  });

  client.send_https_request("google.com", "/?451592", [](const std::string& res) {
    if (res.find("/?451592") != std::string::npos) {
      tdscript::player_count[3] = 1;
    }
  });

  client.send_https_request("google.com", "/?515926", [](const std::string& res) {
    if (res.find("/?515926") != std::string::npos) {
      tdscript::player_count[4] = 1;
    }
  });

  client.send_https_request("en.wikipedia.org", "/w/api.php?action=query&format=json&list=random&rnnamespace=0",
    [&client](const std::string& res) {
      if (res.find("random") != std::string::npos) {
        tdscript::player_count[5] = 1;
      }
      std::regex length_regex("Content-Length: (\\d+)", std::regex_constants::icase);
      std::smatch length_match;
      std::uint16_t content_length = 0;
      if (std::regex_search(res, length_match, length_regex)) {
        content_length = std::stoull(length_match[1]);
      }

      std::string body = res.substr(res.find("\r\n\r\n") + 4);
      std::cout << "body(" << content_length << " -> " << body.length() << "): " << body << '\n';
      try {
        auto data = nlohmann::json::parse(body);
        if (data.contains("query") && data["query"].contains("random") && !data["query"]["random"].empty()) {
          std::string title = data["query"]["random"][0]["title"];
          std::cout << "random title: " << title << '\n';
          client.send_https_request("en.wikipedia.org", "/w/api.php?action=parse&format=json&page=" + tdscript::urlencode(title),
          [title](const std::string& res) {
            std::string body = res.substr(res.find("\r\n\r\n") + 4);
            auto data = nlohmann::json::parse(body);
            if (data.contains("error")) {
              std::cout << data["error"]["info"] << '\n';
            }
            if (data.contains("parse") && data["parse"].contains("text") && data["parse"]["text"].contains("*") > 0) {
              std::string text = data["parse"]["text"]["*"];

              xmlInitParser();
              xmlDocPtr doc = xmlParseMemory(text.c_str(), text.length());  // NOLINT(cppcoreguidelines-narrowing-conversions)
              if (doc == nullptr) {
                fprintf(stderr, "Error: unable to parse string: \"%s\"\n", text.c_str());
                return;
              }

              // xmlDocDump(stdout, doc);
              std::string article_desc;
              std::vector<std::string> desc_find_kws = { "。", " is ", " was ", "." };
              xmlNode *root = xmlDocGetRootElement(doc);
              for (xmlNode *node = root; node; node = node->next ? node->next : node->children) {
                std::cout << "node name: '" << node->name << "'\n";
                if (tdscript::xmlCheckEq(node->name, "p")) {
                  std::string content = tdscript::xmlNodeGetContentStr(node);
                  for (const auto& kw : desc_find_kws) {
                    if (content.find(kw) != std::string::npos) {
                      article_desc = content;
                      break;
                    }
                  }
                }
                if (!article_desc.empty()) {
                  break;
                }
              }

              article_desc = std::regex_replace(article_desc, std::regex(R"(\[\d+\])"), "");
              std::cout << "'" << title << "': '" << article_desc << "'\n";

              xmlFreeDoc(doc);
              xmlCleanupParser();
            }
          });
        }
      } catch (nlohmann::json::parse_error &ex) { }
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
