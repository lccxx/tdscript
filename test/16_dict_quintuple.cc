// created by lccc 12/28/2020, no copyright

#include "tdscript/client.h"

int main() {
  using namespace tdscript;

  tdscript::stop = false;
  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  std::string lang = "en";

  client.dict_get_content(lang, "quintuple", [&](auto content) {
    std::string desc = std::string(content[0].begin(), content[0].end());
    std::cout << "got: \n-------------\n" << desc << "\n-----------" << std::endl;

    if (desc == "Pronunciation") {
      exit(1);
    }
  });

  for (int i = 0; i < 99 && !tdscript::stop; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);
  }
}