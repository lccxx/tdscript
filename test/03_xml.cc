// created by lccc 12/25/2020, no copyright

// #include "tidy.h"

#include "tdscript/client.h"
#include "libxml/parser.h"

#include <string>
#include <iostream>

int main() {
  using namespace tdscript;

  xmlInitParser();

  std::string text = R"(
    <div id="root">
      <div id="1-1">
        <div id="2-11"></div>
        <div id="2-12"></div>
        <div id="2-13">
          <div id="3-131">
            <div id="4-1311"></div>
          </div>
        </div>
      </div>
      <div id="1-2">
        <div id="2-21"></div>
        <div id="2-22"></div>
      </div>
      <div id="1-3">
        <div id="2-31">
          <div id="3-311"></div>
        </div>
        <div id="2-32"></div>
      </div>
    </div>)";
  xmlDocPtr doc = xmlParseMemory(text.c_str(), (int)text.length());

  std::cout << "---- each node, next first (breadth-first search) ----" << '\n';
  xml_each_next(xmlDocGetRootElement(doc)->children, [](auto node) {
    std::string node_id = xml_get_prop(node, "id");
    std::cout << "node: '" << node->name << (node_id.empty() ? "" : "#" + node_id) << "'\n";
    if (node_id == "4-1311") {
      stop = true;
      return 1;
    }
    return 0;
  });

  std::cout << "---- each node, child first (depth-first search) ----" << '\n';
  xml_each_child(xmlDocGetRootElement(doc)->children, [](auto node) {
    std::string node_id = xml_get_prop(node, "id");
    std::cout << "node: '" << node->name << (node_id.empty() ? "" : "#" + node_id) << "'\n";
    if (node_id == "2-32") {
      stop = true;
      return 1;
    }
    return 0;
  });

  if (!stop) {
    return 1;
  }

  xmlFreeDoc(doc);

  text = R"(<div class="mw-parser-output">
    <div class="redirectMsg">
      <p>Redirect to:</p>
      <ul class="redirectText">
        <li><a href="/wiki/Preposition_and_postposition" title="Preposition and postposition">Preposition and postposition</a></li>
      </ul>
    </div>
  </div>)";

  doc = xmlParseMemory(text.c_str(), (int)text.length());
  xml_each_next(xmlDocGetRootElement(doc)->children, [](auto node) {
    std::string node_class = xml_get_prop(node, "class");
    std::cout << "node: '" << node->name << (node_class.empty() ? "" : "." + node_class) << "'\n";
    if (xml_check_eq(node->name, "div") && node_class == "redirectMsg") {
      xml_each_child(node->children, [](auto child) {
        if (xml_check_eq(child->name, "a")) {
          std::string redirect_title = xml_get_prop(child, "title");
          std::cout << "redirect title: " << redirect_title << '\n';
          if (redirect_title != "Preposition and postposition") {
            exit(2);
          }
          return 1;
        }
        return 0;
      });
      return 1;
    }
    return 0;
  });

  xmlFreeDoc(doc);
  xmlCleanupParser();
}