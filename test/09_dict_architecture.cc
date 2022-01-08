// created by lccc 12/28/2020, no copyright

#include "tdscript/client.h"

int main() {
  using namespace tdscript;

  std::string xml = "<ul><li><span class=\"ib-brac qualifier-brac\">(</span><span class=\"ib-content qualifier-content\"><a href=\"https://en.wikipedia.org/wiki/Received_Pronunciation\" class=\"extiw\" title=\"w:Received Pronunciation\">Received Pronunciation</a></span><span class=\"ib-brac qualifier-brac\">)</span> <a href=\"/wiki/Wiktionary:International_Phonetic_Alphabet\" title=\"Wiktionary:International Phonetic Alphabet\">IPA</a><sup>(<a href=\"/wiki/Appendix:English_pronunciation\" title=\"Appendix:English pronunciation\">key</a>)</sup>: <span class=\"IPA\">/&#x2C8;&#x251;&#x2D0;.k&#x26A;.&#x2CC;t&#x25B;k.t&#x283;&#x259;/</span></li>\n"
                    "<li><span class=\"ib-brac qualifier-brac\">(</span><span class=\"ib-content qualifier-content\"><a href=\"https://en.wikipedia.org/wiki/General_American\" class=\"extiw\" title=\"w:General American\">General American</a></span><span class=\"ib-brac qualifier-brac\">)</span> <a href=\"/wiki/Wiktionary:International_Phonetic_Alphabet\" title=\"Wiktionary:International Phonetic Alphabet\">IPA</a><sup>(<a href=\"/wiki/Appendix:English_pronunciation\" title=\"Appendix:English pronunciation\">key</a>)</sup>: <span class=\"IPA\">/&#x2C8;&#x251;&#x279;k&#x26A;t&#x25B;kt&#x283;&#x25A;/</span></li>\n"
                    "<li><style data-mw-deduplicate=\"TemplateStyles:r50165410\">.mw-parser-output .k-player .k-attribution{visibility:hidden}</style><table class=\"audiotable\" style=\"vertical-align: bottom; display:inline-block; list-style:none;line-height: 1em; border-collapse:collapse;\"><tbody><tr><td class=\"unicode audiolink\" style=\"padding-right:5px; padding-left: 0;\">Audio (US)</td><td class=\"audiofile\"><div class=\"mediaContainer\" style=\"width:175px\"><audio id=\"mwe_player_0\" controls=\"\" preload=\"none\" style=\"width:175px\" class=\"kskin\" data-durationhint=\"1.3\" data-startoffset=\"0\" data-mwtitle=\"en-us-architecture.ogg\" data-mwprovider=\"wikimediacommons\"><source src=\"//upload.wikimedia.org/wikipedia/commons/5/51/En-us-architecture.ogg\" type=\"audio/ogg; codecs=&quot;vorbis&quot;\" data-title=\"Original Ogg file (103 kbps)\" data-shorttitle=\"Ogg source\" data-width=\"0\" data-height=\"0\" data-bandwidth=\"103274\"/><source src=\"//upload.wikimedia.org/wikipedia/commons/transcoded/5/51/En-us-architecture.ogg/En-us-architecture.ogg.mp3\" type=\"audio/mpeg\" data-title=\"MP3\" data-shorttitle=\"MP3\" data-transcodekey=\"mp3\" data-width=\"0\" data-height=\"0\"/></audio></div></td><td class=\"audiometa\" style=\"font-size: 80%;\">(<a href=\"/wiki/File:en-us-architecture.ogg\" title=\"File:en-us-architecture.ogg\">file</a>)</td></tr></tbody></table></li>\n"
                    "<li>Hyphenation: <span class=\"Latn\" lang=\"en\">ar&#x2027;chi&#x2027;tec&#x2027;ture</span></li></ul>";
  xmlDocPtr doc = xmlParseMemory(xml.c_str(), (int)xml.length());
  std::cout << "---- each node, child first (depth-first search) ----" << '\n';
  xml_each_child(xmlDocGetRootElement(doc)->children, [](auto node) {
    std::cout << "node: " << node->name << std::endl;
    if (tdscript::xml_check_eq(node->name, "audio")) {
      stop = true;
      return 1;
    }
    return 0;
  });
  if (!stop) { exit(1); }

  tdscript::stop = false;
  tdscript::data_ready = true;
  auto client = tdscript::Client(0);

  std::string lang = "en";

  std::int8_t callback_count = 0;
  client.dict_get_content(lang, "architecture", [&](auto content) {
    std::string desc = std::string(content[0].begin(), content[0].end());
    std::cout << "got: \n-------------\n" << desc << "\n-----------" << std::endl;

    if (++callback_count > 1) {
      tdscript::stop = true;
    }
  });

  for (int i = 0; i < 999 && !tdscript::stop; i++) {
    client.dns_client.receive(tdscript::SOCKET_TIME_OUT_MS);

    client.process_tasks(std::time(nullptr));
  }

  if (!tdscript::stop) {
    return 1;
  }
}