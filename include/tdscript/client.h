// created by lich 12/04/2021, no copyright

#ifndef INCLUDE_TDSCRIPT_CLIENT_H_
#define INCLUDE_TDSCRIPT_CLIENT_H_

#include "tdscript/version.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <unordered_map>
#include <ctime>
#include <functional>

#include <assert.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>


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

  const double TD_RECEIVE_TIMEOUT_S = 0.01;
  const double TD_AUTHORIZE_TIMEOUT_S = 30;
  const int SOCKET_TIME_OUT_MS = 10;
  constexpr int MAX_EVENTS = 1;
  constexpr size_t HTTP_BUFFER_SIZE = 8192;


  class Client {
  public:
    std::unique_ptr<td::ClientManager> td_client_manager;
    std::int32_t client_id;
    std::uint64_t current_query_id = 0;
    bool authorized = false;

    int epollfd;
    struct epoll_event events[MAX_EVENTS];
    std::unordered_map<std::int32_t, std::function<void(std::string)>> request_callbacks;
    std::unordered_map<std::int32_t, SSL*> ssls;

    Client() : Client(0) {};
    Client(std::int32_t log_verbosity_level);

    void send_request(td::td_api::object_ptr<td::td_api::Function> f);
    void send_parameters();
    void send_code();
    void send_password();
    void send_text(std::int64_t chat_id, std::string text);
    void send_text(std::int64_t chat_id, std::int64_t reply_id, std::string text);
    void send_start(std::int64_t chat_id, std::int64_t bot_id, std::string link);
    void send_start(std::int64_t chat_id, std::int64_t bot_id, std::string link, int limit);
    void send_extend(std::int64_t chat_id);
    void delete_messages(std::int64_t chat_id, std::vector<std::int64_t> message_ids);
    void get_message(std::int64_t chat_id, std::int64_t msg_id);
    void forward_message(std::int64_t chat_id, std::int64_t from_chat_id, std::int64_t msg_id);
    void send_http_request(std::string host, std::string path, std::function<void(std::string)> f);
    void send_https_request(std::string host, std::string path, std::function<void(std::string)> f);

    void loop();

    void process_tasks(std::time_t time);
    void process_response(td::ClientManager::Response response);
    void process_message(td::td_api::object_ptr<td::td_api::message> msg);
    void process_message(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, std::string text, std::string link);
    void process_werewolf(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, std::string text, std::string link);
    void process_wiki(std::int64_t chat_id, std::int64_t msg_id, std::string text);
    void process_wiki(std::int64_t chat_id, std::int64_t msg_id, std::string lang, std::string title);
    void process_socket_response(int event_id);
    void process_ssl_response(struct epoll_event event);

    inline void check_environment(const char *name) {
      if (std::getenv(name) == nullptr || std::string(std::getenv(name)).empty()) {
        throw std::invalid_argument(std::string("$").append(name).append(" empty"));
      }
    }
  };  // class Client

  void *get_in_addr(struct sockaddr *sa);
  int connect_host(int epollfd, std::string host, int port);
  void log_ssl();
  std::string gen_http_request_data(std::string host, std::string path);
  template <typename Tk, typename Tv> std::string m2s(std::unordered_map<Tk, Tv> map);
  template <typename Tk, class Tv> std::string ma2s(std::unordered_map<Tk, Tv> map);
  inline unsigned char to_hex( unsigned char x ) { return x + (x > 9 ? ('A'-10) : '0'); }
  std::string urlencode(const std::string &s);
  void save();
  void load();
}  // namespace tdscript

#endif  // INCLUDE_TDSCRIPT_CLIENT_H_
