// created by lich 12/04/2021, no copyright

#include "tdscript/client.h"

#include <iostream>
#include <unordered_map>
#include <regex>
#include <ctime>
#include <chrono>
#include <thread>
#include <fstream>

#include <signal.h>

namespace tdscript {
  const std::string VERSION = std::to_string(TDSCRIPT_VERSION_MAJOR).append(".").append(std::to_string(TDSCRIPT_VERSION_MINOR));

  bool stop = false;
  void quit(int signum) { stop = true; }

  const std::int64_t USER_ID_WEREWOLF = 175844556;
  const std::int32_t EXTEND_TIME = 123;
  const std::string EXTEND_TEXT = std::string("/extend@werewolfbot ").append(std::to_string(EXTEND_TIME));

  std::unordered_map<std::int64_t, std::int32_t> player_count;
  std::unordered_map<std::int64_t, std::uint8_t> has_owner;
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> pending_extend_mesages;
  std::unordered_map<std::int64_t, std::uint64_t> last_extent_at;

  std::unordered_map<std::uint64_t, std::vector<std::function<void()>>> task_queue;
  std::uint64_t tasks_counter = 0;

  bool save_flag = false;
  const auto SAVE_FILENAME = std::string(std::getenv("HOME")).append("/").append(".tdscript.save");


  Client::Client() : Client(0) { }

  Client::Client(std::int32_t log_verbosity_level) {
    assert(std::getenv("HOME") != NULL);
    assert(std::getenv("TG_API_ID") != NULL);
    assert(std::getenv("TG_API_HASH") != NULL);
    assert(std::getenv("TG_DB_ENCRYPTION_KEY") != NULL);
    assert(std::getenv("TG_PHOME_NUMBER") != NULL);

    td::ClientManager::execute(td::td_api::make_object<td::td_api::setLogVerbosityLevel>(log_verbosity_level));
    client_manager = std::make_unique<td::ClientManager>();
    client_id = client_manager->create_client_id();

    load();
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
    parameters->application_version_ = VERSION;
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

  void Client::send_text(std::int64_t chat_id, std::string text) {
    auto send_message = td::td_api::make_object<td::td_api::sendMessage>();
    send_message->chat_id_ = chat_id;
    auto message_content = td::td_api::make_object<td::td_api::inputMessageText>();
    message_content->text_ = td::td_api::make_object<td::td_api::formattedText>();
    message_content->text_->text_ = std::move(text);
    send_message->input_message_content_ = std::move(message_content);
    send_request(std::move(send_message));
  }

  void Client::delete_messages(std::int64_t chat_id, std::vector<std::int64_t> message_ids) {
    send_request(td::td_api::make_object<td::td_api::deleteMessages>(chat_id, std::move(message_ids), true));
  }

  void Client::loop() {
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    std::function<void()> f = [this]() {};

    std::thread([this] {  // tasks loop
      while(!stop) {
        if (save_flag) {
          save();
        }

        for (const auto task : task_queue[tasks_counter]) {
          task();
        }
        task_queue[tasks_counter].clear();
        tasks_counter += 1;

        for (const auto kv : last_extent_at) {
          auto chat_id = kv.first;
          if (pending_extend_mesages[chat_id].size() != 0) {
            send_extend(chat_id);
          }
          if (last_extent_at[chat_id] && std::time(nullptr) - last_extent_at[chat_id] > EXTEND_TIME) {
            send_extend(chat_id);
          }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
    }).detach();

    send_request(td::td_api::make_object<td::td_api::getOption>("version"));

    while(!stop) {  // client receive update loop
      double timeout = 30;
      if (authorized) {
        timeout = 3;
      }

      auto response = client_manager->receive(timeout);

      if (response.request_id != 0) {
        continue;
      }

      if (!response.object) {
        continue;
      }

      auto update = std::move(response.object);
      // std::cout << response.request_id << " " << td::td_api::to_string(update) << std::endl;

      if (td::td_api::updateAuthorizationState::ID == update->get_id()) {
        auto state_id = static_cast<td::td_api::updateAuthorizationState*>(update.get())->authorization_state_->get_id();
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

      if (td::td_api::updateChatLastMessage::ID == update->get_id()) {
        auto msg = std::move(static_cast<td::td_api::updateChatLastMessage*>(update.get())->last_message_);
        if (msg && td::td_api::messageSenderUser::ID == msg->sender_->get_id()) {
          auto chat_id = msg->chat_id_;
          auto user_id = static_cast<td::td_api::messageSenderUser*>(msg->sender_.get())->user_id_;

          if (td::td_api::messageText::ID == msg->content_->get_id()) {
            auto text = static_cast<td::td_api::messageText*>(msg->content_.get())->text_->text_;

            std::cout << "msg: " << user_id << " -> " << chat_id << ", " << text << std::endl;
            if (user_id == USER_ID_WEREWOLF) {
              process_werewolf(chat_id, msg->id_, text);
            }
            if (text == EXTEND_TEXT) {
              pending_extend_mesages[chat_id].push_back(msg->id_);
            }

            save_flag = true;
          }
        }
      }
    }
  }

  void Client::send_extend(std::int64_t chat_id) {
    if (last_extent_at[chat_id] && std::time(nullptr) - last_extent_at[chat_id] < 3) {
      return;
    }
    if (!has_owner[chat_id]) {
      return;
    }
    if (player_count[chat_id] >= 5) {
      return;
    }

    last_extent_at[chat_id] = std::time(nullptr);
    send_text(chat_id, EXTEND_TEXT);
  }

  void Client::process_werewolf(std::int64_t chat_id, std::int64_t msg_id, std::string text) {
    const std::regex players_regex("^#players: (\\d+)");
    std::smatch players_match;
    if (std::regex_search(text, players_match, players_regex)) {
      if (players_match.size() == 2) {
        player_count[chat_id] = std::stoi(players_match[1]);

        if (player_count[chat_id] == 0 || !last_extent_at[chat_id]) {
          last_extent_at[chat_id] = std::time(nullptr);
        }
      }
    }
    if (player_count[chat_id] >= 5) {
      has_owner[chat_id] = 0;
      pending_extend_mesages[chat_id].clear();
      last_extent_at[chat_id] = 0;
      return;
    }

    const std::regex owner_regex("lccc");
    std::smatch owner_match;
    if (std::regex_search(text, owner_match, owner_regex)) {
      has_owner[chat_id] = 1;
    }


    const std::regex cancel_regex("游戏取消|目前没有进行中的游戏|游戏启动中");
    std::smatch cancel_match;
    if (std::regex_search(text, cancel_match, cancel_regex)) {
      has_owner[chat_id] = 0;
      pending_extend_mesages[chat_id].clear();
      last_extent_at[chat_id] = 0;
      return;
    }

    const std::regex remain_regex("还有 1 分钟|还剩 \\d+ 秒");
    std::smatch remain_match;
    if (std::regex_search(text, remain_match, remain_regex)) {
      pending_extend_mesages[chat_id].push_back(msg_id);
    }

    const std::regex remain_reply_regex("现在又多了 123 秒时间");
    std::smatch remain_reply_match;
    if (std::regex_search(text, remain_reply_match, remain_reply_regex)) {
      pending_extend_mesages[chat_id].push_back(msg_id);
      delete_messages(chat_id, pending_extend_mesages[chat_id]);

      pending_extend_mesages[chat_id].clear();
    }
  }

  template <typename Tk, typename Tv>
  std::string m2s(std::unordered_map<Tk, Tv> map) {
    std::string line = "";
    for (const auto e : map) {
      line.append(std::to_string(e.first)).append(":").append(std::to_string(e.second)).append(",");
    }
    return line;
  }

  template <typename Tk, class Tv>
  std::string ma2s(std::unordered_map<Tk, Tv> map) {
    std::string line = "";
    for (const auto e : map) {
      line.append(std::to_string(e.first)).append(":");
      std::string block = "";
      for (const auto a : e.second) {
        block.append(std::to_string(a)).append("|");
      }
      line.append(block).append(",");
    }
    return line;
  }

  void Client::save() {
    std::ofstream ofs;
    ofs.open (SAVE_FILENAME, std::ofstream::out | std::ofstream::binary);

    ofs << m2s(player_count) << '\n';
    ofs << m2s(has_owner) << '\n';
    ofs << ma2s(pending_extend_mesages) << '\n';
    ofs << m2s(last_extent_at) << '\n';

    ofs.close();

    save_flag = false;
  }

  void Client::load() {
    std::ifstream file(SAVE_FILENAME);
    if (!file.good()) {
      return ;
    }

    std::regex comma_regex("([^,]*),");
    std::regex colon_regex("([^:]*):");
    std::regex stick_regex("([^|]*)\\|");
    std::string line = "";
    for (std::int32_t i = 0; getline(file, line); i++) {
      std::smatch comma_match;
      while (std::regex_search(line, comma_match, comma_regex)) {
        std::string word = comma_match[1];
        std::smatch colon_match;
        if (std::regex_search(word, colon_match, colon_regex)) {
          std::string key = colon_match[1];
          std::string value = colon_match.suffix();
          if (i == 0) {
            player_count[std::stoll(key)] = std::stoi(value);
          }
          if (i == 1) {
            has_owner[std::stoll(key)] = std::stoi(value);
          }
          if (i == 2) {
            std::smatch stick_match;
            while (std::regex_search(value, stick_match, stick_regex)) {
              std::string a = stick_match[1];
              pending_extend_mesages[std::stoll(key)].push_back(std::stoll(a));
              value = stick_match.suffix();
            }
          }
          if (1 == 3) {
            last_extent_at[std::stoll(key)] = std::stoull(value);
          }
        }
        line = comma_match.suffix();
      }
    }
  }
}  // namespace tdscript
