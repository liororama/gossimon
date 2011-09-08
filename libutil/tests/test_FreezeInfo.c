/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <unistd.h>
#include <stdio.h>
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

//#include <common.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <info.h>
#include <FreezeInfo.h>

int debug=0;

static char *curr_msg;
void print_start(char *msg)
{
	curr_msg = msg;
	if(debug)
		printf("\n================ %15s ===============\n", msg);
}

void print_end()
{
	if(debug)
		printf("\n++++++++++++++++ %15s +++++++++++++++\n", curr_msg);
}

START_TEST (test_basic)
{
        int                  res;
        freeze_info_status_t stat;

        print_start("Testing basic");
        //msx_set_debug(PRIOD_DEBUG);
        
        
        res = strToFreezeInfoStatus(freezeInfoStatusStr(FRZINF_INIT), &stat);
        fail_unless(stat == FRZINF_INIT, "Translation of FRZINF_INIT failed");

        res = strToFreezeInfoStatus(freezeInfoStatusStr(FRZINF_ERR), &stat);
        fail_unless(stat == FRZINF_ERR, "Translation of FRZINF_ERR failed");
        
        res = strToFreezeInfoStatus(freezeInfoStatusStr(FRZINF_OK), &stat);
        fail_unless(stat == FRZINF_OK, "Translation of FRZINF_OK failed");

        print_end();
        msx_set_debug(0);
} 
END_TEST


char *frz_good_1 =
"<" ITEM_FREEZE_INFO_NAME "  stat=\"ok\" total=\"1000\" free=\"500\" />";

START_TEST (test_good)
{
        int                res;
        freeze_info_t      fi;
        
        print_start("Testing good freeze info");
        msx_set_debug(1);

        res = getFreezeInfo(frz_good_1, &fi);
        fail_unless(res == 1, "Failed to parse frz_good_1");
        fail_unless(fi.status == FRZINF_OK, "Status should be ok (frz_good_1)");
        fail_unless(fi.total_mb == 1000, "Total should be 1000 (frz_good_1)");
        fail_unless(fi.free_mb == 500, "Total should be 500 (frz_good_1)");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


char *frz_bad_1 =
"< blabla  stat=\"ok\" total=\"1000\" free=\"500\" />";

char *frz_bad_2 =
"</ blabla  stat=\"ok\" total=\"1000\" free=\"500\" />";


START_TEST (test_bad)
{
        int                res;
        freeze_info_t      fi;
        
        print_start("Testing good external info");
        msx_set_debug(1);

        res = getFreezeInfo(frz_bad_1, &fi);
        fail_unless(res == 0, "frz_bad_1 should fail");
        
        res = getFreezeInfo(frz_bad_2, &fi);
        fail_unless(res == 0, "frz_bad_2 should fail");
        
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


START_TEST (test_write)
{
        int                res;
        freeze_info_t      fi, fi2;
        char               buff[1024];
        
        print_start("Testing good external info");
        msx_set_debug(1);

        fi.status = FRZINF_OK;
        fi.total_mb = 2000;
        fi.free_mb = 1000;
        
        res =writeFreezeInfo(&fi, buff);
        if(debug) {
                printf("Got freezeinfo xml\n%s\n", buff);
        }

        res = getFreezeInfo(buff, &fi2);
        fail_unless(res == 1, "Failed to parse frz_good_1");
        fail_unless(fi2.status == FRZINF_OK, "Status should be ok");
        fail_unless(fi2.total_mb == 2000, "Total should be 2000");
        fail_unless(fi2.free_mb == 1000, "Total should be 1000");
        
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


/***************************************************/
Suite *pw_suite(void)
{
  Suite *s = suite_create("FreezeInfo");

  TCase *tc_basic = tcase_create("basic");
  TCase *tc_good = tcase_create("good");
  TCase *tc_bad = tcase_create("bad");

  suite_add_tcase (s, tc_basic);
  suite_add_tcase (s, tc_good);
  suite_add_tcase (s, tc_bad);

  tcase_add_test(tc_basic, test_basic);
  tcase_add_test(tc_good, test_good);
  tcase_add_test(tc_bad, test_bad);
  tcase_add_test(tc_bad, test_write);

  return s;
}


int main(int argc, char **argv)
{
  int nf;

  msx_set_debug(0);
  if(argc > 1)
          debug = 1;
  
  
  Suite *s = pw_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
