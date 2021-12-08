// created by lich 12/04/2020, no copyright

#include "tdscript/client.h"

#include "gtest/gtest.h"


TEST(RandomTest, Create) {
  EXPECT_EQ(1, 1) << "1 == 1";
  EXPECT_EQ("0.1", tdscript::VERSION) << "version";

  EXPECT_EQ(false, tdscript::data_ready);
  EXPECT_EQ(0, tdscript::players_message[-1001098611371]);
  EXPECT_EQ(false, tdscript::save_flag);

  tdscript::load();

  EXPECT_EQ(true, tdscript::data_ready);
  EXPECT_EQ(false, tdscript::save_flag);

  tdscript::save_flag = true;
  tdscript::players_message[-1001098611371] = 2739049332736;
  tdscript::save();
  tdscript::load();
  EXPECT_EQ(2739049332736, tdscript::players_message[-1001098611371]);

  tdscript::save_flag = true;
  tdscript::players_message[-1001098611371] = 0;
  tdscript::save();
  tdscript::load();
  EXPECT_EQ(0, tdscript::players_message[-1001098611371]);

}
