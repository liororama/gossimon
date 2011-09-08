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
#include <math.h>
//#include <common.h>
#include <msx_error.h>
#include <msx_debug.h>
#include <info.h>
#include <EcoInfo.h>

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


char *good_xml_1 =
"<echo-info> "
"     <on-time>14.2</on-time> "
"     <status> on  </status> "
"     <curr-price>12</curr-price> "
"     <min-price>10.8</min-price> "
"</echo-info> ";

START_TEST (test_good)
{
        int                res;
        economy_info_t     eInfo;
        
        print_start("Testing good Eco info");
        msx_set_debug(1);
        
        res = parseEconomyInfo(good_xml_1, &eInfo);
        fail_unless(res == 1, "Failed to parse xml_good_1");
        fail_unless(eInfo.status == ECONOMY_ON, "Status should be on (xml_good_1)");
        fail_unless(fabs(eInfo.timeLeft - 14.2) < 0.00001, "Time left should be 14.2 (xml_good_1)");
        fail_unless(fabs(eInfo.minPrice - 10.8) < 0.00001, "Min Price  should be 10.8 (xml_good_1)");
        fail_unless(fabs(eInfo.currPrice - 12) < 0.00001,  "Curr Price  should be 12 (xml_good_1)");
        print_end();
        msx_set_debug(0);
} 
END_TEST


char *extended_xml_1 =
"<echo-info> "
"     <on-time>14.2</on-time> "
"     <status> protected  </status> "
"     <protect-time> 50 </protect-time>"     
"     <min-price>10.8</min-price> "
"     <curr-price>12</curr-price> "
"     <sched-price>12</sched-price>"
"     <inc-price> 4 </inc-price>"
"</echo-info> ";

START_TEST (test_extended_xml)
{
        int                res;
        economy_info_t     eInfo;
        
        print_start("Testing extended Eco info");
        msx_set_debug(1);
        
        res = parseEconomyInfo(extended_xml_1, &eInfo);
        fail_unless(res == 1, "Failed to parse xml_good_1");
        fail_unless(eInfo.status == ECONOMY_PROTECTED, "Status should be protected (extended_xml_1)");
        fail_unless(fabs(eInfo.timeLeft - 14.2) < 0.00001, "Time left should be 14.2 (extended_xml_1)");
        fail_unless(fabs(eInfo.minPrice - 10.8) < 0.00001, "Min Price should be 10.8 (extended_xml_1)");
        fail_unless(fabs(eInfo.currPrice - 12) < 0.00001, "Curr Price should be 12 (extended_xml_1)");
        fail_unless(fabs(eInfo.schedPrice - 12) < 0.00001, "Sched Price should be 12 (extended_xml_1)");
        fail_unless(fabs(eInfo.incPrice - 4) < 0.00001,  "Inc Price should be 4 (extended_xml_1)");
        fail_unless(eInfo.protectedTime == 50,  "Protected Time should be 12 (extended_xml_1)");

        print_end();
        msx_set_debug(0);
} 
END_TEST




char *error_xml_1 =
"<echo-info> "
"     <error>no file</error> "
"</echo-info> ";

START_TEST (test_error)
{
        int                res;
        economy_info_t     eInfo;
        
        print_start("Testing error Eco info");
        msx_set_debug(1);
        
        res = parseEconomyInfo(error_xml_1, &eInfo);
        fail_unless(res == 1, "Failed to parse error_xml_1");
        fail_unless(eInfo.status == ECONOMY_ERROR, "Status should be ERROR (error_xml_1)");
        print_end();
        msx_set_debug(0);
} 
END_TEST





char *bad_xml_1 =
"<echo-info> "
"     <on-time>14.2</on-time> "
"     <status> bla  </status> "
"     <curr-price>12</curr-price> "
"     <min-price>10.8</min-price> "
"</echo-info> ";

char *bad_xml_2 =
"<echo-info> "
"     <on-time>14.2</on-time> "
"     <status> on  </status> "
"     <curr-price>12</curr-price> "
"     <min-price>xxx</min-price> "
"</echo-info> ";



START_TEST (test_bad)
{
     int                res;
     economy_info_t     eInfo;
     
     print_start("Testing good external info");
     msx_set_debug(1);

     res = parseEconomyInfo(bad_xml_1, &eInfo);
     fail_unless(res == 0, "bad_xml_1 should fail");
     
     res = parseEconomyInfo(bad_xml_2, &eInfo);
     fail_unless(res == 0, "bad_xml_2 should fail");
     
     
     print_end();
     msx_set_debug(0);
}
END_TEST



/***************************************************/
Suite *pw_suite(void)
{
  Suite *s = suite_create("EcoInfo");

  TCase *tc_good = tcase_create("good");
  TCase *tc_bad = tcase_create("bad");

  suite_add_tcase (s, tc_good);
  suite_add_tcase (s, tc_bad);

  tcase_add_test(tc_good, test_good);
  tcase_add_test(tc_good, test_error);
  tcase_add_test(tc_good, test_extended_xml);
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
