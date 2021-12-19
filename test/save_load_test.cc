// created by lccc 12/04/2020, no copyright

#include "tdscript/client.h"

#include <cassert>

int main() {
  assert(tdscript::players_message.empty());
  assert(0 == tdscript::players_message[-1001098611371]);
  assert(1 == tdscript::players_message.size());
  assert(0 == tdscript::players_message[-1001031483587]);
  assert(2 == tdscript::players_message.size());

  assert(false == tdscript::data_ready);
  assert(false == tdscript::save_flag);


  tdscript::pending_extend_messages[-1001098611371].push_back(2739791724545);
  tdscript::pending_extend_messages[-1001098611371].push_back(2737428234241);
  tdscript::pending_extend_messages[-1001098611371].push_back(2737438720001);
  tdscript::players_message[-1001098611371] = 2739049332736;
  tdscript::players_message[-1001030076721] = 854676471808;
  tdscript::data_ready = true;
  tdscript::save_flag = true;
  tdscript::save();

  tdscript::pending_extend_messages[-1001098611371].clear();
  tdscript::players_message.clear();

  tdscript::load();
  assert(2737428234241 == tdscript::pending_extend_messages[-1001098611371][1]);
  assert(2737438720001 == tdscript::pending_extend_messages[-1001098611371][2]);
  assert(2739049332736 == tdscript::players_message[-1001098611371]);
  assert(854676471808 == tdscript::players_message[-1001030076721]);

  return 0;
}
