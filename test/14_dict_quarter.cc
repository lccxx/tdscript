// created by lccc 12/28/2020, no copyright

#include "tdscript/client.h"

int main() {
  using namespace tdscript;

  tdscript::stop = false;
  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  std::string lang = "en";
  client.dict_get_content(lang, "quarter", [&](auto content) {
    std::string desc = std::string(content[0].begin(), content[0].end());
    std::cout << "got: \n-------------\n" << desc << "\n-----------" << std::endl;

    if (desc != "Pronunciation") {
      if (desc.find("endeavour.") == std::string::npos) {
        exit(1);
      }
      if (desc.find("quarter-dollar") == std::string::npos) {
        exit(2);
      }
    }

    tdscript::stop = true;
  });

  for (int i = 0; i < 999 && !tdscript::stop; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);
  }

  if (!tdscript::stop) {
    return 1;
  }
}