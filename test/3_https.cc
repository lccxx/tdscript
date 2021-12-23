// created by lccc 12/04/2020, no copyright

#include "tdscript/client.h"
#include "libdns/client.h"

#include <iostream>
#include <cassert>

int main() {
  assert("abc+123+%E4%B8%AD%E6%96%87" == libdns::urlencode("abc 123 中文"));

  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  client.send_https_request("google.com", "/?1", [](const std::vector<std::string>& res) {
    if (res[1].find("/?1") != std::string::npos) {
      tdscript::player_count[0] = 1;
    }
  });

  client.send_https_request("google.com", "/?2", [](const std::vector<std::string>& res) {
    if (res[1].find("/?2") != std::string::npos) {
      tdscript::player_count[1] = 1;
    }
  });

  client.send_https_request("google.com", "/?3333", [](const std::vector<std::string>& res) {
    if (res[1].find("/?3333") != std::string::npos) {
      tdscript::player_count[2] = 1;
    }
  });

  client.send_https_request("google.com", "/?451592", [](const std::vector<std::string>& res) {
    if (res[1].find("/?451592") != std::string::npos) {
      tdscript::player_count[3] = 1;
    }
  });

  std::string lang = "en";
  client.wiki_get_random_title(lang, [&client, lang](auto title) {
    tdscript::player_count[4] = 1;
    std::cout << "random title: " << title << '\n';

    client.wiki_get_content(lang, title, [&client, lang](auto content) {
      tdscript::player_count[5] = 1;

      client.wiki_get_content(lang, "bot", [&client, lang](auto desc) {
        tdscript::player_count[6] = 1;

        assert(!desc.empty());
        assert(desc.size() > 19);

        client.wiki_get_content(lang, "Civil War", [](auto desc) {
          tdscript::player_count[7] = 1;

          assert(desc.size() > 19);
        });
      });
    });
  });

  assert(tdscript::player_count[0] == 0);
  assert(tdscript::player_count[1] == 0);
  assert(tdscript::player_count[2] == 0);
  assert(tdscript::player_count[3] == 0);
  assert(tdscript::player_count[4] == 0);
  assert(tdscript::player_count[5] == 0);
  assert(tdscript::player_count[6] == 0);
  assert(tdscript::player_count[7] == 0);

  for (int i = 0; i < 999; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);
  }

  assert(tdscript::player_count[0] == 1);
  assert(tdscript::player_count[1] == 1);
  assert(tdscript::player_count[2] == 1);
  assert(tdscript::player_count[3] == 1);
  assert(tdscript::player_count[4] == 1);
  assert(tdscript::player_count[5] == 1);
  assert(tdscript::player_count[6] == 1);
  assert(tdscript::player_count[7] == 1);

  if (tdscript::player_count[7] == 0) {
    return 1;
  }
}
