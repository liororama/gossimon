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

#include <ConfFileReader.h>


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

START_TEST (test_basicFile)
{
        char        fileName[] = "./conf-files/conf_file_1";
        conf_file_t cf;
        conf_var_t  varEnt;
        int         res;

        char        *validVarNames[] = { "size", "dir", "name", NULL};
        print_start("Testing basic");
        if(debug) msx_set_debug(MAP_DEBUG);

        cf = cf_new(validVarNames);
        fail_unless(cf != NULL, "Faild to build conf file object");
        
        res = cf_parseFile(cf, fileName);
        fail_unless(res == 1, "Failed to parse a valid conf file\n");

        res = cf_getVar(cf, "name", &varEnt);
        fail_unless(res == 1 && strcmp(varEnt.varValue, "lior") == 0,
                    "Failed to retrive var: name\n");
        
        res = cf_getVar(cf, "blablabla", &varEnt);
        fail_unless(res == 0, "Variable blablabla should not exists");
        
        print_end();
        msx_set_debug(0);
} 
END_TEST

char *basicStr_1 =
"# A comment \n"
"\n"
"# Another comment after empty line\n"
"\n"
"size = 256 \n"
"dir = /tmp \n"
"name=lior \n"
"\n\n ";

char *basicStr_2 =
"# A comment \n"
"\n"
"# Another comment after empty line\n"
"\n"
"  size = 256 \n"
" dir = /tmp \n"
"\tname=lior \n"
"\n\n ";

START_TEST (test_basicStr)
{
        conf_file_t cf;
        conf_var_t  varEnt;
        int         res;

        char        *validVarNames[] = { "size", "dir", "name", NULL};
        print_start("Testing basic");
        if(debug) msx_set_debug(MAP_DEBUG);

        cf = cf_new(validVarNames);
        fail_unless(cf != NULL, "Faild to build conf file object");
        
        res = cf_parse(cf, basicStr_1);
        fail_unless(res == 1, "Failed to parse a valid conf file\n");

        res = cf_getVar(cf, "name", &varEnt);
        fail_unless(res == 1 && strcmp(varEnt.varValue, "lior") == 0,
                    "Failed to retrive var: name\n");
        
        res = cf_getVar(cf, "blablabla", &varEnt);
        fail_unless(res == 0, "Variable blablabla should not exists");

	cf_free(cf);
	

	// Second basic str
	cf = cf_new(validVarNames);
        fail_unless(cf != NULL, "Faild to build conf file object");
        
        res = cf_parse(cf, basicStr_2);
        fail_unless(res == 1, "Failed to parse a valid conf file\n");

        res = cf_getVar(cf, "name", &varEnt);
        fail_unless(res == 1 && strcmp(varEnt.varValue, "lior") == 0,
                    "Failed to retrive var: name\n");
        
        res = cf_getVar(cf, "blablabla", &varEnt);
        fail_unless(res == 0, "Variable blablabla should not exists");

	
	
        print_end();
        msx_set_debug(0);
} 
END_TEST

char *varIntStr_1 =
"# A comment \n"
"\n"
"# Another comment after empty line\n"
"\n"
" size = 256 \n"
" dir = /tmp \n"
"\tname=lior \n"
"\n\n ";

START_TEST (test_getIntVar)
{
        conf_file_t cf;
        int         res;

        char        *validVarNames[] = { "size", "dir", "name", NULL};
        print_start("Testing basic");
        if(debug) msx_set_debug(MAP_DEBUG);

        cf = cf_new(validVarNames);
        fail_unless(cf != NULL, "Faild to build conf file object");
        
        res = cf_parse(cf, varIntStr_1);
        fail_unless(res == 1, "Failed to parse a valid conf file\n");

	int intVal;
        res = cf_getIntVar(cf, "size", &intVal);
        fail_unless(res == 1 , "Failed to retrive integer var: size\n");
        fail_unless(intVal == 256, "Wrong error in getIntVar\n");
	
	cf_free(cf);
	
        print_end();
        msx_set_debug(0);
} 
END_TEST


char *varListStr_1 =
"# A comment \n"
"\n"
"# Another comment after empty line\n"
"\n"
" names = lior,tal,amnon,danny \n"
" names2 = lior ,tal, amnon , danny\n"
"\n\n ";

START_TEST (test_getListVar)
{
        conf_file_t cf;
        int         res;

        char        *validVarNames[] = { "names", "names2", NULL};
        print_start("Testing getListVar");
        if(debug) msx_set_debug(MAP_DEBUG);

        cf = cf_new(validVarNames);
        fail_unless(cf != NULL, "Faild to build conf file object");

        res = cf_parse(cf, varListStr_1);
        fail_unless(res == 1, "Failed to parse a valid conf file\n");

        GPtrArray *ptrArr = g_ptr_array_new();
        res = cf_getListVar(cf, "names", ",", ptrArr);
        fail_unless(res == 1 , "Failed to retrive list var: names\n");
        fail_unless(strcmp((g_ptr_array_index (ptrArr, 0)), "lior") == 0, "First element is not lior [%s]",
            g_ptr_array_index (ptrArr, 0));
        fail_unless(strcmp((g_ptr_array_index (ptrArr, 3)), "danny") == 0, "4'th element is not danny [%s]",
            g_ptr_array_index (ptrArr, 3));



        ptrArr = g_ptr_array_new();
        res = cf_getListVar(cf, "names2", ", ", ptrArr);
        fail_unless(res == 1 , "Failed to retrive list var: names2\n");
        fail_unless(strcmp((g_ptr_array_index (ptrArr, 0)), "lior") == 0, "First element is not lior [%s]",
            g_ptr_array_index (ptrArr, 0));
        fail_unless(strcmp((g_ptr_array_index (ptrArr, 3)), "danny") == 0, "4'th element is not danny [%s]",
            g_ptr_array_index (ptrArr, 3));

        

	cf_free(cf);

        print_end();
        msx_set_debug(0);
}
END_TEST


START_TEST (test_bad_file_1)
{
        char        fileName[] = "./conf-filesls/conf_file_1";
        conf_file_t cf;
        int         res;

        char        *validVarNames[] = { "size", NULL};
        print_start("Testing basic");
        if(debug) msx_set_debug(MAP_DEBUG);

        cf = cf_new(validVarNames);
        fail_unless(cf != NULL, "Faild to build conf file object");
        
        res = cf_parseFile(cf, fileName);
        fail_unless(res == 0, "Failed to parse a valid conf file\n");

        cf_free(cf);
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


/***************************************************/
Suite *mapper_suite(void)
{
  Suite *s = suite_create("Conf File Reader");

  TCase *tc_basic = tcase_create("basic");
  TCase *tc_bad = tcase_create("bad");

  suite_add_tcase (s, tc_basic);
  suite_add_tcase (s, tc_bad);
  

  tcase_add_test(tc_basic, test_basicFile);
  tcase_add_test(tc_basic, test_basicStr);
  tcase_add_test(tc_basic, test_getIntVar);
  tcase_add_test(tc_basic, test_getListVar);

  tcase_add_test(tc_bad, test_bad_file_1);
  
  
  return s;
}


int main(int argc, char **argv)
{
  int nf;

  msx_set_debug(0);
  if(argc > 1)
          debug = 1;
  
  
  Suite *s = mapper_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
