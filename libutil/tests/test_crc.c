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
#include <checksum.h>

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


char *crc32_str_1 = "0123456789";
char *crc32_str_2 = "0123456788";

START_TEST (test_crc32)
{
        print_start("Testing crc32");
        //msx_set_debug(PRIOD_DEBUG);

        int crc1 = crc32(crc32_str_1, strlen(crc32_str_1));
        int crc2 = crc32(crc32_str_2, strlen(crc32_str_2));

        if(debug)
                fprintf(stderr, "crc1 : %u, crc2 : %u\n", crc1, crc2);
        
        fail_unless(crc1 != crc2, "Different strings should produce different crc32");
        print_end();

} 
END_TEST

/***************************************************/
Suite *pw_suite(void)
{
  Suite *s = suite_create("Checksum");

  TCase *tc_crc32 = tcase_create("crc32");

  suite_add_tcase (s, tc_crc32);

  tcase_add_test(tc_crc32, test_crc32);

  return s;
}


int main(int argc, char **argv)
{
  int nf;

  if(argc > 1)
          debug = 1;
  
  
  Suite *s = pw_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
