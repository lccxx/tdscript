// created by lccc 12/24/2020, no copyright

#include "tdscript/client.h"

#include <cassert>

int main() {
  std::cout << "ab's.length(): " << std::string("ab's").length() << std::endl;
  std::cout << "I’m.length(): " << std::string("I’m").length() << std::endl;
  std::string example = "<i>I’m</i>";
  assert(example.length() == 12);
  assert(example.find("<i>") == 0);
  example.erase(0, 3);
  assert(example.size() == 9);
  assert(example.find("</i>") == 5);
  example.erase(5, 4);
  assert(tdscript::ustring_count(example, 0, 5) == 3);

  tdscript::stop = false;
  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  std::string lang = "en";

  client.dict_get_random_title(lang, [&client, lang](auto title) {
    assert(!title.empty());

    std::cout << "random title: '" << title << "'\n";

    client.dict_get_content(lang, title, [lang](auto desc) {
      assert(desc.size() > 0);
      assert(desc[0].size() > 0);
      std::cout << "got: \n-------------\n" << std::string(desc[0].begin(), desc[0].end()) << "\n-----------" << std::endl;

      tdscript::stop = true;
    });
  });

  for (int i = 0; i < 999 && !tdscript::stop; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);
  }

  if (!tdscript::stop) {
    return 1;
  }
}