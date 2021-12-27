// created by lccc 12/24/2020, no copyright

#include "tdscript/client.h"

#include <cassert>

int main() {
  tdscript::stop = false;
  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  std::string lang = "en";

  std::int8_t callback_count = 0;
  client.dict_get_content(lang, "raffle", [&](auto desc) {
    std::cout << "got: \n-------------\n" << desc << "\n-----------" << std::endl;

    if (++callback_count > 1) {
      tdscript::stop = true;
    }
  });

  client.dict_get_random_title(lang, [&client, lang](auto title) {
    assert(!title.empty());

    std::cout << "random title: '" << title << "'\n";

    client.dict_get_content(lang, title, [lang](auto desc) {
      std::cout << "got: \n-------------\n" << desc << "\n-----------" << std::endl;
    });
  });

  client.dict_get_content(lang, "the", [lang](auto desc) {
    std::cout << "got: \n-------------\n" << desc << "\n-----------" << std::endl;
  });

  client.dict_get_content(lang, "franchise", [lang](auto desc) {
    std::cout << "got: \n-------------\n" << desc << "\n-----------" << std::endl;
  });

  for (int i = 0; i < 999 && !tdscript::stop; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);

    client.process_tasks(std::time(nullptr));
  }

  if (!tdscript::stop) {
    return 1;
  }
}