// created by lccc 12/04/2021, no copyright

#include "tdscript/client.h"

#include "libdns/client.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <iostream>
#include <regex>
#include <fstream>
#include <utility>

namespace tdscript {
  const std::int64_t USER_ID_WEREWOLF = 365229753;  // @shenl_werewolfbot 365229753 or @werewolfbot 175844556;
  const std::string USER_NAME_WEREWOLF = "shenl_werewolfbot";
  const std::string USER_NAME_MYSELF = "lcc";
  const std::string START_TEXT = "/startchaos@" + USER_NAME_WEREWOLF;
  const std::int32_t EXTEND_TIME = 123;
  const std::string EXTEND_TEXT = std::string("/extend@") + USER_NAME_WEREWOLF + " " + std::to_string(EXTEND_TIME);
  const std::vector<std::vector<std::int64_t>> STICKS_DONE = {
    { -681384622, 94371840 }
  };
  const std::vector<std::vector<std::int64_t>> STICKS_DONE_FAIL = {
    { -681384622, 95420416 }
  };
  const std::vector<std::vector<std::int64_t>> STICKS_STARTING = {
      { -681384622, 357654593536 }, { -681384622, 357655642112 },
      { -1001098611371, 2753360297984, 2753361346560 },
      { -681384622, 356104798208 },
      { 981032009, 357662982144, 2753361346560 },
      { 981032009, 357661933568, 2753361346560 },
      { 981032009, 357660884992, 2753361346560 },
      { 981032009, 357659836416, 2753361346560 },
      { 981032009, 357658787840, 2753361346560 },
  };
  const std::map<std::int64_t, std::int64_t> STICKS_REPLY_TO = { {2753361346560, 981032009 } };  // msg_id, user_id
  const std::map<std::int64_t, std::string> KEY_PLAYER_IDS = { { 981032009, "@TalkIce" } };
  const std::map<std::string, std::string> KEY_PLAYERS = { };  // { { "KMM", "@JulienKM" } };

  const std::int64_t USER_ID_VG = 195053715;

  bool stop = false;

  const std::string SAVE_FILENAME = std::string(std::getenv("HOME")) + "/." + USER_NAME_MYSELF + "-tdscript-save.json";
  bool save_flag = false;
  bool data_ready = false;
  std::map<std::int64_t, std::int32_t> player_count;
  std::map<std::int64_t, std::uint8_t> has_owner;
  std::map<std::int64_t, std::vector<std::int64_t>> pending_extend_messages;
  std::map<std::int64_t, std::uint64_t> last_extent_at;
  std::map<std::int64_t, std::int64_t> players_message;
  std::map<std::int64_t, std::uint8_t> need_extend;
  std::map<std::int64_t, std::uint8_t> started;
  std::map<std::int64_t, std::uint64_t> last_done_at;

  std::mt19937 rand_engine = std::mt19937((std::random_device())());

  std::map<std::int64_t, std::uint64_t> last_start_at;
  std::map<std::int64_t, std::vector<std::string>> at_list;  // the '@' list
  std::map<std::int64_t, std::vector<std::int64_t>> player_ids;
  bool werewolf_bot_warning = false;
  bool werewolf_bot_banned = false;

  std::time_t last_task_at = -1;
  std::map<std::uint64_t, std::vector<std::function<void()>>> task_queue;
}

void tdscript::Client::send_parameters() {
  auto parameters = td::td_api::make_object<td::td_api::tdlibParameters>();
  parameters->database_directory_ = std::string(std::getenv("HOME")) + "/." + USER_NAME_MYSELF + "-tdscript";
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

void tdscript::Client::send_html(std::int64_t chat_id, std::int64_t reply_id, std::string text, bool no_link_preview, bool no_html) {
  auto send_message = td::td_api::make_object<td::td_api::sendMessage>();
  send_message->chat_id_ = chat_id;
  send_message->reply_to_message_id_ = reply_id;
  auto message_content = td::td_api::make_object<td::td_api::inputMessageText>();
  message_content->text_ = td::td_api::make_object<td::td_api::formattedText>();
  if (!no_html) {
    std::vector<td::td_api::object_ptr<td::td_api::textEntity>> entities;
    std::vector<std::pair<std::string, std::function<td::td_api::object_ptr<td::td_api::TextEntityType>()>>> styles = {
        { "b", []() { return td::td_api::make_object<td::td_api::textEntityTypeBold>(); } },
        { "i", []() { return td::td_api::make_object<td::td_api::textEntityTypeItalic>(); } },
        { "s", []() { return td::td_api::make_object<td::td_api::textEntityTypeStrikethrough>(); } },
        { "u", []() { return td::td_api::make_object<td::td_api::textEntityTypeUnderline>(); } },
        { "code", []() { return td::td_api::make_object<td::td_api::textEntityTypeCode>(); } },
    };
    auto erase_tag = [&text,&entities](std::size_t offset, std::size_t len) {
      text.erase(offset, len);
      for (auto& entity : entities) {
        if (entity->offset_ > offset) {
          entity->offset_ -= (int)len;
        } else if (entity->offset_ + entity->length_ > offset) {
          entity->length_ -= (int)len;
        }
      }
    };
    for (const auto& style : styles) {
      std::string tag_s = "<" + style.first + ">";
      std::string tag_e = "</" + style.first + ">";
      std::size_t offset1, offset2 = 0;
      do {
        offset1 = text.find(tag_s, offset2);
        if (offset1 == std::string::npos) { break; }
        erase_tag(offset1, tag_s.length());
        auto entity = td::td_api::make_object<td::td_api::textEntity>();
        entity->offset_ = ustring_count(text, 0, offset1);
        offset2 = text.find(tag_e, offset1);
        if (offset2 == std::string::npos) { break; }
        erase_tag(offset2, tag_e.length());
        entity->length_ = ustring_count(text, offset1, offset2);
        entity->type_ = style.second();
        entities.push_back(std::move(entity));
      } while (true);
    }
    message_content->text_->entities_ = std::move(entities);
  }
  message_content->text_->text_ = std::move(text);
  message_content->disable_web_page_preview_ = no_link_preview;
  send_message->input_message_content_ = std::move(message_content);
  send_request(std::move(send_message));
}

void tdscript::Client::send_start(std::int64_t chat_id) {
  if (werewolf_bot_banned) { return; }
  if (werewolf_bot_warning) { return; }
  if (need_extend.count(chat_id) > 0 && need_extend.at(chat_id)) { return; }
  if (started.count(chat_id) > 0 && started.at(chat_id)) { return; }
  if (last_done_at.count(chat_id) > 0 && std::time(nullptr) - last_done_at.at(chat_id) < 99) { return; }
  if (last_start_at.count(chat_id) > 0 && std::time(nullptr) - last_start_at.at(chat_id) < 9) { return; }
  last_start_at[chat_id] = std::time(nullptr);

  send_text(chat_id, START_TEXT);
}

void tdscript::Client::send_join(std::int64_t chat_id, std::int64_t bot_id, const std::string& link, int limit) {
  if (werewolf_bot_banned) { return; }
  if (werewolf_bot_warning) { return; }
  if (has_owner.count(chat_id) > 0 && has_owner.at(chat_id)) { return; }
  if (need_extend.count(chat_id) == 0 || !need_extend.at(chat_id)) { return; }
  if (limit <= 0) { return ; }

  std::regex param_regex("\\?start=(.*)");
  std::smatch param_match;
  std::string param;
  if (std::regex_search(link, param_match, param_regex)) {
    param = param_match[1];
  }

  auto send_message = td::td_api::make_object<td::td_api::sendBotStartMessage>();
  send_message->chat_id_ = bot_id;
  send_message->bot_user_id_ = bot_id;
  send_message->parameter_ = param;
  send_request(std::move(send_message));

  // add to the task queue, resend after 9 seconds, limit 9(default)
  task_queue[std::time(nullptr) + 9].push_back([this, chat_id, bot_id, link, limit]() {
    send_join(chat_id, bot_id, link, limit - 1);
  });
}

void tdscript::Client::send_extend(std::int64_t chat_id) {
  if (werewolf_bot_banned) { return; }
  if (werewolf_bot_warning) { return; }
  if (need_extend.count(chat_id) == 0 || !need_extend.at(chat_id)) { return; }
  if (has_owner.count(chat_id) == 0 || !has_owner.at(chat_id)) { return; }
  if (player_count.count(chat_id) > 0 && player_count.at(chat_id) >= 5) { return; }
  if (pending_extend_messages.count(chat_id) > 0 && pending_extend_messages[chat_id].size() > 9) { return; }
  if (last_extent_at.count(chat_id) > 0 && std::time(nullptr) - last_extent_at.at(chat_id) < 5) { return; }
  last_extent_at[chat_id] = std::time(nullptr);
  send_text(chat_id, EXTEND_TEXT);
}

void tdscript::Client::process_tasks(std::time_t time) {
  if (last_task_at == time) { return; }
  last_task_at = time;

  // take out the current tasks and process
  for (const auto& task : task_queue[time]) {
    task();
  }
  task_queue[time].clear();

  // check the /start
  for (const auto kv : last_done_at) {
    send_start(kv.first);
  }

  // confirm the /extend
  for (const auto kv : player_count) {
    auto chat_id = kv.first;
    if (pending_extend_messages.count(chat_id) != 0 && !pending_extend_messages[chat_id].empty()) {
      send_extend(chat_id);
    }
    if (last_extent_at.count(chat_id) > 0 && time - last_extent_at[chat_id] > EXTEND_TIME) {
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
  std::cout << "receive " << response.request_id << ": " << td::td_api::to_string(update) << std::endl;

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
      process_werewolf(chat_id, msg_id, 0, text, "");
      process_message(chat_id, msg_id, 0, text);
    }
  }
}

void tdscript::Client::process_message(td::td_api::object_ptr<td::td_api::message> msg) {
  if (msg && td::td_api::messageSenderUser::ID == msg->sender_id_->get_id()) {
    auto chat_id = msg->chat_id_;
    auto user_id = static_cast<td::td_api::messageSenderUser*>(msg->sender_id_.get())->user_id_;
    auto reply_id = msg->reply_to_message_id_;
    std::string text;
    if (msg->content_ && td::td_api::messageText::ID == msg->content_->get_id()) {
      text = static_cast<td::td_api::messageText*>(msg->content_.get())->text_->text_;
    }
    if (msg->content_ && td::td_api::messagePhoto::ID == msg->content_->get_id()) {
      text = static_cast<td::td_api::messagePhoto*>(msg->content_.get())->caption_->text_;
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

    if (user_id == USER_ID_WEREWOLF || user_id == 0) {
      process_werewolf(chat_id, msg->id_, user_id, text, link);
    }

    process_wiki(chat_id, text);

    if (reply_id && std::regex_search(text, std::regex("\\/[^ ]+"))) {
      process_dict(chat_id, reply_id, text);
    } else {
      process_dict(chat_id, text);
    }

    process_message(chat_id, msg->id_, user_id, text);
  }
}

void tdscript::Client::process_message(std::int64_t chat_id, std::int64_t msg_id, std::int64_t user_id, const std::string& text) {
  std::cout << "msg(" << msg_id << "): " << user_id << " -> " << chat_id << ", " << text << "\n" << std::endl;

  if (text == EXTEND_TEXT) {
    pending_extend_messages[chat_id].push_back(msg_id);
  }

  const std::regex starting_regex("游戏启动中");
  if (std::regex_search(text, starting_regex)) {
    if (user_id == USER_ID_WEREWOLF) {
      for (const auto &at : at_list[chat_id]) { send_text(chat_id, at); }
    }
    select_one_randomly(STICKS_STARTING, [this, chat_id](std::size_t i) {
      while (true) {
        std::cout << "selected stick, number: " << i << std::endl;
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

    if (user_id == USER_ID_WEREWOLF) {
      started[chat_id] = 1;
    }
  }

  if (user_id == USER_ID_VG) {
    if (text.find("Work is finished, my lord!") != std::string::npos) {
      send_text(chat_id, "/work");
    }
    if (text.find("You start to work and harvest") != std::string::npos) {
      send_text(chat_id, "Sell bread💰");
    }
    if (text.find("Your fields are filled.") != std::string::npos) {
      send_text(chat_id, "/harvest");
    }
    if (text.find("You sold") != std::string::npos && text.find("/work") != std::string::npos) {
      send_text(chat_id, "/work");
    }
    if (text.find("Bandits attacked a village.") != std::string::npos) {
      send_text(chat_id, "Run quest🗡");
    }
    if (text.find("These bandits were cowards!") != std::string::npos
        || text.find("Your squad came to the rescue") != std::string::npos) {
      send_text(chat_id, "⭐️⭐️⭐️Save the village");
    }
    if (text.find("The bandits were some strong guys") != std::string::npos) {
      send_text(chat_id, "Send reinforcements! 🗡");
    }
    if (text.find("Your opponent is") == 0) {
      std::smatch need_match;
      std::smatch earn_match;
      if (std::regex_search(text, need_match, std::regex("you need (\\d+)"))
            && std::regex_search(text, earn_match, std::regex("be able to earn (\\d+)"))) {
        int need_num = std::stoi(need_match[1]);
        int earn_num = std::stoi(earn_match[1]);
        if (earn_num - need_num > 10) {
          send_text(chat_id, "Attack!⚔");
        } else {
          send_text(chat_id, "Search opponent👁");
        }
      }
    }
    if (text.find("You won the battle") == 0 || text.find("Our troops, without any problems") == 0) {
      send_text(chat_id, "Search opponent👁");
    }
    if (text.find("you need to send reinforcements") != std::string::npos) {
      send_text(chat_id, "Send reinforcement!🗡");
    }
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
    send_join(chat_id, user_id, link);
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

    at_list[chat_id].clear();
    for (const auto& player : KEY_PLAYERS) {
      if (text.find(player.first) != std::string::npos) {
        at_list[chat_id].push_back(player.second);
      }
    }
  }

  if (players_message.count(chat_id) > 0 && msg_id == players_message.at(chat_id)) {
    const std::regex owner_regex(USER_NAME_MYSELF);
    if (std::regex_search(text, owner_regex)) {
      has_owner[chat_id] = 1;
    }
  }

  if (user_id != USER_ID_WEREWOLF) {
    return;
  }

  const std::regex remain_regex("还有 1 分钟|还剩 \\d+ 秒");
  if (std::regex_search(text, remain_regex)) {
    delete_messages(chat_id, pending_extend_messages[chat_id]);
    pending_extend_messages[chat_id].clear();
    pending_extend_messages[chat_id].push_back(msg_id);
  }

  const std::regex remain_reply_regex("现在又多了 123 秒时间");
  if (std::regex_search(text, remain_reply_regex)) {
    pending_extend_messages[chat_id].push_back(msg_id);
    delete_messages(chat_id, pending_extend_messages[chat_id]);
    pending_extend_messages[chat_id].clear();
  }

  const std::regex cancel_regex("游戏取消|目前没有进行中的游戏|游戏启动中|夜幕降临|第\\d+天");
  if (std::regex_search(text, cancel_regex)) {
    player_count[chat_id] = 0;
    has_owner[chat_id] = 0;
    pending_extend_messages[chat_id].clear();
    last_extent_at[chat_id] = 0;
    players_message[chat_id] = 0;
    need_extend[chat_id] = 0;
  }

  const std::regex done_regex("游戏取消|游戏进行了：\\d+:\\d+:\\d+");
  if (std::regex_search(text, done_regex)) {
    last_done_at[chat_id] = std::time(nullptr);
    started[chat_id] = 0;

    std::smatch done_match;
    if (std::regex_search(text, done_match, std::regex(USER_NAME_MYSELF + ":.* ([^ ]*)\n"))) {
      std::string result = done_match[1];
      bool fail = result == "失败";
      std::cout << "Game done " << (fail ? "lose" : "win") << ", '" << result << "'" << std::endl;
      select_one_randomly(fail ? STICKS_DONE_FAIL : STICKS_DONE, [this, fail, chat_id](std::size_t i) {
        auto stick = fail ? STICKS_DONE_FAIL[i] : STICKS_DONE[i];
        std::int64_t from_chat_id = stick[0];
        std::int64_t from_msg_id = stick[1];
        forward_message(chat_id, from_chat_id, from_msg_id);
      });
    }
  }
}

void tdscript::Client::process_wiki(std::int64_t chat_id, const std::string &text, const std::string& kw, const std::string& domain) {
  std::regex lang_regex("^\\/(.{0,2})" + kw);
  std::regex title_regex(kw + "(.*)$");
  std::smatch lang_match;
  std::smatch title_match;
  std::string lang;
  std::string title;
  if (std::regex_search(text, lang_match, lang_regex) && std::regex_search(text, title_match, title_regex)) {
    lang = lang_match[1];
    title = title_match[1];
    trim(title);
  } else {
    return;
  }
  if (lang.empty()) {
    lang = "en";
  }
  std::string host = lang + "." + domain + ".org";
  if (title.empty()) {
    if (kw == "wiki") {
      wiki_get_random_title(lang, [this, chat_id, lang](auto title) {
        process_wiki(chat_id, lang, title);
      });
    } else if (kw == "dict") {
      dict_get_random_title(lang, [this, chat_id, lang](auto title) {
        process_dict(chat_id, lang, title);
      });
    }
  } else {
    if (kw == "wiki") {
      process_wiki(chat_id, lang, title);
    } else if (kw == "dict") {
      process_dict(chat_id, lang, title);
    }
  }
}

void tdscript::Client::process_dict(std::int64_t chat_id, const std::string &lang, const std::string &title) {
  dict_get_content(lang, title, [this, chat_id](auto desc) {
    if (desc.size() == 1) {
      send_html(chat_id, std::string(desc[0].begin(), desc[0].end()));
    } else if (desc.size() > 1) {
      std::string type(desc[0].begin(), desc[0].end());
      if (type == "Pronunciation") {
        int duration = std::ceil(std::stof(std::string(desc[1].begin(), desc[1].end())));
        std::string data(desc[2].begin(), desc[2].end());
        std::string filename = "/tmp/" + type + std::to_string(chat_id) + std::to_string(std::time(nullptr));
        filename += std::to_string(std::uniform_int_distribution<std::uint64_t>(0, 99999999)(rand_engine));
        std::ofstream ofs;
        ofs.open(filename, std::ofstream::out | std::ofstream::binary);
        ofs << data;
        ofs.close();
        std::string waveform;
        for (int i = 0; i < 64; i++) {
          waveform.push_back(std::uniform_int_distribution<char>(0, '\x1F')(rand_engine));
        }
        send_voice(chat_id, duration, filename, waveform);
        task_queue[std::time(nullptr) + 9].push_back([filename]() {
          std::filesystem::remove(filename);
        });
      }
    }
  });
}

void tdscript::Client::wiki_get_content(const std::string& lang, const std::string& title, const std::function<void(std::string)>& f) {
  wiki_get_data(lang, title, ".wikipedia.org", [this, f, lang, title](const std::string& res_body) {
    rapidjson::Document data; data.Parse(res_body.c_str());
    if (data.HasMember("error")) {
      std::string error_info = data["error"]["info"].GetString();
      f(data["error"]["info"].GetString());
    } else if (data.HasMember("parse")) {
      std::string text = data["parse"]["text"]["*"].GetString();

      xmlInitParser();
      xmlDocPtr doc = xmlParseMemory(text.c_str(), (int)text.length());
      if (doc == nullptr) {
        std::cerr << "Error: unable to parse string: '" << text << "'" << std::endl;
        return;
      }
      xmlDocDump(stdout, doc);
      xmlNode* root = xmlDocGetRootElement(doc);
      xml_clear_wiki(root);

      std::vector<std::string> desc_find_kws = { "。", " is ", " was ", "." };
      std::string article_desc;
      xml_each_next(root->children, [this, lang, f, desc_find_kws, &article_desc](auto node) {
        std::string node_class = xml_get_prop(node, "class");
        std::cout << "node: '" << node->name << (node_class.empty() ? "" : "." + node_class) << "'" << std::endl;
        if (xml_check_eq(node->name, "div") && node_class == "redirectMsg") {
          xml_each_child(node->children, [this, lang, f](auto child) {
            if (xml_check_eq(child->name, "a")) {
              std::string redirect_title = xml_get_prop(child, "title");
              std::cout << "redirect title: " << redirect_title << std::endl;
              wiki_get_content(lang, redirect_title, f);
              return 1;
            }
            return 0;
          });
          return 1;
        }
        if (xml_check_eq(node->name, "p")) {
          std::string content = xml_get_content(node); trim(content);
          std::cout << "content: '" << content << "'" << std::endl;
          if (content[content.length() - 1] == ':') {  // content is "xxx refer to:"
            xml_each_next(node, [&content, &article_desc](auto next_node) {
              std::cout << "next node name: '" << next_node->name << "'" << std::endl;
              if (tdscript::xml_check_eq(next_node->name, "ul")) {
                content.append("\n").append(xml_get_content(next_node));
                article_desc = content; trim(article_desc);
                return 1;
              }
              return 0;
            });
            return 1;
          }
          for (const auto& kw : desc_find_kws) {
            if (content.find(kw) != std::string::npos) {
              article_desc = content;
              break;
            }
          }
        }
        if (!article_desc.empty()) {
          return 1;
        }
        return 0;
      });

      if (!article_desc.empty()) {
        article_desc = std::regex_replace(article_desc, std::regex(R"(\[[^\]]*\])"), "");
        std::cout << "'" << title << "': '" << article_desc << "'" << std::endl;

        f(article_desc);
      }

      xmlFreeDoc(doc);
      xmlCleanupParser();
    }
  });
}

void tdscript::Client::dict_get_content(const std::string& lang, const std::string& title, const std::vector<std::string>& references, const callback_t& f) {
  std::string trim_title = std::regex_replace(title, std::regex("\\/.*"), "");
  wiki_get_data(lang, trim_title, ".wiktionary.org", [this, f, lang, references, trim_title](const std::string& res_body) {
    rapidjson::Document data; data.Parse(res_body.c_str());
    if (data.HasMember("error")) {
      std::string error_info = data["error"]["info"].GetString();
      f({ std::vector<char>(error_info.begin(), error_info.end()) });
    } else if (data.HasMember("parse")) {
      std::string text = data["parse"]["text"]["*"].GetString();

      xmlInitParser();
      xmlDocPtr doc = xmlParseMemory(text.c_str(), (int)text.length());
      if (doc == nullptr) {
        std::cerr << "Error: unable to parse string: '" << text << "'" << std::endl;
        return;
      }
      xmlDocDump(stdout, doc);
      xmlNode* root = xmlDocGetRootElement(doc);
      xml_clear_wiki(root);

      // check if this page doesn't have any define
      if (!xml_each_next(root->children, [](auto node) { return xml_check_eq(node->name, "h3"); })
          || !xml_each_next(root->children, [](auto node) { return xml_check_eq(node->name, "p"); })
          || !xml_each_next(root->children, [](auto node) { return xml_check_eq(node->name, "strong"); })) {
        std::string error_info = "Wiktionary does not have any dictionary entry for this term.";
        auto redirect = [this,lang,references,f,error_info](const std::string& next_title) {
          if (std::count(references.begin(), references.end(), next_title)) {
            return f({ std::vector<char>(error_info.begin(), error_info.end()) });
          }
          auto new_refers = references;
          new_refers.push_back(next_title);
          dict_get_content(lang, next_title, new_refers, f);
        };
        bool see_also_found = xml_each_next(root->children, [lang,redirect,f](auto node) {
          std::string node_class = xml_get_prop(node, "class");
          if (node_class == "disambig-see-also") {
            xml_each_next(node->children, [lang,redirect,f](auto next) {
              if (xml_check_eq(next->name, "a")) {
                std::string next_title = xml_get_prop(next, "title");
                std::cout << "see also redirect title: " << next_title << std::endl;
                redirect(next_title);
                return 1;
              }
              return 0;
            });
            return 1;
          }
          return 0;
        });
        if (!see_also_found) {
          bool see_found = false;
          xml_each_child(xmlDocGetRootElement(doc)->children, [lang,redirect,f,&see_found](auto node) {
            std::cout << "see: " << node->name << std::endl;
            if (see_found) {
              if (xml_check_eq(node->name, "a")) {
                std::string node_title = xml_get_prop(node, "title");
                std::cout << "see redirect title: " << node_title << std::endl;
                redirect(node_title);
                return 1;
              }
            } else if (xml_check_eq(node->name, "text")) {
              std::cout << "  '" << xml_get_content(node) << "'" << std::endl;
              if (xml_get_content(node).find(" see") != std::string::npos) {
                see_found = true;
              }
            }
            return 0;
          });
          if (!see_found) {
            bool definition_found = xml_each_next(xmlDocGetRootElement(doc)->children, [lang,redirect,f](auto node) {
              std::string node_class = xml_get_prop(node, "class");
              if (node_class == "form-of-definition-link") {
                xml_each_child(node->children, [lang,redirect,f](auto next) {
                  if (xml_check_eq(next->name, "a")) {
                    std::string next_title = xml_get_prop(next, "title");
                    std::cout << "form of definition redirect title: " << next_title << std::endl;
                    redirect(next_title);
                    return 1;
                  }
                  return 0;
                });
                return 1;
              }
              return 0;
            });
            if (!definition_found) {
              bool dash_found = xml_each_next(xmlDocGetRootElement(doc)->children, [lang,redirect,f](auto node) {
                if (xml_get_content(node).find("–") != std::string::npos
                    || xml_get_content(node).find('-') != std::string::npos) {
                  for (auto next = node->next; next; next = next->next) {
                    if (xml_check_eq(next->name, "a")) {
                      std::string next_title = xml_get_prop(next, "title");
                      std::cout << "- redirect title: " << next_title << std::endl;
                      redirect(next_title);
                      return 1;
                    }
                  }
                }
                return 0;
              });
              if (!dash_found) {
                return f({ std::vector<char>(error_info.begin(), error_info.end()) });
              }
            }
          }
        }

        xmlFreeDoc(doc);
        xmlCleanupParser();
        return;
      }

      const std::vector<std::string> FUNCTIONS =
          { "Letter", "Syllable", "Numeral", "Number", "Punctuation mark", "Article", "Definitions", "Adnominal",
            "Noun", "Verb", "Participle", "Adjective", "Adverb", "Pronoun",
            "Proper noun", "Preposition", "Conjunction", "Interjection", "Determiner", "Initial", "Preverb", "Particle",
            "Root", "Affix", "Prefix", "Suffix", "Postposition", "Classifier", "Relative",
            "Combining form", "Adjectival noun",
            "Idiom", "Proverb", "Phrase", "Prepositional phrase", "Contraction", "Romanization", "Ligature", "Ideophone",
            "Symbol", "Cuneiform sign", "Han character" };

      // languages -> pronunciations -> etymologies -> functions -> defines -> sub-defines -> examples
      std::vector<std::pair<std::string, std::string>> ls;
      std::map<std::string, std::vector<std::pair<std::string, std::string>>> ps;
      std::map<std::string, std::vector<std::string>> es;
      std::map<std::string, std::vector<std::tuple<std::string, std::string, std::string>>> fs;
      std::map<std::string, std::vector<std::string>> ds;
      std::map<std::string, std::vector<std::string>> ss;
      std::map<std::string, std::vector<std::string>> xs;
      xml_each_next(xmlDocGetRootElement(doc)->children, [trim_title,FUNCTIONS,&ls,&ps,&es,&fs,&ds,&ss,&xs](auto node) {
        if (xml_check_eq(node->name, "h2")) {
          for (xmlNode* h2_child = node->children; h2_child; h2_child = h2_child->next) {
            std::string language = xml_get_prop(h2_child, "id");
            if (!language.empty()) {
              language = std::regex_replace(language, std::regex("_"), " ");
              std::cout << "h2, language: '" << language << "'" << std::endl;
              ls.emplace_back(language, "");
              break;
            }
          }
        }
        if (ls.empty()) { return 0; }

        std::pair<std::string, std::string>& language = ls.back();
        if (xml_check_eq(node->name, "h3")) {
          for (xmlNode* h3_child = node->children; h3_child; h3_child = h3_child->next) {
            std::string node_id = xml_get_prop(h3_child, "id");
            if (node_id == "Pronunciation") {
              xml_each_next(node, [&language, &ps](auto next) {
                if (xml_check_eq(next->name, "ul")) {
                  xml_each_child(next->children, [&language, &ps](auto child) {
                    std::cout << "Pronunciation child: " << child->name << std::endl;
                    if (xml_check_eq(child->name, "audio")) {
                      std::string duration = xml_get_prop(child, "data-durationhint");
                      for (xmlNode* audio_child = child->children; audio_child; audio_child = audio_child->next) {
                        if (xml_check_eq(audio_child->name, "source")) {
                          std::string type = xml_get_prop(audio_child, "type");
                          if (type != "audio/mpeg") {
                            continue;
                          }
                          std::string src = xml_get_prop(audio_child, "src");
                          std::cout << duration << ", " << src << std::endl;
                          ps[language.first].push_back({ duration, src });
                          break;
                        }
                      }
                      return 1;
                    }
                    return 0;
                  });
                  return 1;
                }
                return 0;
              });
            } else if (node_id.find("Etymology") != std::string::npos) {
              xml_each_next(node, [&language, &es](auto next) {
                if (xml_check_eq(next->name, "p")) {
                  std::string etymology = xml_get_content(next);
                  std::cout << "h3, etymology: '" << etymology << "'" << std::endl;
                  es[language.first].push_back(etymology);
                  return 1;
                }
                return 0;
              });
              break;
            }
          }
        }
        std::size_t etymology_i = es[language.first].empty() ? 0 : es[language.first].size() - 1;

        if (xml_check_eq(node->name, "h4") || xml_check_eq(node->name, "h3")) {
          for (xmlNode* h4_child = node->children; h4_child; h4_child = h4_child->next) {
            std::string function = xml_get_prop(h4_child, "id");
            function = std::regex_replace(function, std::regex("_\\d+"), "");
            function = std::regex_replace(function, std::regex("_"), " ");
            if (std::count(FUNCTIONS.begin(), FUNCTIONS.end(), function)) {
              std::cout << "h4, function: '" << function << "'" << std::endl;
              xml_each_next(node, [trim_title, &language, etymology_i, function, &fs](auto next) {
                if (xml_check_eq(next->name, "p") || xml_check_eq(next->name, "strong")) {
                  std::string lang;
                  if (xml_check_eq(next->name, "strong")) {
                    lang = xml_get_prop(next, "lang");
                  } else for (xmlNode* p_child = next->children; p_child; p_child = p_child->next) {
                    if (xml_check_eq(p_child->name, "strong")) {
                      lang = xml_get_prop(p_child, "lang");
                      break;
                    }
                  }
                  if (!lang.empty()) {
                    language.second = lang;
                  }
                  std::string word = xml_get_content(next);
                  if (word.empty()) {
                    word = trim_title;
                  }
                  std::cout << "  " << function << ", " << word << ", " << lang << std::endl;
                  std::string function_key = language.first + std::to_string(etymology_i);
                  bool function_found = false;
                  for (const auto& function_row : fs[function_key]) {
                    if (std::get<0>(function_row) == function && std::get<1>(function_row) == word && std::get<2>(function_row) == lang) {
                      function_found = true;
                    }
                  }
                  if (!function_found) {
                    fs[function_key].emplace_back(function, word, lang);
                  }
                  return 1;
                }
                return 0;
              });
              xml_each_next(node, [language,etymology_i,function,&ds,&ss,&xs](auto next) {
                if (xml_check_eq(next->name, "ol")) {
                  std::string define_key = std::string(language.first).append(std::to_string(etymology_i)).append(function);
                  for (xmlNode* ol_child = next->children; ol_child; ol_child = ol_child->next) {
                    if (xml_check_eq(ol_child->name, "li")) {
                      bool define_found = false;
                      for (xmlNode* ll_child = ol_child->children; ll_child; ll_child = ll_child->next) {
                        std::cout << "  define ll child name: " << ll_child->name << std::endl;
                        if (xml_check_eq(ll_child->name, "text")
                            || xml_check_eq(ll_child->name, "span")
                            || xml_check_eq(ll_child->name, "i")
                            || xml_check_eq(ll_child->name, "a")) {
                          if (!define_found) {
                            ds[define_key].push_back("");
                          }
                          define_found = true;
                          std::string define = xml_get_content(ll_child);
                          if (xml_check_eq(ll_child->name, "i")) {
                            define = "<i>" + define.append("</i>");
                          }
                          std::cout << "    define: '" << define << "'" << std::endl;
                          text_part_append(ds[define_key].back(), define);
                        }
                        std::cout << "  defines " << define_key << " size: " << ds[define_key].size() << std::endl;
                        if (ds[define_key].empty()) {
                          continue;
                        }
                        std::uint8_t define_i = ds[define_key].size() - 1;
                        if (xml_check_eq(ll_child->name, "ol")) {
                          for (xmlNode* ll_ol_child = ll_child->children; ll_ol_child; ll_ol_child = ll_ol_child->next) {
                            std::cout << "    sub-define ll ol child name: " << ll_ol_child->name << std::endl;
                            if (xml_check_eq(ll_ol_child->name, "li")) {
                              std::string sub_define;
                              for (xmlNode* sub_child = ll_ol_child->children; sub_child; sub_child = sub_child->next) {
                                if (xml_check_eq(sub_child->name, "ul")) {
                                  break;
                                }
                                text_part_append(sub_define, xml_get_content(sub_child));
                              }
                              std::cout << "      sub-define: '" << sub_define << "'" << std::endl;
                              ss[define_key + std::to_string(define_i)].push_back(sub_define);
                            }
                          }
                        }
                        if (xml_check_eq(ll_child->name, "dl")) {
                          for (xmlNode* dl_child = ll_child->children; dl_child; dl_child = dl_child->next) {
                            if (xml_check_eq(dl_child->name, "dd")) {
                              std::string example = xml_get_content(dl_child);
                              std::cout << "      example: '" << example << "'" << std::endl;
                              xs[define_key + std::to_string(define_i)].push_back(example);
                            }
                          }
                          break;
                        }
                      }
                    }
                  }
                  return 1;
                }
                return 0;
              });
              break;
            }
          }
        }
        return 0;
      });

      if (!ls.empty()) {
        auto print_functions = [ds,ss,xs,f](
            const std::string& lang, const std::string& language, const std::string& etymology,
            const std::string& key, const std::vector<std::tuple<std::string, std::string, std::string>>& functions) {
          if (functions.empty()) {
            return;
          }
          std::stringstream html;
          if (!language.empty() && std::get<2>(functions[0]) != lang) {
            html << language << '\n';
          }
          if (!etymology.empty() && etymology != std::get<1>(functions[0])) {
            html << etymology << '\n';
          }
          for (const auto& function : functions) {
            std::cout << "function: " << std::get<0>(function) << ", " << std::get<1>(function) << std::endl;
            if (html.rdbuf()->in_avail() != 0) {  // html.empty
              html << '\n';
            }
            html << std::get<0>(function) << ", " << std::get<1>(function) << "\n";
            std::string d_key = key + std::get<0>(function);
            if (ds.count(d_key) > 0) {
              for (int define_i = 0; define_i < ds.at(d_key).size(); define_i++) {
                html << (define_i + 1) << ". " << ds.at(d_key)[define_i] << "\n";
                std::string x_key = d_key + std::to_string(define_i);
                if (ss.count(x_key) > 0) {
                  for (int sub_define_i = 0; sub_define_i < ss.at(x_key).size(); sub_define_i++) {
                    html << "  " << (sub_define_i + 1) << ". " << ss.at(x_key)[sub_define_i] << "\n";
                  }
                }
                if (xs.count(x_key) > 0) {
                  for (const auto &example : xs.at(x_key)) {
                    html << "  <i>" << example << "</i>\n";
                  }
                }
              }
            }
          }
          if (html.rdbuf()->in_avail() != 0) {
            std::string desc = html.str();
            f({ std::vector<char>(desc.begin(), desc.end()) });
          }
        };

        bool lang_found = false;
        for (const auto& language : ls) {
          if (language.second == lang) {
            lang_found = true;
            break;
          }
        }

        for (const auto& language : ls) {
          std::cout << language.first << ", " << language.second << std::endl;
          if (lang_found && language.second != lang) {
            continue;
          }
          if (ps.count(language.first) > 0 && !ps[language.first].empty() && lang_found && lang == language.second) {
            for (const auto &pronunciation : ps[language.first]) {
              std::regex host_path("(upload.wikimedia.org)(.*)");
              std::smatch host_path_match;
              if (std::regex_search(pronunciation.second, host_path_match, host_path)) {
                std::string host = host_path_match[1];
                std::string path = host_path_match[2];
                std::cout << "get pronunciation: https://" << host << path << std::endl;
                send_https_request(host, path, [f, pronunciation,es,language](const std::vector<std::vector<char>>& res) {
                  std::string type = "Pronunciation";
                  std::vector<char> duration(pronunciation.first.begin(), pronunciation.first.end());
                  if (es.count(language.first) > 0 && !es.at(language.first).empty()) {
                    task_queue[std::time(nullptr) + es.at(language.first).size()].push_back([f,type,duration,res]() {
                      f({std::vector<char>(type.begin(), type.end()), duration, res[1]});
                    });
                  } else {
                    f({std::vector<char>(type.begin(), type.end()), duration, res[1]});
                  }
                });
              }
            }
          }
          if (!es[language.first].empty()) {
            for (int etymology_i = 0; etymology_i < es[language.first].size(); etymology_i++) {
              std::string etymology = es[language.first][etymology_i];
              std::string key = language.first + std::to_string(etymology_i);
              auto functions = fs[key];
              if (etymology_i > 0) {
                task_queue[std::time(nullptr) + etymology_i].push_back([lang,print_functions,language,etymology,key,functions]() {
                  print_functions(lang, language.first, etymology, key, functions);
                });
              } else {
                print_functions(lang, language.first, etymology, key, functions);
              }
            }
          } else {
            std::string key = language.first + "0";
            auto functions = fs[key];
            print_functions(lang, language.first, "", key, functions);
          }
        }
      }

      xmlFreeDoc(doc);
      xmlCleanupParser();
    }
  });
}

void tdscript::save() {
  if (!data_ready) { return; }
  if (!save_flag) { return; }
  save_flag = false;

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();

  writer.String("player_count");
  writer.StartObject();
  for (const auto kv : player_count) {
    writer.String(std::to_string(kv.first).c_str());
    writer.Int64(kv.second);
  }
  writer.EndObject();

  writer.String("has_owner");
  writer.StartObject();
  for (const auto kv : has_owner) {
    writer.String(std::to_string(kv.first).c_str());
    writer.Bool(kv.second);
  }
  writer.EndObject();

  writer.String("pending_extend_messages");
  writer.StartObject();
  for (const auto& kv : pending_extend_messages) {
    writer.String(std::to_string(kv.first).c_str());
    writer.StartArray();
    for (const auto& item : kv.second) {
      writer.Int64(item);
    }
    writer.EndArray();
  }
  writer.EndObject();

  writer.String("last_extent_at");
  writer.StartObject();
  for (const auto& kv : last_extent_at) {
    writer.String(std::to_string(kv.first).c_str());
    writer.Uint64(kv.second);
  }
  writer.EndObject();

  writer.String("players_message");
  writer.StartObject();
  for (const auto& kv : players_message) {
    writer.String(std::to_string(kv.first).c_str());
    writer.Int64(kv.second);
  }
  writer.EndObject();

  writer.String("need_extend");
  writer.StartObject();
  for (const auto& kv : need_extend) {
    writer.String(std::to_string(kv.first).c_str());
    writer.Bool(kv.second);
  }
  writer.EndObject();

  writer.String("started");
  writer.StartObject();
  for (const auto& kv : started) {
    writer.String(std::to_string(kv.first).c_str());
    writer.Bool(kv.second);
  }
  writer.EndObject();

  writer.String("last_done_at");
  writer.StartObject();
  for (const auto& kv : last_done_at) {
    writer.String(std::to_string(kv.first).c_str());
    writer.Uint64(kv.second);
  }
  writer.EndObject();

  writer.EndObject();

  std::ofstream ofs;
  ofs.open(SAVE_FILENAME, std::ofstream::out | std::ofstream::binary);

  ofs << buffer.GetString() << '\n';

  ofs.close();
}

void tdscript::load() {
  std::ifstream file(SAVE_FILENAME, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    data_ready = true;
    return ;
  }

  if (file.is_open()) {
    auto size = file.tellg();
    std::string content(size, '\0');
    file.seekg(0);
    if(file.read(&content[0], size)) {
      rapidjson::Document data;
      data.Parse(content.c_str());
      for (rapidjson::Value::ConstMemberIterator i = data.MemberBegin(); i != data.MemberEnd(); ++i) {
        std::string domain = i->name.GetString();
        for (rapidjson::Value::ConstMemberIterator j = i->value.MemberBegin(); j != i->value.MemberEnd(); ++j) {
          std::int64_t key = std::stoll(j->name.GetString());
          if (domain == "player_count") {
            player_count[key] = j->value.GetInt();
          } else if (domain == "has_owner") {
            has_owner[key] = j->value.GetBool();
          } else if (domain == "pending_extend_messages") {
            for (rapidjson::SizeType k = 0; k < j->value.Size(); ++k) {
              pending_extend_messages[key].push_back(j->value[k].GetInt64());
            }
          } else if (domain == "last_extent_at") {
            last_extent_at[key] = j->value.GetUint64();
          } else if (domain == "players_message") {
            players_message[key] = j->value.GetInt64();
          } else if (domain == "need_extend") {
            need_extend[key] = j->value.GetBool();
          } else if (domain == "started") {
            started[key] = j->value.GetBool();
          } else if (domain == "last_done_at") {
            last_done_at[key] = j->value.GetUint64();
          }
        }
      }
    }
  }

  data_ready = true;
}
