// created by lccc 12/28/2020, no copyright

#include "tdscript/client.h"

int main() {
  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  std::string lang = "en";

  client.dict_get_content(lang, "exerting", [&](auto desc) {
    std::cout << "got: \n-------------\n" << desc << "\n-----------" << std::endl;

    tdscript::stop = true;
  });

  for (int i = 0; i < 999 && !tdscript::stop; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);
  }

  if (!tdscript::stop) {
    return 1;
  }
}