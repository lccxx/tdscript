// created by lich 12/04/2021, no copyright

#include "tdscript/client.h"

#include <iostream>
#include <signal.h>

namespace tdscript {
  std::string version = std::to_string(TDSCRIPT_VERSION_MAJOR).append(".").append(std::to_string(TDSCRIPT_VERSION_MINOR));

  bool stop = false;

  void quit(int signum) { stop = true; }

  Client::Client() {
    assert(std::getenv("HOME") != NULL);
    assert(std::getenv("TG_API_ID") != NULL);
    assert(std::getenv("TG_API_HASH") != NULL);
    assert(std::getenv("TG_DB_ENCRYPTION_KEY") != NULL);
    assert(std::getenv("TG_PHOME_NUMBER") != NULL);

    td::ClientManager::execute(td::td_api::make_object<td::td_api::setLogVerbosityLevel>(0));
    client_manager = std::make_unique<td::ClientManager>();
    client_id = client_manager->create_client_id();
  }

  void Client::send_parameters() {
    auto parameters = td::td_api::make_object<td::td_api::tdlibParameters>();
    parameters->database_directory_ = std::string(std::getenv("HOME")).append("/").append(".tdlib");
    parameters->use_message_database_ = true;
    parameters->use_secret_chats_ = false;
    parameters->api_id_ = std::stoi(std::getenv("TG_API_ID"));
    parameters->api_hash_ = std::getenv("TG_API_HASH");
    parameters->system_language_code_ = "en";
    parameters->device_model_ = "Desktop";
    parameters->application_version_ = version;
    parameters->enable_storage_optimizer_ = true;
    send_request(td::td_api::make_object<td::td_api::setTdlibParameters>(std::move(parameters)));
  }

  void Client::send_code() {
    std::cout << "Enter authentication code: " << std::flush;
    std::string code;
    std::cin >> code;
    send_request(td::td_api::make_object<td::td_api::checkAuthenticationCode>(code));
  }

  void Client::send_password() {
    std::cout << "Enter authentication password: " << std::flush;
    std::string password;
    std::getline(std::cin, password);
    send_request(td::td_api::make_object<td::td_api::checkAuthenticationPassword>(password));
  }

  void Client::loop() {
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    while(!stop) {
      auto response = get_response();

      if (!response.object) {
        continue;
      }

      if (response.request_id != 0) {
        continue;
      }

      auto update = std::move(response.object);
      // std::cout << response.request_id << " " << td::td_api::to_string(update) << std::endl;

      if (td::td_api::updateAuthorizationState::ID == update.get()->get_id()) {
        auto state_id = static_cast<td::td_api::updateAuthorizationState*>(update.get())->authorization_state_.get()->get_id();
        switch (state_id) {
        case td::td_api::authorizationStateReady::ID:
          authorized = true;
          break;
        case td::td_api::authorizationStateWaitTdlibParameters::ID:
          send_parameters();
          break;
        case td::td_api::authorizationStateWaitEncryptionKey::ID:
          send_request(td::td_api::make_object<td::td_api::checkDatabaseEncryptionKey>(std::getenv("TG_DB_ENCRYPTION_KEY")));
          break;
        case td::td_api::authorizationStateWaitPhoneNumber::ID:
          send_request(td::td_api::make_object<td::td_api::setAuthenticationPhoneNumber>(std::getenv("TG_PHOME_NUMBER"), nullptr));
          break;
        case td::td_api::authorizationStateWaitCode::ID:
          send_code();
          break;
        case td::td_api::authorizationStateWaitPassword::ID:
          send_password();
          break;
        default:
          break;
        }
      }


      if (td::td_api::updateChatLastMessage::ID == update.get()->get_id()) {
        auto msg = std::move(static_cast<td::td_api::updateChatLastMessage*>(update.get())->last_message_);
        auto chat_id = msg->chat_id_;

        if (td::td_api::messageSenderUser::ID == msg->sender_.get()->get_id()) {
          auto user_id = static_cast<td::td_api::messageSenderUser*>(msg->sender_.get())->user_id_;

          if (td::td_api::messageText::ID == msg->content_.get()->get_id()) {
            auto text = static_cast<td::td_api::messageText*>(msg->content_.get())->text_->text_;

            std::cout << "msg: " << user_id << " -> " << chat_id << ", " << text << std::endl;
          }
        }
      }
    }
  }

}  // namespace tdscript
