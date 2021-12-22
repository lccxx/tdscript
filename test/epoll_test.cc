// created by lccc 12/04/2020, no copyright

#include "tdscript/client.h"
#include "libdns/client.h"
#include "rapidjson/document.h"

#include <iostream>
#include <regex>
#include <cassert>

#include <arpa/inet.h>

int main() {
  assert("abc+123+%E4%B8%AD%E6%96%87" == libdns::urlencode("abc 123 中文"));

  auto client = libdns::Client(0);
  tdscript::player_count.clear();

  client.query("google.com", 1, [&client](const std::vector<std::string>& data) {
    assert(!data.empty());
    std::string ip = data[0];

    client.send_https_request(AF_INET, ip, "google.com", "/?1", [](const std::vector<std::string>& res) {
      if (res[1].find("/?1") != std::string::npos) {
        tdscript::player_count[0] = 1;
      }
    });

    client.send_https_request(AF_INET, ip, "google.com", "/?2", [](const std::vector<std::string>& res) {
      if (res[1].find("/?2") != std::string::npos) {
        tdscript::player_count[1] = 1;
      }
    });

    client.send_https_request(AF_INET, ip, "google.com", "/?3333", [](const std::vector<std::string>& res) {
      if (res[1].find("/?3333") != std::string::npos) {
        tdscript::player_count[2] = 1;
      }
    });

    client.send_https_request(AF_INET, ip, "google.com", "/?451592", [](const std::vector<std::string>& res) {
      if (res[1].find("/?451592") != std::string::npos) {
        tdscript::player_count[3] = 1;
      }
    });
  });

  client.query("en.wikipedia.org", 1, [&client](const std::vector<std::string>& data) {
    assert(!data.empty());
    std::string ip = data[0];

    client.send_https_request(AF_INET, ip, "en.wikipedia.org", "/w/api.php?action=query&format=json&list=random&rnnamespace=0",
    [&client, ip](const std::vector<std::string>& res) {
      std::string body = res[1];
      if (body.find("random") != std::string::npos) {
        tdscript::player_count[4] = 1;
      }

      rapidjson::Document data; data.Parse(body.c_str());
      if (data["query"].IsObject() && data["query"]["random"].IsArray() && data["query"]["random"][0].IsObject()) {
        std::string title = data["query"]["random"][0]["title"].GetString();
        std::cout << "random title: " << title << '\n';
        client.send_https_request(AF_INET, ip, "en.wikipedia.org", "/w/api.php?action=parse&format=json&page=" + libdns::urlencode(title),
        [title](const std::vector<std::string>& res) {
          std::string body = res[1];
          rapidjson::Document data; data.Parse(body.c_str());
          if (data.HasMember("error") && data["error"].IsObject() && data["error"]["info"].IsString()) {
            std::cout << data["error"]["info"].GetString() << '\n';
          }
          if (data.HasMember("parse") && data["parse"].IsObject() && data["parse"]["text"].IsObject() && data["parse"]["text"]["*"].IsString()) {
            std::string text = data["parse"]["text"]["*"].GetString();

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
    });
  });

  assert(tdscript::player_count[0] == 0);
  assert(tdscript::player_count[1] == 0);
  assert(tdscript::player_count[2] == 0);
  assert(tdscript::player_count[3] == 0);
  assert(tdscript::player_count[4] == 0);

  for (int i = 0; i < 999; i++) {
    client.receive(tdscript::SOCKET_TIME_OUT_MS);
  }

  assert(tdscript::player_count[0] == 1);
  assert(tdscript::player_count[1] == 1);
  assert(tdscript::player_count[2] == 1);
  assert(tdscript::player_count[3] == 1);
  assert(tdscript::player_count[4] == 1);

  return 0;
}
