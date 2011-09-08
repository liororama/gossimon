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
#include <JMigrateInfo.h>

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
"<jmigrate type=\"host\">"
"  <machines num=\"1\"> "
"      <vm>  "
"          <id>bla</id> "
"          <mem>256</mem> "
"          <home>cmos-16</home>"
"          <client>mos1</client>"
"      </vm>"
"  </machines> "
"</jmigrate>";

START_TEST (test_good)
{
        int                res;
        jmigrate_info_t    jinfo;
        
        print_start("Testing good JMigrate info");
        msx_set_debug(1);
        
        res = parseJMigrateInfo(good_xml_1, &jinfo);
        fail_unless(res == 1, "Failed to parse xml_good_1");
        fail_unless(jinfo.status == JMIGRATE_HOST, "Status should be host (xml_good_1)");
        fail_unless(jinfo.vmNum == 1, "Number of vm is not 1 (xml_good_1)");
        fail_unless(jinfo.vmInfoArr[0].mem == 256, "VM mem != 256 (xml_good_1)");
        fail_unless(strcmp("cmos-16", jinfo.vmInfoArr[0].home) == 0, "Home != cmos-16 (xml_good_1)");
        print_end();
        msx_set_debug(0);
} 
END_TEST


char *good_xml_2 =
"<jmigrate type=\"host\">"
"  <machines num=\"0\"> "
"  </machines> "
"</jmigrate>";

START_TEST (test_good_2)
{
        int                res;
        jmigrate_info_t    jinfo;
        
        print_start("Testing good JMigrate info");
        msx_set_debug(1);
        
        res = parseJMigrateInfo(good_xml_2, &jinfo);
        fail_unless(res == 1, "Failed to parse xml_good_2");
        fail_unless(jinfo.status == JMIGRATE_HOST, "Status should be host (xml_good_2)");
        fail_unless(jinfo.vmNum == 0, "Number of vm is not 0 (xml_good_2)");
        print_end();
        msx_set_debug(0);
} 
END_TEST


char *good_xml_3 =
"<jmigrate type=\"vm\">\n"
"  <id>bla</id>\n"
"  <host>cmos-16</host>\n"
"  <home>cmos-16</home>\n"
"  <client>mos1</client>\n"
"</jmigrate>";

START_TEST (test_good_3)
{
        int                res;
        jmigrate_info_t    jinfo;
        
        print_start("Testing good JMigrate info");
        msx_set_debug(1);
        
        res = parseJMigrateInfo(good_xml_3, &jinfo);
        fail_unless(res == 1, "Failed to parse xml_good_3");
        fail_unless(jinfo.status == JMIGRATE_VM, "Status should be host (xml_good_3)");
        fail_unless(jinfo.vmNum == 1, "Number of vm is not 1 (xml_good_3)");
        fail_unless(strcmp(jinfo.vmInfoArr[0].host, "cmos-16")==0, "Host is not cmos-16 (good_xml_3)");
        fail_unless(strcmp(jinfo.vmInfoArr[0].client, "mos1")==0, "Client is not mos1 (good_xml_3)");
        print_end();
        msx_set_debug(0);
} 
END_TEST

char *error_xml_1 =
"<jmigrate>\n"
"  <error>open</error>\n"
"</jmigrate>";

START_TEST (test_error_1)
{
        int                res;
        jmigrate_info_t    jinfo;
        
        print_start("Testing error JMigrate info");
        msx_set_debug(1);
        
        res = parseJMigrateInfo(error_xml_1, &jinfo);
        fail_unless(res == 1, "Failed to parse error_xml_1");
        fail_unless(jinfo.status == JMIGRATE_ERROR, "Status should be error (error_xml_3)");
        fail_unless(jinfo.vmNum == 0, "Number of vm is not 0 (xml_error_3)");
        fail_unless(strcmp(jinfo.errMsg, "open")==0, "Error msg is not open (error_xml_3)");
        freeJMigrateInfo(&jinfo);
        print_end();
        msx_set_debug(0);
} 
END_TEST



/***************************************************/
Suite *pw_suite(void)
{
  Suite *s = suite_create("JMigrateInfo");

  TCase *tc_good = tcase_create("good");
  //TCase *tc_bad = tcase_create("bad");

  suite_add_tcase (s, tc_good);
  //suite_add_tcase (s, tc_bad);

  tcase_add_test(tc_good, test_error_1);
  tcase_add_test(tc_good, test_good);
  tcase_add_test(tc_good, test_good_2);
  tcase_add_test(tc_good, test_good_3);

  
  
  //tcase_add_test(tc_good, test_extended_xml);
  //tcase_add_test(tc_bad, test_bad);

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
