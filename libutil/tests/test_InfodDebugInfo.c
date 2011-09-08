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
#include <InfodDebugInfo.h>

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


char *idi_good_1 =
"<" ITEM_INFOD_DEBUG_NAME "  nchild=\"4\">450</" ITEM_INFOD_DEBUG_NAME ">";

START_TEST (test_good)
{
        int                res;
        infod_debug_info_t idi;
        
        print_start("Testing good infod debug info");
        msx_set_debug(1);

        res = getInfodDebugInfo(idi_good_1, &idi);
        fail_unless(res == 1, "Failed to parse idi_good_1");
        fail_unless(idi.childNum == 4, "child num should be 4 (idi_good_1)");
        fail_unless(idi.uptimeSec == 450, "Uptime sec should be 450 (idi_good_1)");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


char *idi_bad_1 =
"< blabla  nchild=\"4\"  />";

char *idi_bad_2 =
"</ blabla  nchild=\"500\" />";

char *idi_bad_3 =
"<" ITEM_INFOD_DEBUG_NAME "  nchild=\"4\">aaa</" ITEM_INFOD_DEBUG_NAME ">";

START_TEST (test_bad)
{
        int                res;
        infod_debug_info_t idi;
        
        print_start("Testing good external info");
        msx_set_debug(1);

        res = getInfodDebugInfo(idi_bad_1, &idi);
        fail_unless(res == 0, "idi_bad_1 should fail");
        
        res = getInfodDebugInfo(idi_bad_2, &idi);
        fail_unless(res == 0, "idi_bad_2 should fail");
        
        res = getInfodDebugInfo(idi_bad_3, &idi);
        fail_unless(res == 0, "idi_bad_3 should fail");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST

/***************************************************/
Suite *pw_suite(void)
{
  Suite *s = suite_create("InfodDebugInfo");

  TCase *tc_good = tcase_create("good");
  TCase *tc_bad = tcase_create("bad");

  suite_add_tcase (s, tc_good);
  suite_add_tcase (s, tc_bad);

  tcase_add_test(tc_good, test_good);
  tcase_add_test(tc_bad, test_bad);

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
