// created by lccc 12/04/2021, no copyright

#ifndef INCLUDE_TDSCRIPT_CLIENT_H_
#define INCLUDE_TDSCRIPT_CLIENT_H_

#include "tdscript/config.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include "libdns/client.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdexcept>
#include <unordered_map>
#include <ctime>
#include <functional>
#include <random>
#include <iostream>
#include <csignal>

#include <arpa/inet.h>

namespace tdscript {
  const std::string VERSION = std::to_string(TDSCRIPT_VERSION_MAJOR) + std::string(".") + std::to_string(TDSCRIPT_VERSION_MINOR);

  extern bool stop;

  extern bool save_flag;
  extern bool data_ready;
  extern std::unordered_map<std::int64_t, std::int32_t> player_count;
  extern std::unordered_map<std::int64_t, std::uint8_t> has_owner;
  extern std::unordered_map<std::int64_t, std::vector<std::int64_t>> pending_extend_messages;
  extern std::unordered_map<std::int64_t, std::uint64_t> last_extent_at;
  extern std::unordered_map<std::int64_t, std::int64_t> players_message;
  extern std::unordered_map<std::int64_t, std::uint8_t> need_extend;

  extern std::mt19937 rand_engine;

  const double RECEIVE_TIMEOUT_S = 0.01;
  const double AUTHORIZE_TIMEOUT_S = 30;
  const int SOCKET_TIME_OUT_MS = 10;

  inline void check_environment(const char *name) {
    if (std::getenv(name) == nullptr || std::string(std::getenv(name)).empty()) {
      throw std::invalid_argument(std::string("$").append(name).append(" empty"));
    }
  }

  inline void quit(int signum) { stop = true; printf("received the signal: %d\n", signum); }

  void save();
  void load();

  inline void initial() {
    check_environment("HOME");
    check_environment("TG_API_ID");
    check_environment("TG_API_HASH");
    check_environment("TG_DB_ENCRYPTION_KEY");
    check_environment("TG_PHONE_NUMBER");

    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    load();
  }

  class Client {
   private:
    typedef td::td_api::object_ptr<td::td_api::Object> tdo_ptr;
    typedef std::function<void(std::vector<std::string>)> callback_t;

   public:
    std::unique_ptr<td::ClientManager> td_client_manager;
    std::int32_t client_id;
    std::uint64_t current_query_id = 0;
    std::unordered_map<std::uint64_t, std::function<void(tdo_ptr)>> query_callbacks;
    bool authorized = false;

    libdns::Client dns_client;

    explicit inline Client(std::int32_t log_verbosity_level) {
      if (!data_ready) {
        initial();
      }

      td::ClientManager::execute(td::td_api::make_object<td::td_api::setLogVerbosityLevel>(log_verbosity_level));
      td_client_manager = std::make_unique<td::ClientManager>();
      client_id = td_client_manager->create_client_id();
      send_request(td::td_api::make_object<td::td_api::getOption>("version"));
    }
    Client() : Client(0) {};

    inline void send_request(td::td_api::object_ptr<td::td_api::Function> f) {
      send_request(std::move(f), [](tdo_ptr update){ });
    }
    inline void send_request(td::td_api::object_ptr<td::td_api::Function> f, std::function<void(tdo_ptr)> callback) {
      std::cout << "send " << (current_query_id + 1) << ": " << td::td_api::to_string(f) << std::endl;
      td_client_manager->send(client_id, current_query_id++, std::move(f));
      query_callbacks[current_query_id] = std::move(callback);
    }
    inline void send_parameters() {
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
    inline void send_code() {
      std::cout << "Enter authentication code: " << std::flush;
      std::string code;
      std::cin >> code;
      send_request(td::td_api::make_object<td::td_api::checkAuthenticationCode>(code));
    }
    inline void send_password() {
      std::cout << "Enter authentication password: " << std::flush;
      std::string password;
      std::getline(std::cin, password);
      send_request(td::td_api::make_object<td::td_api::checkAuthenticationPassword>(password));
    }
    inline void send_text(std::int64_t chat_id, std::string text) {
      send_text(chat_id, 0, std::move(text), false);
    }
    inline void send_text(std::int64_t chat_id, std::string text, bool no_link_preview) {
      send_text(chat_id, 0, std::move(text), no_link_preview);
    }
    inline void send_text(std::int64_t chat_id, std::int64_t reply_id, std::string text, bool no_link_preview) {
      auto send_message = td::td_api::make_object<td::td_api::sendMessage>();
      send_message->chat_id_ = chat_id;
      send_message->reply_to_message_id_ = reply_id;
      auto message_content = td::td_api::make_object<td::td_api::inputMessageText>();
      message_content->text_ = td::td_api::make_object<td::td_api::formattedText>();
      message_content->text_->text_ = std::move(text);
      message_content->disable_web_page_preview_ = no_link_preview;
      send_message->input_message_content_ = std::move(message_content);
      send_request(std::move(send_message));
    }
    inline void send_reply(std::int64_t chat_id, std::int64_t reply_id, std::string text) {
      send_text(chat_id, reply_id, std::move(text), false);
    }
    inline void send_start(std::int64_t chat_id, std::int64_t bot_id, const std::string& link) {
      send_start(chat_id, bot_id, link, 9);
    }
    void send_start(std::int64_t chat_id, std::int64_t bot_id, const std::string& link, int limit);
    void send_extend(std::int64_t chat_id);
    inline void delete_messages(std::int64_t chat_id, std::vector<std::int64_t> message_ids) {
      send_request(td::td_api::make_object<td::td_api::deleteMessages>(chat_id, std::move(message_ids), true));
    }
    inline void get_message(std::int64_t chat_id, std::int64_t msg_id, std::function<void(tdo_ptr)> callback) {
      if (chat_id == 0 || msg_id == 0) { return; }
      std::vector<std::int64_t> message_ids = { msg_id };
      send_request(td::td_api::make_object<td::td_api::getMessages>(chat_id, std::move(message_ids)), std::move(callback));
    }
    inline void forward_message(std::int64_t chat_id, std::int64_t from_chat_id, std::int64_t msg_id) {
      forward_message(chat_id, from_chat_id, msg_id, true);
    }
    inline void forward_message(std::int64_t chat_id, std::int64_t from_chat_id, std::int64_t msg_id, bool copy) {
      auto send_message = td::td_api::make_object<td::td_api::forwardMessages>();
      send_message->chat_id_ = chat_id;
      send_message->from_chat_id_ = from_chat_id;
      send_message->message_ids_ = { msg_id };
      send_message->send_copy_ = copy;
      send_message->remove_caption_ = false;
      send_request(std::move(send_message));
    }
    inline void send_https_request(const std::string& host, const std::string& path, const callback_t& f) {
      dns_client.query(host, TDSCRIPT_WITH_IPV6 ? 28 : 1, [this, host, path, f](std::vector<std::string> data) {
        if (!data.empty()) {
          dns_client.send_https_request(TDSCRIPT_WITH_IPV6 ? AF_INET6 : AF_INET, data[0], host, path, f);
        }
      });
    }

    inline void loop() {
      while(!stop) {
        process_tasks(std::time(nullptr));

        process_response(td_client_manager->receive(authorized ? RECEIVE_TIMEOUT_S : AUTHORIZE_TIMEOUT_S));

        dns_client.receive(SOCKET_TIME_OUT_MS);
      }
    }

    void process_tasks(std::time_t time);
    void process_response(td::ClientManager::Response response);
    void process_message(td::td_api::object_ptr<td::td_api::message> msg);
    void process_message(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, const std::string& text, const std::string& link);
    void process_werewolf(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, const std::string& text, const std::string& link);
    void process_wiki(std::int64_t chat_id, const std::string &text);
    void process_wiki(std::int64_t chat_id, const std::string &lang, const std::string &title);
  };  // class Client

  template<typename T>
  inline void select_one_randomly(const std::vector<T>& arr, const std::function<void(std::size_t)>& f) {
   if (!arr.empty()) { f((std::uniform_int_distribution<int>(0, arr.size() - 1))(rand_engine)); }
  }

  bool xmlCheckEq(const xmlChar *a, const char *b);
  std::string xmlNodeGetContentStr(const xmlNode *node);

  template<typename Tk, typename Tv> inline std::string m2s(std::unordered_map<Tk, Tv> map) {
    std::string line;
    for (const auto e : map) {
      line.append(std::to_string(e.first)).append(":").append(std::to_string(e.second)).append(",");
    }
    return line;
  }
  template<typename Tk, class Tv> inline std::string ma2s(std::unordered_map<Tk, Tv> map) {
    std::string line;
    for (const auto e : map) {
      line.append(std::to_string(e.first)).append(":");
      std::string block;
      for (const auto a : e.second) {
        block.append(std::to_string(a)).append("|");
      }
      line.append(block).append(",");
    }
    return line;
  }
}  // namespace tdscript

#endif  // INCLUDE_TDSCRIPT_CLIENT_H_
