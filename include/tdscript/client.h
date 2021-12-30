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
#include <unordered_map>
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

  extern std::time_t last_task_at;
  extern std::unordered_map<std::time_t, std::vector<std::function<void()>>> task_queue;

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
  inline void xml_each_next(const xmlNode* node, const std::function<bool(const xmlNode* node)>& f) {
    std::queue<const xmlNode*> nodes;
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
  inline void xml_each_child(const xmlNode* node, const std::function<bool(const xmlNode* node)>& f) {
    std::deque<const xmlNode*> nodes;
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

      dns_client = libdns::Client(log_verbosity_level ? 1 : 0);
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
    inline void send_html(std::int64_t chat_id, std::int64_t reply_id, std::string text, bool no_link_preview, bool no_html) {
      auto send_message = td::td_api::make_object<td::td_api::sendMessage>();
      send_message->chat_id_ = chat_id;
      send_message->reply_to_message_id_ = reply_id;
      auto message_content = td::td_api::make_object<td::td_api::inputMessageText>();
      message_content->text_ = td::td_api::make_object<td::td_api::formattedText>();
      if (!no_html) {
        std::vector<td::td_api::object_ptr<td::td_api::textEntity>> entities;
        std::size_t offset1, offset2 = 0;
        do {
          offset1 = text.find("<i>", offset2);
          if (offset1 == std::string::npos) { break; }
          text.erase(offset1, 3);
          auto entity = td::td_api::make_object<td::td_api::textEntity>();
          entity->offset_ = ustring_count(text, 0, offset1);
          offset2 = text.find("</i>", offset1);
          if (offset2 == std::string::npos) { break; }
          text.erase(offset2, 4);
          entity->length_ = ustring_count(text, offset1, offset2);
          entity->type_ = td::td_api::make_object<td::td_api::textEntityTypeItalic>();
          entities.push_back(std::move(entity));
        } while (true);
        message_content->text_->entities_ = std::move(entities);
      }
      message_content->text_->text_ = std::move(text);
      message_content->disable_web_page_preview_ = no_link_preview;
      send_message->input_message_content_ = std::move(message_content);
      send_request(std::move(send_message));
    }
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

    inline void wiki_get_content(const std::string& lang, const std::string& title, const std::function<void(std::string)>& f) {
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

          std::vector<std::string> desc_find_kws = { "。", " is ", " was ", "." };
          std::string article_desc;
          xml_each_next(xmlDocGetRootElement(doc)->children, [this, lang, f, desc_find_kws, &article_desc](auto node) {
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

    inline void dict_get_random_title(const std::string& lang, const std::function<void(std::string)>& f) {
      wiki_get_random_title(lang, ".wiktionary.org", f);
    }

    inline void dict_get_content(const std::string& lang, const std::string& title, const callback_t& f) {
      wiki_get_data(lang, title, ".wiktionary.org", [this, f, lang, title](const std::string& res_body) {
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

          const std::vector<std::string> FUNCTIONS =
              { "Article", "Noun", "Verb", "Participle", "Adjective", "Adverb", "Pronoun",
                "Preposition", "Conjunction", "Interjection", "Determiner", "Prefix", "Suffix" };

          // languages -> pronunciations -> etymologies -> functions -> defines -> sub-defines -> examples
          std::vector<std::string> ls;
          std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> ps;
          std::unordered_map<std::string, std::vector<std::string>> es;
          std::unordered_map<std::string, std::vector<std::tuple<std::string, std::string, std::string>>> fs;
          std::unordered_map<std::string, std::vector<std::string>> ds;
          std::unordered_map<std::string, std::vector<std::string>> ss;
          std::unordered_map<std::string, std::vector<std::string>> xs;
          xml_each_next(xmlDocGetRootElement(doc)->children, [FUNCTIONS,&ls,&ps,&es,&fs,&ds,&ss,&xs](auto node) {
            if (xml_check_eq(node->name, "h2")) {
              for (xmlNode* h2_child = node->children; h2_child; h2_child = h2_child->next) {
                std::string language = xml_get_prop(h2_child, "id");
                if (!language.empty()) {
                  std::cout << "h2, language: '" << language << "'" << std::endl;
                  ls.push_back(language);
                  break;
                }
              }
            }
            if (ls.empty()) { return 0; }

            std::string language = ls.back();
            if (xml_check_eq(node->name, "h3")) {
              for (xmlNode* h3_child = node->children; h3_child; h3_child = h3_child->next) {
                std::string node_id = xml_get_prop(h3_child, "id");
                if (node_id == "Pronunciation") {
                  xml_each_next(node, [language, &ps](auto next) {
                    if (xml_check_eq(next->name, "ul")) {
                      xml_each_child(next, [language, &ps](auto child) {
                        std::cout << "Pronunciation child: " << child->name << std::endl;
                        if (xml_check_eq(child->name, "audio")) {
                          std::string duration = xml_get_prop(child, "data-durationhint");
                          for (xmlNode* audio_child = child->children; audio_child; audio_child = audio_child->next) {
                            if (xml_check_eq(audio_child->name, "source")) {
                              std::string src = xml_get_prop(audio_child, "src");
                              std::cout << duration << ", " << src << std::endl;
                              ps[language].push_back({ duration, src });
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
                  xml_each_next(node, [language, &es](auto next) {
                    if (xml_check_eq(next->name, "p")) {
                      std::string etymology = xml_get_content(next);
                      std::cout << "h3, etymology: '" << etymology << "'" << std::endl;
                      es[language].push_back(etymology);
                      return 1;
                    }
                    return 0;
                  });
                  break;
                }
              }
            }
            std::size_t etymology_i = es[language].empty() ? 0 : es[language].size() - 1;

            if (xml_check_eq(node->name, "h4") || xml_check_eq(node->name, "h3")) {
              for (xmlNode* h4_child = node->children; h4_child; h4_child = h4_child->next) {
                std::string function = xml_get_prop(h4_child, "id");
                function = std::regex_replace(function, std::regex("_\\d+"), "");
                if (std::count(FUNCTIONS.begin(), FUNCTIONS.end(), function)) {
                  std::cout << "h4, function: '" << function << "'" << std::endl;
                  xml_each_next(node, [language, etymology_i, function, &fs](auto next) {
                    if (xml_check_eq(next->name, "p")) {
                      std::string lang;
                      for (xmlNode* p_child = next->children; p_child; p_child = p_child->next) {
                        if (xml_check_eq(p_child->name, "strong")) {
                          lang = xml_get_prop(p_child, "lang");
                          break;
                        }
                      }
                      std::string word = xml_get_content(next);
                      std::cout << "  " << function << ", " << word << ", " << lang << std::endl;
                      fs[language + std::to_string(etymology_i)].push_back({ function, word, lang });
                      return 1;
                    }
                    return 0;
                  });
                  xml_each_next(node, [language,etymology_i,function,&ds,&ss,&xs](auto next) {
                    if (xml_check_eq(next->name, "ol")) {
                      std::string define_key = std::string(language).append(std::to_string(etymology_i)).append(function);
                      for (xmlNode* ol_child = next->children; ol_child; ol_child = ol_child->next) {
                        if (xml_check_eq(ol_child->name, "li")) {
                          bool define_found = false;
                          for (xmlNode* ll_child = ol_child->children; ll_child; ll_child = ll_child->next) {
                            std::cout << "  define ll child name: " << ll_child->name << std::endl;
                            if (xml_check_eq(ll_child->name, "text")
                                    || xml_check_eq(ll_child->name, "span")
                                    || xml_check_eq(ll_child->name, "a")) {
                              if (!define_found) {
                                ds[define_key].push_back("");
                              }
                              define_found = true;
                              std::string define = xml_get_content(ll_child);
                              std::cout << "    define: '" << define << "'" << std::endl;
                              if (ds[define_key].back().empty()
                                  || ds[define_key].back()[ds[define_key].back().length() - 1] == '('
                                  || define[0] == ')' || define[0] == ',' || define[0] == '.' || define[0] == ';') {
                                ds[define_key].back().append(define);
                              } else {
                                ds[define_key].back().append(" ").append(define);
                              }
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
                                  std::string sub_define = xml_get_content(ll_ol_child);
                                  std::size_t stop_pos = sub_define.find('.');
                                  sub_define.erase(stop_pos, sub_define.size() - 1 - stop_pos);
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
            auto print_functions =
                [](const std::string& lang, bool lang_found,
                    const std::unordered_map<std::string, std::vector<std::string>>& ds,
                    const std::unordered_map<std::string, std::vector<std::string>>& ss,
                    const std::unordered_map<std::string, std::vector<std::string>>& xs,
                    const callback_t& f, const std::string& language, const std::string& etymology, const std::string& key,
                    const std::vector<std::tuple<std::string, std::string, std::string>>& functions) {
              if (functions.empty()) {
                return;
              }
              std::stringstream html;
              if (!language.empty() && std::get<2>(functions[0]) != lang) {
                if (lang_found) {
                  return;
                }
                html << language << '\n';
              }
              if (!etymology.empty() && etymology != std::get<1>(functions[0])) {
                html << etymology << '\n';
              }
              for (const auto& function : functions) {
                if (html.rdbuf()->in_avail() != 0) {
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
            for (const auto& fkv : fs) {
              for (const auto& function : fkv.second) {
                if (std::get<2>(function) == lang) {
                  lang_found = true;
                  break;
                }
              }
              if (lang_found) {
                break;
              }
            }

            for (const auto& language : ls) {
              if (!ps[language].empty()) {
                for (const auto& pronunciation : ps[language]) {
                  std::regex host_path("(upload.wikimedia.org)(.*)");
                  std::smatch host_path_match;
                  if (std::regex_search(pronunciation.second, host_path_match, host_path)) {
                    std::string host = host_path_match[1];
                    std::string path = host_path_match[2];
                    std::cout << "get pronunciation: https://" << host << path << std::endl;
                    send_https_request(host, path, [f, pronunciation](std::vector<std::vector<char>> res) {
                      std::string type = "Pronunciation";
                      std::string duration = pronunciation.first;
                      f({ std::vector<char>(type.begin(), type.end()),
                          std::vector<char>(duration.begin(), duration.end()), res[1] });
                    });
                  }
                }
              }
              if (!es[language].empty()) {
                for (int etymology_i = 0; etymology_i < es[language].size(); etymology_i++) {
                  std::string etymology = es[language][etymology_i];
                  std::string key = language + std::to_string(etymology_i);
                  auto functions = fs[key];
                  if (etymology_i > 0) {
                    task_queue[std::time(nullptr)
                        + etymology_i].push_back([lang,lang_found,ds,ss,xs,f,print_functions,language,etymology,key,functions]() {
                      print_functions(lang, lang_found, ds, ss, xs, f, language, etymology, key, functions);
                    });
                  } else {
                    print_functions(lang, lang_found, ds, ss, xs, f, language, etymology, key, functions);
                  }
                }
              } else {
                std::string key = language + "0";
                auto functions = fs[key];
                print_functions(lang, lang_found, ds, ss, xs, f, language, "", key, functions);
              }
            }
          }

          xmlFreeDoc(doc);
          xmlCleanupParser();
        }
      });
    };

    inline void process_wiki(std::int64_t chat_id, const std::string &text, const std::string& kw, const std::string& domain) {
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

    inline void process_wiki(std::int64_t chat_id, const std::string &text) {
      process_wiki(chat_id, text, "wiki", "wikipedia");
    }

    inline void process_wiki(std::int64_t chat_id, const std::string &lang, const std::string &title) {
      wiki_get_content(lang, title, [this, chat_id](auto desc) { send_text(chat_id, desc, true); });
    }

    inline void process_dict(std::int64_t chat_id, const std::string &text) {
      process_wiki(chat_id, text, "dict", "wiktionary");
    }

    inline void process_dict(std::int64_t chat_id, const std::string &lang, const std::string &title) {
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
  };  // class Client
}  // namespace tdscript

#endif  // INCLUDE_TDSCRIPT_CLIENT_H_
