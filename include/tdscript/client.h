// created by lich 12/04/2021, no copyright no copyleft

#ifndef INCLUDE_TDSCRIPT_CLIENT_H_
#define INCLUDE_TDSCRIPT_CLIENT_H_

#include "tdscript/version.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <assert.h>


namespace tdscript {
  extern std::string version;

  extern bool stop;

  void quit(int signum);

  class Client {
  public:
    double timeout = 9;
    std::unique_ptr<td::ClientManager> client_manager;
    std::int32_t client_id;
    std::uint64_t current_query_id = 0;
    bool authorized = false;

    Client();

    inline void send_request(td::td_api::object_ptr<td::td_api::Function> f) {
      client_manager->send(client_id, current_query_id++, std::move(f));
    }

    void send_parameters();
    void send_code();
    void send_password();

    inline td::ClientManager::Response get_response() {
      return client_manager->receive(timeout);
    }

    void loop();
  };  // class Client

}  // namespace tdscript

#endif  // INCLUDE_TDSCRIPT_CLIENT_H_
