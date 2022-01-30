#include <iostream>
#include <string>
#include <regex>

int main() {
  std::vector<std::string> texts = { R"(CA 主音吉他最帅了 但是上班害人: 💀死亡 - 共济会会员👷 胜利
lccc: 💀死亡 - 🔥纵火犯 失败
RSM504: 🙂幸存 - 👻小偷😈 失败
)",
    R"(新空 E: 🙂幸存 - 铁匠 ⚒ 胜利
CA 主音吉他最帅了 但是上班害人: 🙂幸存 - 💤睡神 胜利
lccc: 🙂幸存 - 📚长老 胜利
)",
    R"(幸存者们: 1 / 5
old toy: 💀死亡 - 侦探🕵  失败
lccc: 💀死亡 - 旁观者👁  失败
飞 sir: 💀死亡 - 先知👳 失败
ShinkuE: 💀死亡 - 🐺🌝狼人(潜隐者) 失败
lkwatry Lee: 🙂幸存 - 变态杀人狂🔪 胜利

游戏进行了：00:02:21)" };

  for (auto text : texts) {
    std::smatch done_match;
    if (std::regex_search(text, done_match, std::regex("lccc:.* ([^ ]*)\n"))) {
      bool fail = done_match[1] == "失败";
      std::cout << (fail ? "FAIL" : "SUCCESS") << std::endl;
    }
  }
}