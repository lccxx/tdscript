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
  const std::int64_t USER_ID_WEREWOLF = 175844556;
  const std::string START_TEXT = "/startchaos@werewolfbot";
  const std::int32_t EXTEND_TIME = 123;
  const std::string EXTEND_TEXT = std::string("/extend@werewolfbot ") + std::to_string(EXTEND_TIME);
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
  const std::unordered_map<std::int64_t, std::int64_t> STICKS_REPLY_TO = { {2753361346560, 981032009 } };  // msg_id, user_id
  const std::unordered_map<std::int64_t, std::string> KEY_PLAYER_IDS = { { 981032009, "@TalkIce" } };
  const std::unordered_map<std::string, std::string> KEY_PLAYERS = { { "KMM", "@JulienKM" } };

  bool stop = false;

  const std::string SAVE_FILENAME = std::string(std::getenv("HOME")) + std::string("/.tdscript-save.json");
  bool save_flag = false;
  bool data_ready = false;
  std::unordered_map<std::int64_t, std::int32_t> player_count;
  std::unordered_map<std::int64_t, std::uint8_t> has_owner;
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> pending_extend_messages;
  std::unordered_map<std::int64_t, std::uint64_t> last_extent_at;
  std::unordered_map<std::int64_t, std::int64_t> players_message;
  std::unordered_map<std::int64_t, std::uint8_t> need_extend;
  std::unordered_map<std::int64_t, std::uint8_t> started;
  std::unordered_map<std::int64_t, std::uint64_t> last_done_at;

  std::mt19937 rand_engine = std::mt19937((std::random_device())());

  std::unordered_map<std::int64_t, std::uint64_t> last_start_at;
  std::unordered_map<std::int64_t, std::vector<std::string>> at_list;  // the '@' list
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> player_ids;
  bool werewolf_bot_warning = false;

  std::time_t last_task_at = -1;
  std::unordered_map<std::time_t, std::vector<std::function<void()>>> task_queue;
}

void tdscript::Client::send_start(std::int64_t chat_id) {
  if (werewolf_bot_warning) { return; }
  if (need_extend.at(chat_id)) { return; }
  if (started.at(chat_id)) { return; }
  if (last_done_at.count(chat_id) > 0 && std::time(nullptr) - last_done_at.at(chat_id) < 99) { return; }
  if (last_start_at.count(chat_id) > 0 && std::time(nullptr) - last_start_at.at(chat_id) < 9) { return; }
  last_start_at[chat_id] = std::time(nullptr);

  send_text(chat_id, START_TEXT);
}

void tdscript::Client::send_join(std::int64_t chat_id, std::int64_t bot_id, const std::string& link, int limit) {
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

  // add to the task queue, resend after 9 seconds, limit 9(default)
  task_queue[std::time(nullptr) + 9].push_back([this, chat_id, bot_id, link, limit]() {
    send_join(chat_id, bot_id, link, limit - 1);
  });
}

void tdscript::Client::send_extend(std::int64_t chat_id) {
  if (werewolf_bot_warning) { return; }
  if (!need_extend.at(chat_id)) { return; }
  if (!has_owner.at(chat_id)) { return; }
  if (player_count.at(chat_id) >= 5) { return; }
  if (pending_extend_messages.count(chat_id) != 0 && pending_extend_messages[chat_id].size() > 9) { return; }
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
  std::cout << "msg(" << msg_id << "): " << user_id << " -> " << chat_id << ", " << text << "\n" << std::endl;

  if (user_id == USER_ID_WEREWOLF || user_id == 0) {
    process_werewolf(chat_id, msg_id, user_id, text, link);
  }

  process_wiki(chat_id, text);
  process_dict(chat_id, text);

  if (text == EXTEND_TEXT) {
    pending_extend_messages[chat_id].push_back(msg_id);
  }

  const std::regex starting_regex("游戏启动中");
  if (std::regex_search(text, starting_regex)) {
    for (const auto& at : at_list[chat_id]) { send_text(chat_id, at); }
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
    const std::regex owner_regex("lccc");
    if (std::regex_search(text, owner_regex)) {
      has_owner[chat_id] = 1;
    }
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
  }
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
