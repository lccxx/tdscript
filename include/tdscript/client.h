// created by lccc 12/04/2021, no copyright

#ifndef INCLUDE_TDSCRIPT_CLIENT_H_
#define INCLUDE_TDSCRIPT_CLIENT_H_

#include "tdscript/config.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
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
    int i = 0; do { if (a[i] != b[i]) { return false; } i++; } while (a[i] && b[i]);return true;
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
      send_https_request(host, path, [host, f](const std::vector<std::string> &res) {
        rapidjson::Document data;
        data.Parse(res[1].c_str());
        if (data.HasMember("query")) {
          f(data["query"]["random"][0]["title"].GetString());
        }
      });
    }

    inline void dict_get_random_title(const std::string& lang, const std::function<void(std::string)>& f) {
      wiki_get_random_title(lang, ".wiktionary.org", f);
    }

    inline void wiki_get_random_title(const std::string& lang, const std::function<void(std::string)>& f) {
      wiki_get_random_title(lang, ".wikipedia.org", f);
    }

    inline void wiki_get_data(const std::string& lang, const std::string& title, const std::string& wd, const std::function<void(std::string)>& f) {
      std::string host = lang + wd;
      std::string path = "/w/api.php?action=parse&format=json&page=" + libdns::urlencode(title);
      send_https_request(host, path, [f](const std::vector<std::string>& res){ f(res[1]); });
    }

    inline void dict_get_content(const std::string& lang, const std::string& title, const std::function<void(std::string)>& f) {
      wiki_get_data(lang, title, ".wiktionary.org", [f, lang, title](const std::string& res_body) {
        rapidjson::Document data; data.Parse(res_body.c_str());
        if (data.HasMember("error")) {
          f(data["error"]["info"].GetString());
        } else if (data.HasMember("parse")) {
          std::string text = data["parse"]["text"]["*"].GetString();

          xmlInitParser();
          xmlDocPtr doc = xmlParseMemory(text.c_str(), (int)text.length());
          if (doc == nullptr) {
            fprintf(stderr, "Error: unable to parse string: \"%s\"\n", text.c_str());
            return;
          }
          xmlDocDump(stdout, doc);

          const std::vector<std::string> FUNCTIONS =
              { "Article", "Noun", "Verb", "Participle", "Adjective", "Adverb", "Pronoun",
                "Preposition", "Conjunction", "Interjection", "Determiner" };

          // languages -> etymologies -> functions -> defines -> examples
          std::vector<std::string> ls;
          std::unordered_map<std::string, std::vector<std::string>> es;
          std::unordered_map<std::string, std::vector<std::tuple<std::string, std::string, std::string>>> fs;
          std::unordered_map<std::string, std::vector<std::string>> ds;
          std::unordered_map<std::string, std::vector<std::string>> xs;
          xml_each_next(xmlDocGetRootElement(doc)->children, [FUNCTIONS,&ls,&es,&fs,&ds,&xs](auto node) {
            if (xml_check_eq(node->name, "h2")) {
              for (xmlNode* h2_child = node->children; h2_child; h2_child = h2_child->next) {
                std::string language = xml_get_prop(h2_child, "id");
                if (!language.empty()) {
                  std::cout << "h2, language: '" << language << "'\n";
                  ls.push_back(language);
                  break;
                }
              }
            }
            if (ls.empty()) { return 0; }

            std::string language = ls.back();
            if (xml_check_eq(node->name, "h3")) {
              for (xmlNode* h3_child = node->children; h3_child; h3_child = h3_child->next) {
                std::string etymology = xml_get_prop(h3_child, "id");
                if (etymology.find("Etymology") != std::string::npos) {
                  std::cout << "h3, etymology: '" << etymology << "'\n";
                  es[language].push_back(etymology);
                  break;
                }
              }
            }
            std::string etymology = es[language].empty()? "" : es[language].back();

            if (xml_check_eq(node->name, "h4") || xml_check_eq(node->name, "h3")) {
              for (xmlNode* h4_child = node->children; h4_child; h4_child = h4_child->next) {
                std::string function = xml_get_prop(h4_child, "id");
                function = std::regex_replace(function, std::regex("_\\d+"), "");
                if (std::count(FUNCTIONS.begin(), FUNCTIONS.end(), function)) {
                  std::cout << "h4, function: '" << function << "'\n";
                  xml_each_next(node, [language, etymology, function, &fs](auto next) {
                    if (xml_check_eq(next->name, "p")) {
                      std::string lang;
                      for (xmlNode* p_child = next->children; p_child; p_child = p_child->next) {
                        if (xml_check_eq(p_child->name, "strong")) {
                          lang = xml_get_prop(p_child, "lang");
                          break;
                        }
                      }
                      std::string word = xml_get_content(next);
                      std::cout << "  " << function << ", " << word << ", " << lang << "\n";
                      fs[language + etymology].push_back({ function, word, lang });
                      return 1;
                    }
                    return 0;
                  });
                  xml_each_next(node, [language,etymology,function,&ds,&xs](auto next) {
                    if (xml_check_eq(next->name, "ol")) {
                      std::string define_key = std::string(language).append(etymology).append(function);
                      for (xmlNode* ol_child = next->children; ol_child; ol_child = ol_child->next) {
                        if (xml_check_eq(ol_child->name, "li")) {
                          bool define_found = false;
                          for (xmlNode* ll_child = ol_child->children; ll_child; ll_child = ll_child->next) {
                            if (!define_found && (xml_check_eq(ll_child->name, "span") || xml_check_eq(ll_child->name, "a"))) {
                              std::cout << "    '" << xml_get_content(ll_child) << "'\n";
                              ds[define_key].push_back(xml_get_content(ll_child));
                              define_found = true;
                            }
                            if (define_found && xml_check_eq(ll_child->name, "dl")) {
                              for (xmlNode* dl_child = ll_child->children; dl_child; dl_child = dl_child->next) {
                                if (xml_check_eq(dl_child->name, "dd")) {
                                  std::cout << "      '" << xml_get_content(dl_child) << "'\n";
                                  std::uint8_t define_i = ds[define_key].size() - 1;
                                  xs[define_key + std::to_string(define_i)].push_back(xml_get_content(dl_child));
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
            auto print_functions = [lang,f,&ds,&xs](
                const std::string& language, const std::string& key,
                const std::vector<std::tuple<std::string, std::string, std::string>>& functions) {
              std::stringstream ss;
              for (const auto& function : functions) {
                if (!language.empty() && std::get<2>(function) != lang) {
                  ss << language << '\n';
                }
                ss << std::get<0>(function) << "\n" << std::get<1>(function) << "\n";
                std::string d_key = key + std::get<0>(function);
                for (int define_i = 0; define_i < ds[d_key].size(); define_i++) {
                  ss << "  " << (define_i + 1) << ". " << ds[d_key][define_i] << "\n";
                  std::string x_key = d_key + std::to_string(define_i);
                  for (const auto & example : xs[x_key]) {
                    ss << "    " << example << "\n";
                  }
                }
              }
              f(ss.str());
            };

            bool lang_found = false;
            for (const auto& fkv : fs) {
              for (const auto& function : fkv.second) {
                if (std::get<2>(function) == lang) {
                  print_functions("", fkv.first, fs[fkv.first]);
                  lang_found = true;
                }
              }
            }
            if (!lang_found) {
              for (const auto &language : ls) {
                for (const auto &etymology : es[language]) {
                  std::string key = language + etymology;
                  print_functions(language, key, fs[key]);
                }
                if (es[language].empty()) {
                  print_functions(language, language, fs[language]);
                }
              }
            }
          }

          xmlFreeDoc(doc);
          xmlCleanupParser();
        }
      });
    };

    inline void wiki_get_content(const std::string& lang, const std::string& title, const std::function<void(std::string)>& f) {
      wiki_get_data(lang, title, ".wikipedia.org", [this, f, lang, title](const std::string& res_body) {
        rapidjson::Document data; data.Parse(res_body.c_str());
        if (data.HasMember("error")) {
          f(data["error"]["info"].GetString());
        } else if (data.HasMember("parse")) {
          std::string text = data["parse"]["text"]["*"].GetString();

          xmlInitParser();
          xmlDocPtr doc = xmlParseMemory(text.c_str(), (int)text.length());
          if (doc == nullptr) {
            fprintf(stderr, "Error: unable to parse string: \"%s\"\n", text.c_str());
            return;
          }
          xmlDocDump(stdout, doc);

          std::vector<std::string> desc_find_kws = { "。", " is ", " was ", "." };
          std::string article_desc;
          xml_each_next(xmlDocGetRootElement(doc)->children, [this, lang, f, desc_find_kws, &article_desc](auto node) {
            std::string node_class = xml_get_prop(node, "class");
            std::cout << "node: '" << node->name << (node_class.empty() ? "" : "." + node_class) << "'\n";
            if (xml_check_eq(node->name, "div") && node_class == "redirectMsg") {
              xml_each_child(node->children, [this, lang, f](auto child) {
                if (xml_check_eq(child->name, "a")) {
                  std::string redirect_title = xml_get_prop(child, "title");
                  std::cout << "redirect title: " << redirect_title << '\n';
                  wiki_get_content(lang, redirect_title, f);
                  return 1;
                }
                return 0;
              });
              return 1;
            }
            if (xml_check_eq(node->name, "p")) {
              std::string content = xml_get_content(node); trim(content);
              std::cout << "content: '" << content << "'\n";
              if (content[content.length() - 1] == ':') {  // content is "xxx refer to:"
                xml_each_next(node, [&content, &article_desc](auto next_node) {
                  std::cout << "next node name: '" << next_node->name << "'\n";
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
            article_desc = std::regex_replace(article_desc, std::regex(R"(\[[^]]*\])"), "");
            std::cout << "'" << title << "': '" << article_desc << "'\n";

            f(article_desc);
          }

          xmlFreeDoc(doc);
          xmlCleanupParser();
        }
      });
    }

    inline void process_wiki(std::int64_t chat_id, const std::string &text, std::string kw, std::string domain) {
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
        wiki_get_random_title(lang, [this, chat_id, lang](auto title) {
          process_wiki(chat_id, lang, title);
        });
      } else {
        process_wiki(chat_id, lang, title);
      }
    }

    inline void process_dict(std::int64_t chat_id, const std::string &text) {
      process_wiki(chat_id, text, "dict", "wiktionary");
    }

    inline void process_wiki(std::int64_t chat_id, const std::string &text) {
      process_wiki(chat_id, text, "wiki", "wikipedia");
    }

    inline void process_wiki(std::int64_t chat_id, const std::string &lang, const std::string &title) {
      wiki_get_content(lang, title, [this, chat_id](auto desc) { send_text(chat_id, desc, true); });
    }
  };  // class Client

  template<typename T>
  inline void select_one_randomly(const std::vector<T>& arr, const std::function<void(std::size_t)>& f) {
    if (!arr.empty()) { f((std::uniform_int_distribution<int>(0, arr.size() - 1))(rand_engine)); }
  }
}  // namespace tdscript

#endif  // INCLUDE_TDSCRIPT_CLIENT_H_
