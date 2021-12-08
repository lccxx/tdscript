// created by lich 12/04/2021, no copyright

#ifndef INCLUDE_TDSCRIPT_CLIENT_H_
#define INCLUDE_TDSCRIPT_CLIENT_H_

#include "tdscript/version.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <stdexcept>
#include <unordered_map>

#include <assert.h>


namespace tdscript {
  extern const std::string VERSION;

  extern bool stop;
  void quit(int signum);

  extern bool save_flag;
  extern bool data_ready;
  extern std::unordered_map<std::int64_t, std::int32_t> player_count;
  extern std::unordered_map<std::int64_t, std::uint8_t> has_owner;
  extern std::unordered_map<std::int64_t, std::vector<std::int64_t>> pending_extend_mesages;
  extern std::unordered_map<std::int64_t, std::uint64_t> last_extent_at;
  extern std::unordered_map<std::int64_t, std::int64_t> players_message;
  extern std::unordered_map<std::int64_t, std::uint8_t> need_extend;


  class Client {
  public:
    std::unique_ptr<td::ClientManager> client_manager;
    std::int32_t client_id;
    std::uint64_t current_query_id = 0;
    bool authorized = false;

    Client();
    Client(std::int32_t log_verbosity_level);

    void send_request(td::td_api::object_ptr<td::td_api::Function> f);
    void send_parameters();
    void send_code();
    void send_password();
    void send_text(std::int64_t chat_id, std::string text);
    void send_start(std::int64_t chat_id, std::int64_t bot_id, std::string link);
    void send_start(std::int64_t chat_id, std::int64_t bot_id, std::string link, int limit);
    void send_extend(std::int64_t chat_id);
    void delete_messages(std::int64_t chat_id, std::vector<std::int64_t> message_ids);
    void get_message(std::int64_t chat_id, std::int64_t msg_id);
    void forward_message(std::int64_t chat_id, std::int64_t from_chat_id, std::int64_t msg_id);

    void loop();

    void process_message(td::td_api::object_ptr<td::td_api::message> msg);
    void process_message(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, std::string text, std::string link);
    void process_werewolf(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, std::string text, std::string link);

    inline void check_environment(const char *name) {
      if (std::getenv(name) == nullptr || std::string(std::getenv(name)).empty()) {
        throw std::invalid_argument(std::string("$").append(name).append(" empty"));
      }
    }
  };  // class Client

  template <typename Tk, typename Tv> std::string m2s(std::unordered_map<Tk, Tv> map);
  template <typename Tk, class Tv> std::string ma2s(std::unordered_map<Tk, Tv> map);
  void save();
  void load();
}  // namespace tdscript

#endif  // INCLUDE_TDSCRIPT_CLIENT_H_
