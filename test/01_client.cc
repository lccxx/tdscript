// created by lccc 12/04/2020, no copyright

#include "tdscript/client.h"

#include <cassert>

int main() {
  assert("0.1" == tdscript::VERSION);

  tdscript::data_ready = true;
  auto client = tdscript::Client(1);

  std::map<std::string, std::uint8_t> check_list;

  for (int i = 0; i < 9; i++) {
    auto res = client.td_client_manager->receive(tdscript::RECEIVE_TIMEOUT_S);
    assert(res.request_id == 0);
    auto update = std::move(res.object);
    if (update) {
      std::cout << "receive " << res.request_id << ": " << td::td_api::to_string(update) << '\n';
      if (update->get_id() == td::td_api::updateOption::ID) {
        check_list["version"] = true;
      } else if (update->get_id() == td::td_api::updateAuthorizationState::ID) {
        check_list["authorization"] = true;
      }
    }
  }

  assert(check_list["version"]);
  assert(check_list["authorization"]);

  if (check_list.empty()) {
    return 1;
  }
}

