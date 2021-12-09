// created by lich 12/04/2021, no copyright

#include "tdscript/client.h"

#include <iostream>
#include <sstream>
#include <regex>
#include <fstream>

#include <signal.h>


namespace tdscript {
  const std::string VERSION = std::to_string(TDSCRIPT_VERSION_MAJOR).append(".").append(std::to_string(TDSCRIPT_VERSION_MINOR));

  bool stop = false;
  void quit(int signum) { stop = true; }

  const std::int64_t USER_ID_WEREWOLF = 175844556;
  const std::int32_t EXTEND_TIME = 123;
  const std::string EXTEND_TEXT = std::string("/extend@werewolfbot ").append(std::to_string(EXTEND_TIME));
  const std::vector<std::string> AT_LIST = { "@JulienKM" };
  const std::unordered_map<std::int64_t, std::int64_t> STICKS_STARTING = { { -681384622, 356104798208 } };

  const auto SAVE_FILENAME = std::string(std::getenv("HOME")).append("/").append(".tdscript.save");
  bool save_flag = false;
  bool data_ready = false;
  std::unordered_map<std::int64_t, std::int32_t> player_count;
  std::unordered_map<std::int64_t, std::uint8_t> has_owner;
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> pending_extend_mesages;
  std::unordered_map<std::int64_t, std::uint64_t> last_extent_at;
  std::unordered_map<std::int64_t, std::int64_t> players_message;
  std::unordered_map<std::int64_t, std::uint8_t> need_extend;

  SSL_CTX *ssl_ctx;

  std::time_t last_task_at = -1;
  std::unordered_map<std::time_t, std::vector<std::function<void()>>> task_queue;


  Client::Client(std::int32_t log_verbosity_level) {
    check_environment("HOME");
    check_environment("TG_API_ID");
    check_environment("TG_API_HASH");
    check_environment("TG_DB_ENCRYPTION_KEY");
    check_environment("TG_PHOME_NUMBER");

    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    load();

    td::ClientManager::execute(td::td_api::make_object<td::td_api::setLogVerbosityLevel>(log_verbosity_level));
    td_client_manager = std::make_unique<td::ClientManager>();
    client_id = td_client_manager->create_client_id();
    send_request(td::td_api::make_object<td::td_api::getOption>("version"));

    epollfd = epoll_create1(0);

    SSL_library_init();
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
    ssl_ctx = SSL_CTX_new(TLS_client_method());
  }

  void Client::send_request(td::td_api::object_ptr<td::td_api::Function> f) {
    std::cout << "send " << (current_query_id + 1) << ": " << td::td_api::to_string(f) << std::endl;
    td_client_manager->send(client_id, current_query_id++, std::move(f));
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
    send_text(chat_id, 0, text);
  }

  void Client::send_text(std::int64_t chat_id, std::int64_t reply_id, std::string text) {
    auto send_message = td::td_api::make_object<td::td_api::sendMessage>();
    send_message->chat_id_ = chat_id;
    send_message->reply_to_message_id_ = reply_id;
    auto message_content = td::td_api::make_object<td::td_api::inputMessageText>();
    message_content->text_ = td::td_api::make_object<td::td_api::formattedText>();
    message_content->text_->text_ = std::move(text);
    send_message->input_message_content_ = std::move(message_content);
    send_request(std::move(send_message));
  }

  void Client::send_start(std::int64_t chat_id, std::int64_t bot_id, std::string link) {
    send_start(chat_id, bot_id, link, 9);
  }

  void Client::send_start(std::int64_t chat_id, std::int64_t bot_id, std::string link, int limit) {
    if (has_owner[chat_id]) { return; }
    if (!need_extend[chat_id]) { return; }
    if (limit <= 0) { return ; }
    std::regex param_regex("\\?start=(.*)");
    std::smatch param_match;
    std::string param;
    if (std::regex_search(link, param_match, param_regex)) {
      if (param_match.size() == 2) {
        param = param_match[1];
      }
    }

    auto send_message = td::td_api::make_object<td::td_api::sendBotStartMessage>();
    send_message->chat_id_ = bot_id;
    send_message->bot_user_id_ = bot_id;
    send_message->parameter_ = param;
    send_request(std::move(send_message));

    // add to the task queue, resend after 3 seconds, limit 9(default)
    task_queue[std::time(nullptr) + 3].push_back([this, chat_id, bot_id, link, limit]() {
      send_start(chat_id, bot_id, link, limit - 1);
    });
  }

  void Client::send_extend(std::int64_t chat_id) {
    if (!need_extend[chat_id]) { return; }
    if (!has_owner[chat_id]) { return; }
    if (player_count[chat_id] >= 5) { return; }
    if (pending_extend_mesages[chat_id].size() > 10) { return; }
    if (last_extent_at[chat_id] && std::time(nullptr) - last_extent_at[chat_id] < 5) { return; }
    last_extent_at[chat_id] = std::time(nullptr);
    send_text(chat_id, EXTEND_TEXT);
  }

  void Client::delete_messages(std::int64_t chat_id, std::vector<std::int64_t> message_ids) {
    send_request(td::td_api::make_object<td::td_api::deleteMessages>(chat_id, std::move(message_ids), true));
  }

  void Client::get_message(std::int64_t chat_id, std::int64_t msg_id) {
    if (chat_id == 0 || msg_id == 0) { return; }
    std::vector<std::int64_t> message_ids = { msg_id };
    send_request(td::td_api::make_object<td::td_api::getMessages>(chat_id, std::move(message_ids)));
  }

  void Client::forward_message(std::int64_t chat_id, std::int64_t from_chat_id, std::int64_t msg_id) {
    auto send_message = td::td_api::make_object<td::td_api::forwardMessages>();
    send_message->chat_id_ = chat_id;
    send_message->from_chat_id_ = from_chat_id;
    send_message->message_ids_ = { msg_id };
    send_message->send_copy_ = true;
    send_message->remove_caption_ = false;
    send_request(std::move(send_message));
  }

  void Client::send_http_request(std::string host, std::string path, std::function<void(std::string)> f) {
    int sockfd;
    if ((sockfd = connect_host(epollfd, host, 80)) == -1) {
      perror("connect");
      return;
    }
    auto data = gen_http_request_data(host, path);
    if (send(sockfd, data.c_str(), data.length(), MSG_DONTWAIT) == -1) {
      perror("send");
      epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
      close(sockfd);
      return;
    }
    std::cout << "HTTP request: " << data << '\n';
    request_callbacks[sockfd] = f;
  }

  void Client::send_https_request(std::string host, std::string path, std::function<void(std::string)> f) {
    int sockfd;
    if ((sockfd = connect_host(epollfd, host, 443)) == -1) {
      perror("connect");
      return;
    }

    SSL *ssl = SSL_new(ssl_ctx);
    do {
      if (!ssl) {
        printf("Error creating SSL.\n");
        log_ssl();
        break;
      }
      if (SSL_set_fd(ssl, sockfd) == 0) {
        printf("Error to set fd.\n");
        log_ssl();
        break;
      }
      int err = SSL_connect(ssl);
      if (err <= 0) {
        printf("Error creating SSL connection.  err=%x\n", err);
        log_ssl();
        break;
      }
      printf ("SSL connection using %s\n", SSL_get_cipher(ssl));

      auto data = gen_http_request_data(host, path);
      if (SSL_write(ssl, data.c_str(), data.length()) < 0) {
        log_ssl();
        break;
      }

      std::cout << "HTTPS request: " << data << '\n';
      request_callbacks[sockfd] = f;
      ssls[sockfd] = ssl;
      return;
    } while (false);

    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
    close(sockfd);
    SSL_free(ssl);
  }


  void Client::loop() {
    while(!stop) {
      process_tasks(std::time(nullptr));

      process_response(td_client_manager->receive(authorized ? TD_RECEIVE_TIMEOUT_S : TD_AUTHORIZE_TIMEOUT_S));

      process_socket_response(epoll_wait(epollfd, events, MAX_EVENTS, SOCKET_TIME_OUT_MS));
    }
  }


  void Client::process_tasks(std::time_t time) {
    if (last_task_at == time) { return; }
    last_task_at = time;

    // take out the current tasks and process
    for (const auto task : task_queue[time]) {
      task();
    }
    task_queue[time].clear();

    // confirm the extend
    for (const auto kv : player_count) {
      auto chat_id = kv.first;
      if (pending_extend_mesages[chat_id].size() != 0) {
        send_extend(chat_id);
      }
      if (last_extent_at[chat_id] && time - last_extent_at[chat_id] > EXTEND_TIME) {
        send_extend(chat_id);
      }
    }

    // confirm the number of players
    for (const auto kv : players_message) {
      auto chat_id = kv.first;
      auto msg_id = kv.second;
      if (time - last_extent_at[chat_id] > EXTEND_TIME / 2 && time % 7 == 0) {
        get_message(chat_id, msg_id);
      }
    }

    save();
  }

  void Client::process_response(td::ClientManager::Response response) {
    if (!response.object) { return; }
    auto update = std::move(response.object);
    std::cout << "receive " << response.request_id << ": " << td::td_api::to_string(update) << std::endl;

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

    if (td::td_api::updateNewMessage::ID == update->get_id()) {
      process_message(std::move(static_cast<td::td_api::updateNewMessage*>(update.get())->message_));
    }

    if (td::td_api::updateMessageContent::ID == update->get_id()) {
      auto umsg = static_cast<td::td_api::updateMessageContent*>(update.get());
      auto msg_id = umsg->message_id_;
      auto chat_id = umsg->chat_id_;
      if (td::td_api::messageText::ID == umsg->new_content_->get_id()) {
        auto text = static_cast<td::td_api::messageText*>(umsg->new_content_.get())->text_->text_;
        process_message(chat_id, msg_id, 0, text, "");
      }
    }

    if (td::td_api::messages::ID == update->get_id()) {  // from get_message
      auto msgs = std::move(static_cast<td::td_api::messages*>(update.get())->messages_);
      for (int i = 0; i < msgs.size(); i++) {
        process_message(std::move(msgs[i]));
      }
    }
  }

  void Client::process_message(td::td_api::object_ptr<td::td_api::message> msg) {
    if (msg && td::td_api::messageSenderUser::ID == msg->sender_->get_id()) {
      auto chat_id = msg->chat_id_;
      auto user_id = static_cast<td::td_api::messageSenderUser*>(msg->sender_.get())->user_id_;
      std::string text = "";
      if (msg->content_ && td::td_api::messageText::ID == msg->content_->get_id()) {
        text = static_cast<td::td_api::messageText*>(msg->content_.get())->text_->text_;
      }
      std::string link = "";
      if (msg->reply_markup_ && td::td_api::replyMarkupInlineKeyboard::ID == msg->reply_markup_->get_id()) {
        auto rows = std::move(static_cast<td::td_api::replyMarkupInlineKeyboard*>(msg->reply_markup_.get())->rows_);
        if (rows.size() == 1) {
          if (rows[0].size() == 1) {
            if (rows[0][0].get()->type_ && td::td_api::inlineKeyboardButtonTypeUrl::ID == rows[0][0].get()->type_->get_id()) {
              link = static_cast<td::td_api::inlineKeyboardButtonTypeUrl*>(rows[0][0].get()->type_.get())->url_;
            }
          }
        }
      }

      process_message(chat_id, msg->id_, user_id, text, link);
    }
  }

  void Client::process_message(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, std::string text, std::string link) {
    std::cout << "msg(" << msg_id << "): " << user_id << " -> " << chat_id << ", " << text << "\n\n";

    if (user_id == USER_ID_WEREWOLF || user_id == 0) {
      process_werewolf(chat_id, msg_id, user_id, text, link);
    }

    process_wiki(chat_id, msg_id, text);

    if (text == EXTEND_TEXT) {
      pending_extend_mesages[chat_id].push_back(msg_id);
    }

    const std::regex starting_regex("游戏启动中");
    std::smatch starting_match;
    if (std::regex_search(text, starting_match, starting_regex)) {
      for (const auto at : AT_LIST) { send_text(chat_id, at); }
      for (const auto kv : STICKS_STARTING) { forward_message(chat_id, kv.first, kv.second); }
    }

    save_flag = true;
  }

  void Client::process_werewolf(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, std::string text, std::string link) {
    const std::regex players_regex("^#players: (\\d+)");
    std::smatch players_match;
    if (std::regex_search(text, players_match, players_regex)) {
      if (players_match.size() == 2) {
        player_count[chat_id] = std::stoi(players_match[1]);

        if (user_id && msg_id) {
          if (players_message[chat_id] != msg_id) {
            last_extent_at[chat_id] = std::time(nullptr);
          }
          players_message[chat_id] = msg_id;
        }
      }
    }

    if (msg_id == players_message[chat_id]) {
      const std::regex owner_regex("lccc");
      std::smatch owner_match;
      if (std::regex_search(text, owner_match, owner_regex)) {
        has_owner[chat_id] = 1;
      }
    }

    if (user_id && !link.empty()) {
      need_extend[chat_id] = 1;
      send_start(chat_id, user_id, link);
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

    const std::regex cancel_regex("游戏取消|目前没有进行中的游戏|游戏启动中|夜幕降临|第\\d+天");
    std::smatch cancel_match;
    if (std::regex_search(text, cancel_match, cancel_regex)) {
      player_count[chat_id] = 0;
      has_owner[chat_id] = 0;
      pending_extend_mesages[chat_id].clear();
      last_extent_at[chat_id] = 0;
      players_message[chat_id] = 0;
      need_extend[chat_id] = 0;
    }
  }

  void Client::process_wiki(std::int64_t chat_id, std::int64_t msg_id, std::string text) {
    std::regex lang_regex("^\\/(.{0,2})wiki");
    std::regex title_regex("wiki(.*)$");
    std::smatch lang_match;
    std::smatch title_match;
    std::string lang;
    std::string title;
    if (std::regex_search(text, lang_match, lang_regex)
          && std::regex_search(text, title_match, title_regex)) {
      if (lang_match.size() == 2) {
        lang = lang_match[1];
      }
      if (title_match.size() == 2) {
        title = title_match[1];
      }
    } else {
      return;
    }
    if (lang.empty()) {
      lang = "en";
    }
    std::string host = lang.append(".wikipedia.org");
    if (title.empty()) {
      send_https_request(host, "/w/api.php?action=query&format=json&list=random&rnnamespace=0",
      [this, host, lang, chat_id, msg_id](std::string res) {
          std::string body = res.substr(res.find("\r\n\r\n"));
          try {
            auto data = nlohmann::json::parse(body);
            if (data.contains("query") && data["query"].contains("random") && data["query"]["random"].size() > 0) {
              std::string title = data["query"]["random"][0]["title"];
              std::cout << "wiki random title: " << title << '\n';
              process_wiki(chat_id, msg_id, lang, title);
            }
          } catch (nlohmann::json::parse_error &ex) { }
      });
    } else {
      process_wiki(chat_id, msg_id, lang, title);
    }
  }

  void Client::process_wiki(std::int64_t chat_id, std::int64_t msg_id, std::string lang, std::string title) {
    std::string host = lang.append(".wikipedia.org");
    std::stringstream ss;
    ss << title << '\n';
    // TODO: parse html https://github.com/lccxz/tg-script/blob/master/tg.rb#L278
    ss << "\nhttps://" << host << "/wiki/" << urlencode(title);
    send_text(chat_id, msg_id, ss.str());
  }

  void Client::process_socket_response(int event_id) {
    if (event_id < 1) { return; }
    struct epoll_event event = events[0];
    int sockfd = event.data.fd;
    if (ssls[sockfd]) {
      return process_ssl_response(event);
    }

    std::string res;
    char buffer[HTTP_BUFFER_SIZE];
    size_t response_size;
    do {
      if ((response_size = recv(sockfd, buffer, HTTP_BUFFER_SIZE, MSG_DONTWAIT)) == -1) {
        perror("recv");
        break;
      }
      res.append(std::string(buffer, 0, response_size));
      if (response_size < HTTP_BUFFER_SIZE) {
        break;
      }
    } while(true);

    std::cout << "socket(" << sockfd << ") response: " << res << '\n';
    request_callbacks[sockfd](res);
    request_callbacks.erase(sockfd);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &event);
    close(sockfd);
  }

  void Client::process_ssl_response(struct epoll_event event) {
    int sockfd = event.data.fd;
    SSL *ssl = ssls[sockfd];

    std::regex length_regex("Content-Length: (\\d+)", std::regex_constants::icase);
    std::string res;
    char buffer[HTTP_BUFFER_SIZE];
    int response_size;
    do {
      if ((response_size = SSL_read(ssl, buffer, HTTP_BUFFER_SIZE)) <= 0) {
        perror("recv");
        break;
      }
      res.append(std::string(buffer, 0, response_size));
      std::string body = res.substr(res.find("\r\n\r\n") + 4);
      std::smatch length_match;
      std::uint16_t content_length;
      if (std::regex_search(res, length_match, length_regex)) {
        if (length_match.size() == 2) {
          content_length = std::stoull(length_match[1]);
        }
      }
      if (content_length && content_length > body.length()) {
        continue;
      }
      if (response_size < HTTP_BUFFER_SIZE) {
        break;
      }
    } while(true);

    std::cout << "ssl socket(" << sockfd << ") response: " << res << '\n';
    request_callbacks[sockfd](res);
    request_callbacks.erase(sockfd);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &event);
    close(sockfd);
    SSL_free(ssl);
  }


  void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
  }

  int connect_host(int epollfd, std::string host, int port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char s[INET6_ADDRSTRLEN];
    int rv = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &servinfo);
    if (rv != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return -1;
    }
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
          perror("client: socket");
          continue;
        }

        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = sockfd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
          perror("epoll_ctl: ");
          return -1;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
          epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &event);
          close(sockfd);
          perror("client: connect");
          continue;
        }

        break;
    }
    if (p == NULL) {
      fprintf(stderr, "client: failed to connect\n");
      return -1;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client(%d): connecting to %s\n", sockfd, s);
    freeaddrinfo(servinfo); // all done with this structure

    return sockfd;
  }

  void log_ssl() {
    int err;
    while (err = ERR_get_error()) {
      char *str = ERR_error_string(err, 0);
      if (!str)
          return;
      printf(str);
      printf("\n");
    }
  }

  std::string gen_http_request_data(std::string host, std::string path) {
    std::stringstream req;
    req << "GET " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";
    req << "User-Agent: tdscript/" << VERSION << "\r\n";
    req << "Accept: */*\r\n";
    req << "\r\n";

    return req.str();
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

  std::string urlencode(const std::string &s) {
    std::ostringstream os;

    for (std::string::const_iterator ci = s.begin(); ci != s.end(); ++ci) {
      if ((*ci >= 'a' && *ci <= 'z') ||
              (*ci >= 'A' && *ci <= 'Z') ||
              (*ci >= '0' && *ci <= '9') ) { // allowed
        os << *ci;
      } else if (*ci == ' ') {
        os << '+';
      } else {
        os << '%' << to_hex(*ci >> 4) << to_hex(*ci % 16);
      }
    }

    return os.str();
  }

  void save() {
    if (!data_ready) { return; }
    if (!save_flag) { return; }
    save_flag = false;

    std::ofstream ofs;
    ofs.open (SAVE_FILENAME, std::ofstream::out | std::ofstream::binary);

    ofs << m2s(player_count) << '\n';
    ofs << m2s(has_owner) << '\n';
    ofs << ma2s(pending_extend_mesages) << '\n';
    ofs << m2s(last_extent_at) << '\n';
    ofs << m2s(players_message) << '\n';
    ofs << m2s(need_extend) << '\n';

    ofs.close();
  }

  void load() {
    std::ifstream file(SAVE_FILENAME);
    if (!file.good()) {
      data_ready = true;
      return ;
    }

    std::regex kv_regex("([^:]*):([^,]*),");
    std::regex stick_regex("([^|]*)\\|");
    std::string line = "";
    for (std::int32_t i = 0; std::getline(file, line); i++) {
      std::cout << "load " << i << ": " << line << '\n';
      std::smatch kv_match;
      while (std::regex_search(line, kv_match, kv_regex)) {
        std::string key = kv_match[1];
        std::string value = kv_match[2];
        std::cout << "  key: " << key << ", value: " << value << '\n';
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
            std::cout << "    " << a << '\n';
            pending_extend_mesages[std::stoll(key)].push_back(std::stoll(a));
            value = stick_match.suffix();
          }
        }
        if (i == 3) {
          last_extent_at[std::stoll(key)] = std::stoull(value);
        }
        if (i == 4) {
          players_message[std::stoll(key)] = std::stoll(value);
        }
        if (i == 5) {
          need_extend[std::stoll(key)] = std::stoi(value);
        }
        line = kv_match.suffix();
      }
    }
    data_ready = true;
  }
}  // namespace tdscript
