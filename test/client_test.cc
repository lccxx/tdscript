// created by lich 12/04/2020, no copyright

#include "tdscript/client.h"

#include "gtest/gtest.h"


TEST(RandomTest, Create) {
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ("0.1", tdscript::VERSION) << "version";

  auto client = tdscript::Client(1023);

  signal(SIGINT, tdscript::quit);
  signal(SIGTERM, tdscript::quit);

  client.send_request(td::td_api::make_object<td::td_api::getOption>("version"));
  while (!tdscript::stop) {
    auto response = client.client_manager->receive(9);
    if (response.request_id == 0 && response.object) {
      auto update = std::move(response.object);
      if (td::td_api::updateAuthorizationState::ID == update->get_id()) {
        auto state_id = static_cast<td::td_api::updateAuthorizationState*>(update.get())->authorization_state_.get()->get_id();
        if (td::td_api::authorizationStateWaitTdlibParameters::ID == state_id) {
          client.send_parameters();
        } else if (td::td_api::authorizationStateWaitEncryptionKey::ID == state_id) {
          client.send_request(td::td_api::make_object<td::td_api::checkDatabaseEncryptionKey>(std::getenv("TG_DB_ENCRYPTION_KEY")));
        }
      }
    }
  }
}
