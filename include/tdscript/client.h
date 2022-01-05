// created by lccc 12/04/2021, no copyright

#ifndef INCLUDE_TDSCRIPT_CLIENT_H_
#define INCLUDE_TDSCRIPT_CLIENT_H_

#include "tdscript/config.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <cmath>
#include <td/telegram/td_api.hpp>

#include "libdns/client.h"

#include "rapidjson/document.h"

#include "libxml/parser.h"

#include <stdexcept>
#include <map>
#include <ctime>
#include <functional>
#include <random>
#include <iostream>
#include <csignal>
#include <sstream>
#include <regex>
#include <queue>
#include <utility>
#include <fstream>
#include <filesystem>

#include <arpa/inet.h>

namespace tdscript {
  const std::string VERSION = std::to_string(TDSCRIPT_VERSION_MAJOR) + "." + std::to_string(TDSCRIPT_VERSION_MINOR);

  extern bool stop;

  extern bool save_flag;
  extern bool data_ready;
  extern std::map<std::int64_t, std::int32_t> player_count;
  extern std::map<std::int64_t, std::uint8_t> has_owner;
  extern std::map<std::int64_t, std::vector<std::int64_t>> pending_extend_messages;
  extern std::map<std::int64_t, std::uint64_t> last_extent_at;
  extern std::map<std::int64_t, std::int64_t> players_message;
  extern std::map<std::int64_t, std::uint8_t> need_extend;

  extern std::mt19937 rand_engine;

  extern std::time_t last_task_at;
  extern std::map<std::time_t, std::vector<std::function<void()>>> task_queue;

  const double RECEIVE_TIMEOUT_S = 0.01;
  const double AUTHORIZE_TIMEOUT_S = 30;
  const int SOCKET_TIME_OUT_MS = 10;

  inline void check_environment(const char *name) {
    if (std::getenv(name) == nullptr || std::string(std::getenv(name)).empty()) {
      throw std::invalid_argument(std::string("$").append(name).append(" empty"));
    }
  }

  inline void quit(int signum) { stop = true; std::cout << "received the signal: " << signum << std::endl; }

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

  template<typename T>
  inline void select_one_randomly(const std::vector<T>& arr, const std::function<void(std::size_t)>& f) {
    if (!arr.empty()) { f((std::uniform_int_distribution<int>(0, arr.size() - 1))(rand_engine)); }
  }

  inline int ustring_count(const std::string& text, std::size_t offset1, std::size_t offset2) {
    int len = 0;
    for (std::size_t i = offset1; i < offset2; i++) {
      len += (text[i] & 0xc0) != 0x80;
    }
    return len;
  }

  inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
      return !std::isspace(ch);
    }));
  }
  inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
      return !std::isspace(ch);
    }).base(), s.end());
  }
  inline void trim(std::string &s) { ltrim(s); rtrim(s); }

  inline void text_part_append(std::string& main, const std::string& apart) {
    if (main.empty() || apart.empty()
        || main[main.length() - 1] == '(' || apart[0] == ')' || main[main.length() - 1] == '-'
        || apart[0] == ' ' || apart[0] == ',' || apart[0] == '.' || apart[0] == ';') {
      main.append(apart);
    } else {
      main.append(" ").append(apart);
    }
  }

  inline bool xml_check_eq(const xmlChar *a, const char *b) {
    int i = 0; do { if (a[i] != b[i]) { return false; } i++; } while (a[i] && b[i]);
    return a[i] == b[i];
  }

  inline std::string xml_get_prop(const xmlNode *node, const std::string& name) {
    std::stringstream ss; ss << xmlGetProp(node, (xmlChar*)name.c_str()); return ss.str();
  }

  inline std::string xml_get_content(const xmlNode *node) {
    std::stringstream ss; ss << xmlNodeGetContent(node);
    std::string content = ss.str(); trim(content);
    return content;
  }

  // breadth-first search
  inline void xml_each_next(xmlNode* node, const std::function<bool(xmlNode* node)>& f) {
    std::queue<xmlNode*> nodes;
    do {
      if (f(node)) {
        return;
      }
      if (node->children) {
        nodes.push(node);
      }
      if (node->next) {
        node = node->next;
      } else if (!nodes.empty()) {
        node = nodes.front()->children;
        nodes.pop();
      } else {
        break;
      }
    } while(true);
  }

  // depth-first search
  inline void xml_each_child(xmlNode* node, const std::function<bool(xmlNode* node)>& f) {
    std::deque<xmlNode*> nodes;
    do {
      if (f(node)) {
        return;
      }
      if (node->next) {
        nodes.push_back(node);
      }
      if (node->children) {
        node = node->children;
      } else if (!nodes.empty()) {
        node = nodes.back()->next;
        nodes.pop_back();
      } else {
        break;
      }
    } while(true);
  }

  class Client {
   private:
    typedef td::td_api::object_ptr<td::td_api::Object> tdo_ptr;
    typedef std::function<void(std::vector<std::vector<char>>)> callback_t;

   public:
    std::unique_ptr<td::ClientManager> td_client_manager;
    std::int32_t client_id;
    std::uint64_t current_query_id = 0;
    std::map<std::uint64_t, std::function<void(tdo_ptr)>> query_callbacks;
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

      dns_client = libdns::Client(log_verbosity_level ? 1 : 0);
    }

    Client() : Client(0) {};

    inline void send_request(td::td_api::object_ptr<td::td_api::Function> f) {
      send_request(std::move(f), [](tdo_ptr update){ });
    }

    inline void send_request(td::td_api::object_ptr<td::td_api::Function> f, std::function<void(tdo_ptr)> callback) {
      std::cout << "send " << (current_query_id + 1) << ": " << td::td_api::to_string(f) << std::endl;
      td_client_manager->send(client_id, ++current_query_id, std::move(f));
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

    void send_html(std::int64_t chat_id, std::int64_t reply_id, std::string text, bool no_link_preview, bool no_html);

    inline void send_html(std::int64_t chat_id, std::string text) {
      send_html(chat_id, 0, std::move(text), true, false);
    }
    inline void send_text(std::int64_t chat_id, std::string text) {
      send_text(chat_id, 0, std::move(text), false);
    }
    inline void send_text(std::int64_t chat_id, std::string text, bool no_link_preview) {
      send_text(chat_id, 0, std::move(text), no_link_preview);
    }
    inline void send_text(std::int64_t chat_id, std::int64_t reply_id, std::string text, bool no_link_preview) {
      send_html(chat_id, reply_id, std::move(text), no_link_preview, true);
    }

    inline void send_voice(std::int64_t chat_id, int duration, std::string filename, std::string waveform) {
      auto send_message = td::td_api::make_object<td::td_api::sendMessage>();
      send_message->chat_id_ = chat_id;
      auto message_content = td::td_api::make_object<td::td_api::inputMessageVoiceNote>();
      message_content->waveform_ = std::move(waveform);
      message_content->duration_ = duration;
      auto file_local = td::td_api::make_object<td::td_api::inputFileLocal>();
      file_local->path_ = std::move(filename);
      message_content->voice_note_ = std::move(file_local);
      send_message->input_message_content_ = std::move(message_content);
      send_request(std::move(send_message));
    }

    inline void send_reply(std::int64_t chat_id, std::int64_t reply_id, std::string text) {
      send_text(chat_id, reply_id, std::move(text), false);
    }
    void send_start(std::int64_t chat_id);
    inline void send_join(std::int64_t chat_id, std::int64_t bot_id, const std::string& link) {
      send_join(chat_id, bot_id, link, 9);
    }
    void send_join(std::int64_t chat_id, std::int64_t bot_id, const std::string& link, int limit);
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

    inline void wiki_get_random_title(const std::string& lang, const std::string& wd, const std::function<void(std::string)>& f) {
      std::string host = lang + wd;
      std::string path = "/w/api.php?action=query&format=json&list=random&rnnamespace=0";
      send_https_request(host, path, [host, f](const std::vector<std::vector<char>> &res) {
        rapidjson::Document data;
        data.Parse(std::string(res[1].begin(), res[1].end()).c_str());
        if (data.HasMember("query")) {
          std::string title = data["query"]["random"][0]["title"].GetString();
          f(title);
        }
      });
    }

    inline void wiki_get_random_title(const std::string& lang, const std::function<void(std::string)>& f) {
      wiki_get_random_title(lang, ".wikipedia.org", f);
    }

    inline void wiki_get_data(const std::string& lang, const std::string& title, const std::string& wd, const std::function<void(std::string)>& f) {
      std::string host = lang + wd;
      std::string path = "/w/api.php?action=parse&format=json&page=" + libdns::urlencode(title);
      send_https_request(host, path, [f](const std::vector<std::vector<char>>& res){
        f(std::string(res[1].begin(), res[1].end()));
      });
    }

    void wiki_get_content(const std::string& lang, const std::string& title, const std::function<void(std::string)>& f);

    inline void dict_get_random_title(const std::string& lang, const std::function<void(std::string)>& f) {
      wiki_get_random_title(lang, ".wiktionary.org", f);
    }

    void dict_get_content(const std::string& lang, const std::string& title, const callback_t& f);

    void process_wiki(std::int64_t chat_id, const std::string &text, const std::string& kw, const std::string& domain);

    inline void process_wiki(std::int64_t chat_id, const std::string &text) {
      process_wiki(chat_id, text, "wiki", "wikipedia");
    }

    inline void process_wiki(std::int64_t chat_id, const std::string &lang, const std::string &title) {
      wiki_get_content(lang, title, [this, chat_id](auto desc) { send_text(chat_id, desc, true); });
    }

    inline void process_dict(std::int64_t chat_id, const std::string &text) {
      process_wiki(chat_id, text, "dict", "wiktionary");
    }

    inline void process_dict(std::int64_t chat_id, std::int64_t reply_id, const std::string &text) {
      get_message(chat_id, reply_id, [this,chat_id,text](tdo_ptr update) {
        std::cout << "callback: " << td::td_api::to_string(update) << std::endl;

        if (td::td_api::messages::ID == update->get_id()) {
          auto& msgs = static_cast<td::td_api::messages*>(update.get())->messages_;
          if (msgs.size() != 1) { return; }
          if (td::td_api::messageText::ID == msgs[0]->content_->get_id()) {
            std::string r_text = static_cast<td::td_api::messageText*>(msgs[0]->content_.get())->text_->text_; trim(r_text);
            process_wiki(chat_id, text + " " + r_text, "dict", "wiktionary");
          }
        }
      });
    }

    void process_dict(std::int64_t chat_id, const std::string &lang, const std::string &title);
  };  // class Client
}  // namespace tdscript

#endif  // INCLUDE_TDSCRIPT_CLIENT_H_
