#include <iostream>
#include <string>
#include <regex>

int main() {
  std::vector<std::string> texts = { R"(CA 主音吉他最帅了 但是上班害人: 💀死亡 - 共济会会员👷 胜利
lccc: 💀死亡 - 🔥纵火犯 失败
RSM504: 🙂幸存 - 👻小偷😈 失败)",
    R"(新空 E: 🙂幸存 - 铁匠 ⚒ 胜利
CA 主音吉他最帅了 但是上班害人: 🙂幸存 - 💤睡神 胜利
lccc: 🙂幸存 - 📚长老 胜利)",
    "CA 主音吉他最帅了 但是上班害人: 💀死亡 - 共济会会员👷 胜利\r\nlccc: 💀死亡 - 🔥纵火犯 失败\r\nRSM504: 🙂幸存 - 👻小偷😈 失败" };

  for (auto text : texts) {
    std::smatch done_match;
    if (std::regex_search(text.append("\n"), done_match, std::regex("lccc:.* ([^ ]*)\r?\n"))) {
      if (done_match[1] == "胜利") {
        std::cout << "Yes!" << std::endl;
      }
      std::cout << "'" << done_match[1] << "'" << std::endl;
    }
  }
}