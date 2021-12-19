// created by lccc 12/04/2021, no copyright

#include "tdscript/client.h"

#include "libdns/client.h"
#include "rapidjson/document.h"

#include <iostream>
#include <csignal>
#include <regex>
#include <fstream>
#include <utility>

#include <arpa/inet.h>

namespace tdscript {
  const std::int64_t USER_ID_WEREWOLF = 175844556;
  const std::int32_t EXTEND_TIME = 123;
  const std::string EXTEND_TEXT = std::string("/extend@werewolfbot ") + std::to_string(EXTEND_TIME);
  const std::vector<std::vector<std::int64_t>> STICKS_STARTING = {
      { -681384622, 357654593536 }, { -681384622, 357655642112 }, { -681384622, 356104798208 },
      { -1001098611371, 2753360297984, 2753361346560 },
      { 981032009, 357662982144, 2753361346560 },
      { 981032009, 357661933568, 2753361346560 },
      { 981032009, 357660884992, 2753361346560 },
      { 981032009, 357659836416, 2753361346560 },
      { 981032009, 357658787840, 2753361346560 },
  };
  const std::unordered_map<std::int64_t, std::int64_t> STICKS_REPLY_TO = { {2753361346560, 981032009 } };  // msg_id, user_id
  const std::unordered_map<std::int64_t, std::string> KEY_PLAYER_IDS = { { 981032009, "@TalkIce" } };
  const std::unordered_map<std::string, std::string> KEY_PLAYERS = { { "KMM", "@JulienKM" } };

  bool stop = false;

  const std::string SAVE_FILENAME = std::string(std::getenv("HOME")) + std::string("/.tdscript.save");
  bool save_flag = false;
  bool data_ready = false;
  std::unordered_map<std::int64_t, std::int32_t> player_count;
  std::unordered_map<std::int64_t, std::uint8_t> has_owner;
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> pending_extend_messages;
  std::unordered_map<std::int64_t, std::uint64_t> last_extent_at;
  std::unordered_map<std::int64_t, std::int64_t> players_message;
  std::unordered_map<std::int64_t, std::uint8_t> need_extend;

  std::unordered_map<std::int64_t, std::vector<std::string>> at_list;  // the '@' list
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> player_ids;
  bool werewolf_bot_warning = false;

  std::time_t last_task_at = -1;
  std::unordered_map<std::time_t, std::vector<std::function<void()>>> task_queue;
}

tdscript::Client::Client(std::int32_t log_verbosity_level) {
  check_environment("HOME");
  check_environment("TG_API_ID");
  check_environment("TG_API_HASH");
  check_environment("TG_DB_ENCRYPTION_KEY");
  check_environment("TG_PHONE_NUMBER");

  signal(SIGINT, quit);
  signal(SIGTERM, quit);

  load();

  td::ClientManager::execute(td::td_api::make_object<td::td_api::setLogVerbosityLevel>(log_verbosity_level));
  td_client_manager = std::make_unique<td::ClientManager>();
  client_id = td_client_manager->create_client_id();
  send_request(td::td_api::make_object<td::td_api::getOption>("version"));
}

void tdscript::Client::send_request(td::td_api::object_ptr<td::td_api::Function> f) {
  send_request(std::move(f), [](tdo_ptr update){ });
}

void tdscript::Client::send_request(td::td_api::object_ptr<td::td_api::Function> f, std::function<void(tdo_ptr)> callback) {
  std::cout << "send " << (current_query_id + 1) << ": " << td::td_api::to_string(f) << std::endl;
  td_client_manager->send(client_id, current_query_id++, std::move(f));
  query_callbacks[current_query_id] = std::move(callback);
}

void tdscript::Client::send_parameters() {
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

void tdscript::Client::send_code() {
  std::cout << "Enter authentication code: " << std::flush;
  std::string code;
  std::cin >> code;
  send_request(td::td_api::make_object<td::td_api::checkAuthenticationCode>(code));
}

void tdscript::Client::send_password() {
  std::cout << "Enter authentication password: " << std::flush;
  std::string password;
  std::getline(std::cin, password);
  send_request(td::td_api::make_object<td::td_api::checkAuthenticationPassword>(password));
}

void tdscript::Client::send_text(std::int64_t chat_id, std::string text) {
  send_text(chat_id, 0, std::move(text), false);
}

void tdscript::Client::send_text(std::int64_t chat_id, std::string text, bool no_link_preview) {
  send_text(chat_id, 0, std::move(text), no_link_preview);
}

void tdscript::Client::send_text(std::int64_t chat_id, std::int64_t reply_id, std::string text, bool no_link_preview) {
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

void tdscript::Client::send_reply(std::int64_t chat_id, std::int64_t reply_id, std::string text) {
  send_text(chat_id, reply_id, std::move(text), false);
}

void tdscript::Client::send_start(std::int64_t chat_id, std::int64_t bot_id, const std::string& link) {
  send_start(chat_id, bot_id, link, 9);
}

void tdscript::Client::send_start(std::int64_t chat_id, std::int64_t bot_id, const std::string& link, int limit) {
  if (werewolf_bot_warning) { return; }
  if (has_owner.at(chat_id)) { return; }
  if (!need_extend.at(chat_id)) { return; }
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

void tdscript::Client::send_extend(std::int64_t chat_id) {
  if (werewolf_bot_warning) { return; }
  if (!need_extend.at(chat_id)) { return; }
  if (!has_owner.at(chat_id)) { return; }
  if (player_count.at(chat_id) >= 5) { return; }
  if (pending_extend_messages.count(chat_id) != 0 && pending_extend_messages[chat_id].size() > 10) { return; }
  if (last_extent_at.count(chat_id) > 0 && std::time(nullptr) - last_extent_at.at(chat_id) < 5) { return; }
  last_extent_at[chat_id] = std::time(nullptr);
  send_text(chat_id, EXTEND_TEXT);
}

void tdscript::Client::delete_messages(std::int64_t chat_id, std::vector<std::int64_t> message_ids) {
  send_request(td::td_api::make_object<td::td_api::deleteMessages>(chat_id, std::move(message_ids), true));
}

void tdscript::Client::get_message(std::int64_t chat_id, std::int64_t msg_id, std::function<void(tdo_ptr)> callback) {
  if (chat_id == 0 || msg_id == 0) { return; }
  std::vector<std::int64_t> message_ids = { msg_id };
  send_request(td::td_api::make_object<td::td_api::getMessages>(chat_id, std::move(message_ids)), std::move(callback));
}

void tdscript::Client::forward_message(std::int64_t chat_id, std::int64_t from_chat_id, std::int64_t msg_id) {
  forward_message(chat_id, from_chat_id, msg_id, true);
}

void tdscript::Client::forward_message(std::int64_t chat_id, std::int64_t from_chat_id, std::int64_t msg_id, bool copy) {
  auto send_message = td::td_api::make_object<td::td_api::forwardMessages>();
  send_message->chat_id_ = chat_id;
  send_message->from_chat_id_ = from_chat_id;
  send_message->message_ids_ = { msg_id };
  send_message->send_copy_ = copy;
  send_message->remove_caption_ = false;
  send_request(std::move(send_message));
}

void tdscript::Client::send_https_request(const std::string& host, const std::string& path, const callback_t& f) { // NOLINT(readability-convert-member-functions-to-static)
  dns_client.query(host, TDSCRIPT_WITH_IPV6 ? 28 : 1, [this, host, path, f](std::vector<std::string> data) {
    if (!data.empty()) {
      dns_client.send_https_request(TDSCRIPT_WITH_IPV6 ? AF_INET6 : AF_INET, data[0], host, path, f);
    }
  });
}

void tdscript::Client::loop() {
  while(!stop) {
    process_tasks(std::time(nullptr));

    process_response(td_client_manager->receive(authorized ? RECEIVE_TIMEOUT_S : AUTHORIZE_TIMEOUT_S));

    dns_client.receive();
  }
}

void tdscript::Client::process_tasks(std::time_t time) {
  if (last_task_at == time) { return; }
  last_task_at = time;

  // take out the current tasks and process
  for (const auto& task : task_queue[time]) {
    task();
  }
  task_queue[time].clear();

  // confirm the /extend
  for (const auto kv : player_count) {
    auto chat_id = kv.first;
    if (pending_extend_messages.count(chat_id) != 0 && !pending_extend_messages[chat_id].empty()) {
      send_extend(chat_id);
    }
    if (last_extent_at.at(chat_id) && time - last_extent_at[chat_id] > EXTEND_TIME) {
      send_extend(chat_id);
    }
  }

  // confirm the number of players
  for (const auto kv : players_message) {
    auto chat_id = kv.first;
    auto msg_id = kv.second;
    if (time - last_extent_at[chat_id] > EXTEND_TIME / 2 && time % 7 == 0) {
      get_message(chat_id, msg_id, [this, chat_id](tdo_ptr update) {
        if (td::td_api::messages::ID == update->get_id()) {
          auto& msgs = static_cast<td::td_api::messages*>(update.get())->messages_;
          if (msgs.size() != 1) { return; }
          if (td::td_api::messageText::ID == msgs[0]->content_->get_id()) {
            auto& entities = static_cast<td::td_api::messageText*>(msgs[0]->content_.get())->text_->entities_;
            player_ids[chat_id].clear();
            for (auto& entity : entities) {
              if (td::td_api::textEntityTypeMentionName::ID == entity->type_->get_id()) {
                std::int64_t mention_id = static_cast<td::td_api::textEntityTypeMentionName*>(entity->type_.get())->user_id_;
                player_ids[chat_id].push_back(mention_id);
              }
            }

            process_message(std::move(msgs[0]));
          }
        }
      });
    }
  }

  save();
}

void tdscript::Client::process_response(td::ClientManager::Response response) {
  if (!response.object) { return; }
  auto update = std::move(response.object);
  std::cout << "receive " << response.request_id << ": " << td::td_api::to_string(update) << '\n';

  if (response.request_id != 0) {
    if (query_callbacks.count(response.request_id) > 0) {
      query_callbacks[response.request_id](std::move(update));
      query_callbacks.erase(response.request_id);
    }
    return;
  }

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
        send_request(td::td_api::make_object<td::td_api::setAuthenticationPhoneNumber>(std::getenv("TG_PHONE_NUMBER"), nullptr));
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
    auto msg = static_cast<td::td_api::updateMessageContent*>(update.get());
    auto msg_id = msg->message_id_;
    auto chat_id = msg->chat_id_;
    if (td::td_api::messageText::ID == msg->new_content_->get_id()) {
      auto text = static_cast<td::td_api::messageText*>(msg->new_content_.get())->text_->text_;
      process_message(chat_id, msg_id, 0, text, "");
    }
  }
}

void tdscript::Client::process_message(td::td_api::object_ptr<td::td_api::message> msg) {
  if (msg && td::td_api::messageSenderUser::ID == msg->sender_id_->get_id()) {
    auto chat_id = msg->chat_id_;
    auto user_id = static_cast<td::td_api::messageSenderUser*>(msg->sender_id_.get())->user_id_;
    std::string text;
    if (msg->content_ && td::td_api::messageText::ID == msg->content_->get_id()) {
      text = static_cast<td::td_api::messageText*>(msg->content_.get())->text_->text_;
    }
    std::string link;
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

void tdscript::Client::process_message(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, const std::string& text, const std::string& link) {
  std::cout << "msg(" << msg_id << "): " << user_id << " -> " << chat_id << ", " << text << "\n\n";

  if (user_id == USER_ID_WEREWOLF || user_id == 0) {
    process_werewolf(chat_id, msg_id, user_id, text, link);
  }

  process_wiki(chat_id, text);

  if (text == EXTEND_TEXT) {
    pending_extend_messages[chat_id].push_back(msg_id);
  }

  const std::regex starting_regex("游戏启动中");
  std::smatch starting_match;
  if (std::regex_search(text, starting_match, starting_regex)) {
    for (const auto& at : at_list[chat_id]) { send_text(chat_id, at); } at_list[chat_id].clear();
    select_one_randomly(STICKS_STARTING, [this, chat_id](std::size_t i) {
      while (true) {
        std::cout << "selected stick, number: " << i << '\n';
        std::int64_t from_chat_id = STICKS_STARTING[i][0];
        std::int64_t from_msg_id = STICKS_STARTING[i][1];
        if (STICKS_STARTING[i].size() > 2) {
          std::int64_t reply_msg_id = STICKS_STARTING[i][2];
          std::int64_t reply_user_id = STICKS_REPLY_TO.at(reply_msg_id);
          if (std::count(player_ids[chat_id].begin(), player_ids[chat_id].end(), reply_user_id)) {
            if (KEY_PLAYER_IDS.count(reply_user_id)) {
              forward_message(chat_id, from_chat_id, from_msg_id, false);
              return send_reply(chat_id, reply_msg_id, KEY_PLAYER_IDS.at(reply_user_id));
            }
          }
          i = (i + 1) % STICKS_STARTING.size();
          continue;
        }
        return forward_message(chat_id, from_chat_id, from_msg_id);
      }
    });
  }

  save_flag = true;
}

void tdscript::Client::process_werewolf(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, const std::string& text, const std::string& link) {
  if (user_id != 0 && text == "Please do not spam me. Next time is automated ban.") {
    werewolf_bot_warning = true;
    task_queue[std::time(nullptr) + 99].push_back([]() { werewolf_bot_warning = false; });
  }

  if (user_id && !link.empty()) {
    need_extend[chat_id] = 1;
    send_start(chat_id, user_id, link);
  }

  const std::regex players_regex("^#players: (\\d+)");
  std::smatch players_match;
  if (std::regex_search(text, players_match, players_regex)) {
    player_count[chat_id] = std::stoi(players_match[1]);

    if (user_id && msg_id) {
      if (players_message[chat_id] != msg_id) {
        last_extent_at[chat_id] = std::time(nullptr);
      }
      players_message[chat_id] = msg_id;
    }
  }

  if (players_message.count(chat_id) > 0 && msg_id == players_message.at(chat_id)) {
    const std::regex owner_regex("lccc");
    std::smatch owner_match;
    if (std::regex_search(text, owner_match, owner_regex)) {
      has_owner[chat_id] = 1;
    }
  }

  for (const auto& player : KEY_PLAYERS) {
    if (text.find(player.first) != std::string::npos) {
      if (std::count(at_list[chat_id].begin(), at_list[chat_id].end(), player.second) == 0) {
        at_list[chat_id].push_back(player.second);
      }
    }
  }

  const std::regex remain_regex("还有 1 分钟|还剩 \\d+ 秒");
  std::smatch remain_match;
  if (std::regex_search(text, remain_match, remain_regex)) {
    delete_messages(chat_id, pending_extend_messages[chat_id]);
    pending_extend_messages[chat_id].clear();
    pending_extend_messages[chat_id].push_back(msg_id);
  }

  const std::regex remain_reply_regex("现在又多了 123 秒时间");
  std::smatch remain_reply_match;
  if (std::regex_search(text, remain_reply_match, remain_reply_regex)) {
    pending_extend_messages[chat_id].push_back(msg_id);
    delete_messages(chat_id, pending_extend_messages[chat_id]);
    pending_extend_messages[chat_id].clear();
  }

  const std::regex cancel_regex("游戏取消|目前没有进行中的游戏|游戏启动中|夜幕降临|第\\d+天");
  std::smatch cancel_match;
  if (std::regex_search(text, cancel_match, cancel_regex)) {
    player_count[chat_id] = 0;
    has_owner[chat_id] = 0;
    pending_extend_messages[chat_id].clear();
    last_extent_at[chat_id] = 0;
    players_message[chat_id] = 0;
    need_extend[chat_id] = 0;
  }
}

void tdscript::Client::process_wiki(std::int64_t chat_id, const std::string &text) {
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
  std::string host = lang + ".wikipedia.org";
  if (title.empty()) {
    send_https_request(host, "/w/api.php?action=query&format=json&list=random&rnnamespace=0",
    [this, host, lang, chat_id](const std::vector<std::string>& res) {
      std::string body = res[1];
      rapidjson::Document data; data.Parse(body.c_str());
      if (data["query"].IsObject() && data["query"]["random"].IsArray() && data["query"]["random"][0].IsObject()) {
        std::string title = data["query"]["random"][0]["title"].GetString();
        std::cout << "wiki random title: " << title << '\n';
        process_wiki(chat_id, lang, title);
      }
    });
  } else {
    process_wiki(chat_id, lang, title.erase(0, title.find_first_not_of(' ')));
  }
}

// TODO: fix https://en.wikipedia.org/wiki/Civil_War
// TODO: implement https://github.com/lccxz/tg-script/blob/master/tg.rb#L278
void tdscript::Client::process_wiki(std::int64_t chat_id, const std::string &lang, const std::string &title) {
  std::string host = lang + ".wikipedia.org";
  send_https_request(host, "/w/api.php?action=parse&format=json&page=" + tdscript::urlencode(title),
  [this, chat_id, lang, title](const std::vector<std::string>& res) {
    std::string body = res[1];
    rapidjson::Document data; data.Parse(body.c_str());
    if (data["error"].IsObject() && data["error"]["info"].IsString()) {
      std::cout << data["error"]["info"].GetString() << '\n';
      send_text(chat_id, data["error"]["info"].GetString(), true);
    }
    if (data["parse"].IsObject() && data["parse"]["text"].IsObject() && data["parse"]["text"]["*"].IsString()) {
      std::string text = data["parse"]["text"]["*"].GetString();

      xmlInitParser();
      xmlDocPtr doc = xmlParseMemory(text.c_str(), text.length()); // NOLINT(cppcoreguidelines-narrowing-conversions)
      if (doc == nullptr) {
        fprintf(stderr, "Error: unable to parse string: \"%s\"\n", text.c_str());
        return;
      }

      // xmlDocDump(stdout, doc);
      std::string article_desc;
      std::vector<std::string> desc_find_kws = { "。", " is ", " was ", "." };
      xmlNode *root = xmlDocGetRootElement(doc);
      for (xmlNode *node = root; node; node = node->next ? node->next : node->children) {
        std::cout << "node name: '" << node->name << "'\n";
        if (tdscript::xmlCheckEq(node->name, "p")) {
          std::string content = tdscript::xmlNodeGetContentStr(node);
          for (const auto& kw : desc_find_kws) {
            if (content.find(kw) != std::string::npos) {
              article_desc = content;
              break;
            }
          }
        }
        if (!article_desc.empty()) {
          break;
        }
      }

      article_desc = std::regex_replace(article_desc, std::regex(R"(\[\d+\])"), "");
      std::cout << "'" << title << "': '" << article_desc << "'\n";

      send_text(chat_id, article_desc, true);

      xmlFreeDoc(doc);
      xmlCleanupParser();
    }
  });
}


void tdscript::quit(int signum) { stop = true; printf("received the signal: %d\n", signum); }

void tdscript::check_environment(const char *name) {
  if (std::getenv(name) == nullptr || std::string(std::getenv(name)).empty()) {
    throw std::invalid_argument(std::string("$").append(name).append(" empty"));
  }
}

template<typename T>
void tdscript::select_one_randomly(const std::vector<T>& arr, const std::function<void(std::size_t)>& f) {
  if (!arr.empty()) { f(rand() % arr.size()); }  // NOLINT(cert-msc50-cpp)
}

bool tdscript::xmlCheckEq(const xmlChar *a, const char *b) {
  int i = 0;
  do {
    if (a[i] != b[i]) { return false; }
    i++;
  } while (a[i] && b[i]);
  return true;
}

std::string tdscript::xmlNodeGetContentStr(const xmlNode *node) {
  std::stringstream content;
  content << xmlNodeGetContent(node);
  return content.str();
}

template<typename Tk, typename Tv>
std::string tdscript::m2s(std::unordered_map<Tk, Tv> map) {
  std::string line;
  for (const auto e : map) {
    line.append(std::to_string(e.first)).append(":").append(std::to_string(e.second)).append(",");
  }
  return line;
}

template<typename Tk, class Tv>
std::string tdscript::ma2s(std::unordered_map<Tk, Tv> map) {
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

std::string tdscript::urlencode(const std::string& str) {
  std::string encode;

  const char *s = str.c_str();
  for (int i = 0; s[i]; i++) {
    char ci = s[i];
    if ((ci >= 'a' && ci <= 'z') ||
        (ci >= 'A' && ci <= 'Z') ||
        (ci >= '0' && ci <= '9') ) { // allowed
      encode.push_back(ci);
    } else if (ci == ' ') {
      encode.push_back('+');
    } else {
      encode.append("%").append(char_to_hex(ci));
    }
  }

  return encode;
}

std::string tdscript::char_to_hex(char c) {
  std::uint8_t n = c;
  std::string res;
  res.append(HEX_CODES[n / 16]);
  res.append(HEX_CODES[n % 16]);
  return res;
}

void tdscript::save() {
  if (!data_ready) { return; }
  if (!save_flag) { return; }
  save_flag = false;

  std::ofstream ofs;
  ofs.open (SAVE_FILENAME, std::ofstream::out | std::ofstream::binary);

  ofs << m2s(player_count) << '\n';
  ofs << m2s(has_owner) << '\n';
  ofs << ma2s(pending_extend_messages) << '\n';
  ofs << m2s(last_extent_at) << '\n';
  ofs << m2s(players_message) << '\n';
  ofs << m2s(need_extend) << '\n';

  ofs.close();
}

void tdscript::load() {
  std::ifstream file(SAVE_FILENAME);
  if (!file.good()) {
    data_ready = true;
    return ;
  }

  std::regex kv_regex("([^:]*):([^,]*),");
  std::regex stick_regex("([^|]*)\\|");
  std::string line;
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
          pending_extend_messages[std::stoll(key)].push_back(std::stoll(a));
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
