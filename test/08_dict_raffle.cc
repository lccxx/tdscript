// created by lccc 12/24/2020, no copyright

#include "tdscript/client.h"

#include <cassert>

int main() {
  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  std::string lang = "en";

  std::int8_t callback_count = 0;
  client.dict_get_content(lang, "raffle", [&](auto desc) {
    assert(desc.size() > 0);
    assert(desc[0].size() > 0);
    std::cout << "got: \n-------------\n" << std::string(desc[0].begin(), desc[0].end()) << "\n-----------" << std::endl;

    if (++callback_count > 1) {
      tdscript::stop = true;
    }
  });

  for (int i = 0; i < 999 && !tdscript::stop; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);

    client.process_tasks(std::time(nullptr));
  }

  if (!tdscript::stop) {
    return 1;
  }
}