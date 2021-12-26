// created by lccc 12/24/2020, no copyright

#include "tdscript/client.h"

#include <cassert>

int main() {
  tdscript::stop = false;
  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  std::string lang = "en";
  client.dict_get_random_title(lang, [&client, lang](auto title) {
    assert(!title.empty());

    std::cout << "random title: '" << title << "'\n";

    client.dict_get_content(lang, title, [lang,&client](auto desc) {
      std::cout << desc << '\n';

      client.dict_get_content(lang, "the", [lang,&client](auto desc) {
        std::cout << desc << '\n';

        client.dict_get_content(lang, "franchise", [](auto desc) {
          std::cout << desc << '\n';

          tdscript::stop = true;
        });
      });
    });
  });

  for (int i = 0; i < 999 && !tdscript::stop; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);
  }

  if (!tdscript::stop) {
    return 1;
  }
}