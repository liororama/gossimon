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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <msx_error.h>
#include <msx_debug.h>

#include <ProviderInfoModule.h>

int debug=0;

char bigBuff[8192];

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


struct test_info {
        int mem;
        int swap;
};

void set_fixed_data(node_info_t *ninfo)
{
        struct test_info *tip;
        
        ninfo->hdr.psize = NINFO_SZ + sizeof(struct test_info);
        ninfo->hdr.fsize = NINFO_SZ + sizeof(struct test_info);

        tip = (struct test_info *)((char*)ninfo + NINFO_SZ);
        tip->mem = 1000;
        tip->swap = 4000;
}


//************* Definition of two test info modules ***********
int pim_1_init(void **data, void *init_data) {return 1;}
int pim_1_free(void **data) {return 1;}
int pim_1_update(void *data) {return 1;}

char *pim_1_data = "11111";
int pim_1_get(void *module_data, void *data, int *size)
{
        sprintf((char *)data, "%s", pim_1_data);
        *size = strlen(pim_1_data) + 1;
        return 1;
}

int pim_2_init(void **data, void *init_data) {return 1;}
int pim_2_free(void **data) {return 1;}
int pim_2_update(void *data) {return 1;}

char *pim_2_data = "22222";
int pim_2_get(void *module_data, void *data, int *size)
{
        sprintf((char *)data, "%s", pim_2_data);
        *size = strlen(pim_2_data) + 1;
        return 1;
}


pim_entry_t  pim_arr[] =
{
        { name         : "pim_1",
          init_func    : pim_1_init,
          free_func    : pim_1_free,
          update_func  : pim_1_update,
          get_func     : pim_1_get,
          period       : 5,
	  init_data    : NULL,
        },
        
        { name         : "pim_2",
          init_func    : pim_2_init,
          free_func    : pim_2_free,
          update_func  : pim_2_update,
          get_func     : pim_2_get,
          period       : 5,
	  init_data    : NULL,
        }
};



START_TEST (test_basic)
{
        int res;
        pim_t pim;
        node_info_t *ninfo = (node_info_t *)bigBuff;
        
        print_start("Basic pim functionality");
        msx_set_debug(0);
        
        pim = pim_init(pim_arr, 2);
        fail_unless(pim != NULL, "Failed to create pim object");
        
        res = pim_update(pim);

        set_fixed_data(ninfo);
        pim_packInfo(pim, ninfo);

        int size;
        char *data = get_vlen_info(ninfo, pim_arr[0].name, &size);
        fail_unless(data != NULL, "Getting vlen name failed");
        fail_unless(strcmp(data, pim_1_data) == 0, "Data after get_vlen is not the same");

        data = get_vlen_info(ninfo, pim_arr[1].name, &size);
        fail_unless(data != NULL, "Getting vlen name 2 failed");
        fail_unless(strcmp(data, pim_2_data) == 0, "Data after get_vlen 2 is not the same");
        
        pim_free(pim);
        print_end();
        msx_set_debug(0);
} 
END_TEST



/***************************************************/
Suite *pim_suite(void)
{
  Suite *s = suite_create("Provider Info Module");

  TCase *tc_basic = tcase_create("basic");

  suite_add_tcase (s, tc_basic);

  tcase_add_test(tc_basic, test_basic);

  return s;
}


int main(int argc, char **argv)
{
  int nf;

  msx_set_debug(0);
  if(argc > 1)
          debug = 1;
  
  
  Suite *s = pim_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
