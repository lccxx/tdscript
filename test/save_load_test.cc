// created by lccc 12/04/2020, no copyright

#include "tdscript/client.h"

#include "gtest/gtest.h"


TEST(RandomTest, Create) {
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ("0.1", tdscript::VERSION) << "version";

  EXPECT_EQ(0, tdscript::players_message.size());
  EXPECT_EQ(0, tdscript::players_message[-1001098611371]);
  EXPECT_EQ(1, tdscript::players_message.size());
  EXPECT_EQ(0, tdscript::players_message[-1001031483587]);
  EXPECT_EQ(2, tdscript::players_message.size());

  EXPECT_EQ(false, tdscript::data_ready);
  EXPECT_EQ(false, tdscript::save_flag);


  tdscript::pending_extend_mesages[-1001098611371].push_back(2739791724545);
  tdscript::pending_extend_mesages[-1001098611371].push_back(2737428234241);
  tdscript::pending_extend_mesages[-1001098611371].push_back(2737438720001);
  tdscript::players_message[-1001098611371] = 2739049332736;
  tdscript::players_message[-1001030076721] = 854676471808;
  tdscript::data_ready = true;
  tdscript::save_flag = true;
  tdscript::save();

  tdscript::pending_extend_mesages[-1001098611371].clear();
  tdscript::players_message.clear();

  tdscript::load();
  EXPECT_EQ(2737428234241, tdscript::pending_extend_mesages[-1001098611371][1]);
  EXPECT_EQ(2737438720001, tdscript::pending_extend_mesages[-1001098611371][2]);
  EXPECT_EQ(2739049332736, tdscript::players_message[-1001098611371]);
  EXPECT_EQ(854676471808, tdscript::players_message[-1001030076721]);
}
