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

#include <pe.h>
#include <info.h>
#include <info_reader.h>

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

/*
 * Return a newly allocated buffer holding the document
 */
char*
read_desc_file( const char* docname, int* strsz ) {

	struct stat status;
	char* buff;
	int fd;

	/* Get file status */ 
	if( stat( docname, &status ) < 0 ) {
		debug_r( "Error: Failed stat\n%s", strerror(errno) );
		return NULL;
	}
      
	if((fd = open( docname, O_RDONLY )) < 0 ){
		debug_r( "Error: Failed open\n%s", strerror(errno) );
		return NULL;
	}
      
	if( !( buff = (char*)malloc( status.st_size +2))){
		debug_r( "Error: Failed malloc\n" );
		return NULL;
	}

	if( read( fd, buff, status.st_size) != status.st_size ){
		debug_r( "Error: Failed read\n%s", strerror(errno) );
		free( buff ) ;
		buff = NULL;
	}
        // Just in case
        buff[status.st_size] = '\0';
	close(fd);
	*strsz = status.st_size; 
	return buff;
}



char vlen_desc_1[500];

void set_vlen_desc_1() {
        char *p = vlen_desc_1;
        vlen_desc_1[0] = '\0';
        
        strcat(p, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
        strcat(p, "<local_info>\n");
        strcat(p, "        <base name=\"mem\"        type=\"unsigned short\"  unit=\"4KB\"/>\n");
        strcat(p, "        <base name=\"swap\"       type=\"unsigned long\"   unit=\"4KB\"/>\n");
        strcat(p, "        <vlen name=\"pid-stat\"   type=\"string\" />\n");
        strcat(p, "        <vlen name=\"usage-info\" type=\"string\" />\n");
        strcat(p, "</local_info>\n" );
 
}

char *vlen_data_1  = "pid:111 running 90%\npid:222 running 80%\n";
char *vlen_data_2  = "C1 1927\nC4 0 1 12\n";


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

char buff[4096];

START_TEST (test_vlen)
{
	variable_map_t *mapping = NULL;
        var_t          *var;
        var_t          *var_vlen;
        node_info_t    *ninfo = (node_info_t *)buff;
        int             res;
        char           *vlenData;
        int             size;
        
        print_start("Testing 1 vlen item");
        msx_set_debug(1);

        set_vlen_desc_1();
        if(debug) printf("desc:\n%s\n", vlen_desc_1);
        
        set_fixed_data(ninfo);
        
	mapping = create_info_mapping( vlen_desc_1 );
        fail_unless(mapping != NULL, "Failed to create variable mapping");
        if(debug) variable_map_print( mapping ) ;
        
        var = get_var_desc( mapping, "mem" ) ;
        fail_unless( var != NULL, "Failed to get mem variable info");
        
        var_vlen = get_var_desc( mapping, "pid-stat" ) ;
        fail_unless( var_vlen != NULL, "Failed to get vlen variable pid-stat info");

        res = add_vlen_info(ninfo, "pid-stat", vlen_data_1, strlen(vlen_data_1)+1);
        fail_unless(res, "Failed to add vlen_1 info");
        fail_unless(IS_VLEN_INFO(ninfo), "There is no vlen info when should be\n");

        
        vlenData = get_vlen_info(ninfo, "pid-stat", &size);
        fail_unless(vlenData != NULL, "Failed to retrieve vlen pid-stat");
        fail_unless(size == strlen(vlen_data_1)+1, "Length of vlen_data_1 is not correct");

        vlenData = get_vlen_info(ninfo, "xxxxx", &size);
        fail_unless(vlenData == NULL, "Non existing vlen item found");
        

        if(debug) {
                print_vlen_items(stderr, ninfo);
                
        }
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


START_TEST (test_vlen_2)
{
	variable_map_t *mapping = NULL;
        var_t          *var;
        var_t          *var_vlen;
        node_info_t    *ninfo = (node_info_t *)buff;
        int             res;
        char           *vlenData;
        int             size;
        
        print_start("Testing 2 vlen items");
        msx_set_debug(1);

        set_vlen_desc_1();
        if(debug) printf("desc:\n%s\n", vlen_desc_1);
        
        set_fixed_data(ninfo);
        
	mapping = create_info_mapping( vlen_desc_1 );
        fail_unless(mapping != NULL, "Failed to create variable mapping");
        var = get_var_desc( mapping, "mem" ) ;
        fail_unless( var != NULL, "Failed to get mem variable info");
        
        var_vlen = get_var_desc( mapping, "pid-stat" ) ;
        fail_unless( var_vlen != NULL, "Failed to get vlen variable pid-stat info");

        res = add_vlen_info(ninfo, "pid-stat", vlen_data_1, strlen(vlen_data_1)+1);
        fail_unless(res, "Failed to add vlen_1 info");

        res = add_vlen_info(ninfo, "usage-info", vlen_data_2, strlen(vlen_data_2)+1);
        fail_unless(res, "Failed to add vlen_2 info");

        
        vlenData = get_vlen_info(ninfo, "pid-stat", &size);
        fail_unless(vlenData != NULL, "Failed to retrieve vlen pid-stat");
        fail_unless(size == (strlen(vlen_data_1)+1), "Length of vlen_data_1 is not correct");
        fail_unless(strcmp(vlenData, vlen_data_1)==0, "Vlen data and original data do not match");
        
        
        vlenData = get_vlen_info(ninfo, "usage-info", &size);
        fail_unless(vlenData != NULL, "Failed to retrieve vlen pid-stat");
        fail_unless(size == strlen(vlen_data_2)+1, "Length of vlen_data_2 is not correct");

        fail_unless(strcmp(vlenData, vlen_data_2)==0, "Vlen data and original data do not match");
        
        if(debug) {
                print_vlen_items(stderr, ninfo);
                variable_map_print( mapping ) ;
        }
        
        print_end();
        msx_set_debug(0);
} 
END_TEST



START_TEST (test_good)
{
        char           *str;
	int             size;
	variable_map_t *mapping = NULL;
        var_t          *var;
        
        print_start("Testing good descriptions");
        msx_set_debug(1);

        
	str = read_desc_file( "descriptions/good_desc_1.xml", &size );
        fail_unless(str != NULL, "Failed reading description file 1\n");
	mapping = create_info_mapping( str );
        if(debug) variable_map_print( mapping ) ;

        fail_unless(mapping != NULL, "Failed to create variable mapping");
        var = get_var_desc( mapping, "tmem" ) ;
        fail_unless( var != NULL, "Failed to get mem variable info\n");
        

        str = read_desc_file( "descriptions/good_desc_2.xml", &size );
        fail_unless(str != NULL, "Failed reading description file 2 \n");
	mapping = create_info_mapping( str );
        fail_unless(mapping != NULL, "Failed to create variable mapping");
        var = get_var_desc( mapping, "tmem" ) ;
        fail_unless( var != NULL, "Failed to get mem variable info\n");


        var = get_var_desc( mapping, "token_level" ) ;
        fail_unless( var != NULL, "Failed to get mem variable info\n");
        
        //var_external = get_var_desc( mapping, "external" ) ;
        //fail_unless( var_external != NULL, "Failed to get mem variable info\n");

        //fail_unless( var_external->size == 0, "Size of external is not 0");
        //fail_unless( var_external->offset == var->offset + var->size,
        //             "Offset of external is not correct");
        
        
        
        print_end();
        msx_set_debug(0);
} 
END_TEST


/***************************************************/
Suite *mapper_suite(void)
{
  Suite *s = suite_create("Info Reader");

  TCase *tc_good = tcase_create("good");
  TCase *tc_vlen = tcase_create("vlen");
  
  suite_add_tcase (s, tc_good);
  suite_add_tcase (s, tc_vlen);
  
  tcase_add_test(tc_good, test_good);
  tcase_add_test(tc_vlen, test_vlen);
  tcase_add_test(tc_vlen, test_vlen_2);
  
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
