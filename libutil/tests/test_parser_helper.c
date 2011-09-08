/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#include <stdio.h>
#include <check.h>
#include <stdlib.h>
#include <string.h>


#include <parse_helper.h>


static char *curr_msg;
void print_start(char *msg)
{
	curr_msg = msg;
	printf("\n================ %15s ===============\n", msg);
}
void print_end()
{
	printf("\n++++++++++++++++ %15s +++++++++++++++\n", curr_msg);
}


START_TEST (test_split_str)
{

 char str[100];
 char *dirs[5];

 strcpy(str, "dir1,dir2");
 split_str(str, dirs, 5, ",");
 
 
 fail_unless( strcmp(dirs[0], "dir1") == 0,
	      "Spliting a string and the first element is not correct");

 fail_unless( strcmp(dirs[1], "dir2") == 0,
	      "Spliting a string and the second element is not correct");

 fail_unless( dirs[2] == NULL,
	      "Spliting a string and the last element is not NULL");


 char *items[10];
 int   num;
 strcpy(str, "item1\n");
 num = split_str(str, items, 10, "\n");
 fail_unless(num == 1, "Item num should be 1 (spliter is at end of string)");
 
}
END_TEST


START_TEST (test_split_single_str)
{

 char str[100];
 char *dirs[5];

 strcpy(str, "dir1");
 split_str(str, dirs, 5, ",");
 
 
 fail_unless( strcmp(dirs[0], "dir1") == 0,
	      "Spliting a string and the first element is not correct");

 fail_unless( dirs[1] == NULL,
	      "Spliting a string and the last element is not NULL");
 
}
END_TEST

START_TEST (test_split_empty_str)
{

 char str[100];
 char *dirs[5];

 strcpy(str, "");
 split_str(str, dirs, 5, ",");
 
 fail_unless( dirs[0] == NULL,
	      "Spliting an empty string and the first element is not NULL");
 
}
END_TEST

START_TEST (test_split_large_str)
{
 char str[100];
 char *dirs[5];

 strcpy(str, "d1,d2,d2,d3,d4,d5,d6,d7,d8");
 split_str(str, dirs, 5, ",");
 
 fail_unless( dirs[4] == NULL,
	      "Spliting a large string and checking if the last element is NULL");
 
}
END_TEST


char *tag_good_start_1 = "< proc ";
char *tag_good_start_2 = "<proc2 ";
char *tag_good_start_3 = "</proc-watch>";
char *tag_good_start_4 = "<proc-watch>";

char *tag_bad_start_1 = "proc ";
char *tag_bad_start_2 = "< ";

START_TEST (test_get_tag_start)
{
        char *ptr;
        char  buff[256];
        int   close_tag;
        // Good tests
        ptr = get_tag_start(tag_good_start_1, buff, &close_tag);
        fail_unless(strcmp(buff, "proc") == 0, "Faild to get good tag1 proc");
        fail_unless(close_tag == 0, "good start 1: close tag != 0");
        
        ptr = get_tag_start(tag_good_start_2, buff, &close_tag);
        fail_unless(strcmp(buff, "proc2") == 0, "Faild to get good tag2 proc");
        fail_unless(close_tag == 0, "good start 2: close tag != 0");
        
        ptr = get_tag_start(tag_good_start_3, buff, &close_tag);
        fail_unless(strcmp(buff, "proc-watch") == 0, "Faild to get good tag3 proc-watch");
        fail_unless(close_tag == 1, "good start 3: close tag != 1");

        ptr = get_tag_start(tag_good_start_4, buff, &close_tag);
        fail_unless(strcmp(buff, "proc-watch") == 0, "Faild to get good tag4 proc-watch");
        fail_unless(close_tag == 0, "good start 4: close tag != 0");
        fail_unless(*ptr == '>', "good start 4: ptr should be >");


        
        
        // Bad tests
        ptr = get_tag_start(tag_bad_start_1, buff, &close_tag);
        fail_unless(ptr == NULL, "Bad test 1 should fail");

        ptr = get_tag_start(tag_bad_start_2, buff, &close_tag);
        fail_unless(ptr == NULL, "Bad test 2 should fail");
 
}
END_TEST

char *attr_good_1 = " name = AAAA val = BBB";
char *attr_good_2 = " NNNN = lior />";
char *attr_good_3 = " />";
char *attr_good_4 = "attr=attrVal />";
char *attr_good_5 = "attr=attrVal/>";

char *attr_good_6 = "attr=\"no process\" />";

START_TEST (test_get_tag_attr)
{
        char *ptr;
        char name[256];
        char val[256];
        int  end;

        ptr = get_tag_attr(attr_good_1, name, val, &end);
        fail_unless(strcmp(name, "name") == 0, "Failed to get good attr 1 name");
        fail_unless(strcmp(val, "AAAA") == 0, "Failed to get good attr 1 val");
        fail_unless(end == 0, "attr 1 test: end was not detected correctly");
        fail_unless(ptr == attr_good_1 + 12, "attr 1 tests: ptr not in right place");
        

        ptr = get_tag_attr(attr_good_2, name, val, &end);
        fail_unless(strcmp(name, "NNNN") == 0, "Failed to get good attr 2 name");
        fail_unless(strcmp(val, "lior") == 0, "Failed to get good attr 2 val");
        fail_unless(end == 0, "attr 2 test: end was not detected correctly");
        fail_unless(ptr == attr_good_2 + 12, "attr 2 tests: ptr not in right place");

        // Getting end
        ptr = get_tag_attr(attr_good_3, name, val, &end);
        fail_unless(end == 1, "attr 3 test: end was not detected correctly");
        fail_unless(ptr == attr_good_3 + 1 , "attr 3 tests: ptr not in right place");
        
        ptr = get_tag_attr(attr_good_4, name, val, &end);
        fail_unless(strcmp(name, "attr") == 0, "Failed to get good attr 4 name");
        fail_unless(strcmp(val, "attrVal") == 0, "Failed to get good attr 4 val");
        fail_unless(end == 0, "attr 4 test: end was not detected correctly");
        fail_unless(ptr == attr_good_4 + 12, "attr 4 tests: ptr not in right place");

        // No space after value
        ptr = get_tag_attr(attr_good_5, name, val, &end);
        fail_unless(strcmp(name, "attr") == 0, "Failed to get good attr 5 name");
        fail_unless(strcmp(val, "attrVal") == 0, "Failed to get good attr 5 val");
        fail_unless(end == 0, "attr 5 test: end was not detected correctly");
        fail_unless(ptr == attr_good_5 + 12, "attr 5 tests: ptr not in right place");
        
        ptr = get_tag_attr(ptr, name, val, &end);
        fail_unless(end == 1, "attr 5+ptr test: end was not detected correctly");

        // Value with quotes
        ptr = get_tag_attr(attr_good_6, name, val, &end);
        fail_unless(strcmp(name, "attr") == 0, "Failed to get good attr 6 name");
        fail_unless(strcmp(val, "no process") == 0, "Failed to get good attr 6 val");
        fail_unless(end == 0, "attr 6 test: end was not detected correctly");
        
        
}
END_TEST


char *attr_bad_1 = " name AAA attr attrVal ";
char *attr_bad_2 = " name /> ";
char *attr_bad_3 = " name=/> ";

START_TEST (test_bad_get_tag_attr)
{
        char *ptr;
        char name[256];
        char val[256];
        int  end;
        
        ptr = get_tag_attr(attr_bad_1, name, val, &end);
        fail_unless(ptr == NULL, "bad attr 1: should fail\n");

        ptr = get_tag_attr(attr_bad_2, name, val, &end);
        fail_unless(ptr == NULL, "bad attr 2: should fail\n");

        ptr = get_tag_attr(attr_bad_3, name, val, &end);
        fail_unless(ptr == NULL, "bad attr 3: should fail\n");

        

}
END_TEST


char *tag_end_good_1 = " />";
char *tag_end_good_2 = " / > ";
char *tag_end_good_3 = " > ";

char *tag_end_bad_1 = " / ";
char *tag_end_bad_2 = " ddd";

START_TEST (test_get_tag_end)
{
        char *ptr;
        int  short_tag;

        // Good end
        ptr = get_tag_end(tag_end_good_1, &short_tag);
        fail_unless(ptr != NULL && short_tag == 1, "Faild getting end tag 1");
        
        ptr = get_tag_end(tag_end_good_2, &short_tag);
        fail_unless(ptr != NULL && short_tag == 1, "Faild getting end tag 2");

        ptr = get_tag_end(tag_end_good_3, &short_tag);
        fail_unless(ptr != NULL && short_tag == 0, "Faild getting end tag 3");
        
        // Bad end
        ptr = get_tag_end(tag_end_bad_1, &short_tag);
        fail_unless(ptr == NULL, "Bad end 1 should fail");
        
        ptr = get_tag_end(tag_end_bad_2, &short_tag);
        fail_unless(ptr == NULL, "Bad end 2 should fail");
        
        
}
END_TEST

char *xml_str_1 = "<proc name=infod pid=1121 status=\"no pid file\" />";

START_TEST (test_parse_short_xml)
{
        char *ptr;
        char tagName[256];
        char name[256];
        char val[256];
        int  close_tag, end, short_tag;

        ptr = xml_str_1;
        ptr = get_tag_start(ptr, tagName, &close_tag);
        fail_unless(ptr != NULL, "Failed getting xml1 tag");
        fail_unless(strcmp(tagName, "proc") == 0, "Failed getting xml1 tag name");
        do {
                ptr = get_tag_attr(ptr, name, val, &end);
                fail_unless(ptr != NULL, "Faild getting xml1 attr");
        } while(end == 0);
        
        ptr = get_tag_end(ptr, &short_tag);
        fail_unless(ptr != NULL, "Faild getting xml1 tag end");
        fail_unless(short_tag == 1, "xml 1 should be a short tag");
}
END_TEST



char *xml_str_long_1 = "<infod-debug nchild=\"5\">4560</infod-debug>";

START_TEST (test_parse_long_xml)
{
        char *ptr;
        char tagName[256];
        char name[256];
        char val[256];
        int  close_tag, end, short_tag;

        ptr = xml_str_long_1;
        ptr = get_tag_start(ptr, tagName, &close_tag);
        fail_unless(ptr != NULL, "Failed getting xml long 1 tag");
        fail_unless(strcmp(tagName, "infod-debug") == 0, "Failed getting xml long 1 tag name");
        do {
                ptr = get_tag_attr(ptr, name, val, &end);
                fail_unless(ptr != NULL, "Faild getting xml1 attr");
        } while(end == 0);
        ptr = get_tag_end(ptr, &short_tag);
        fail_unless(ptr != NULL, "Faild getting xml1 tag end");
        fail_unless(short_tag == 0, "xml 1 should NOT be a short tag");
        
        ptr = get_upto_tag_start(ptr, name);
        fail_unless(ptr != NULL, "Failed to get upto tag start");
        fail_unless(strcmp(name, "4560") == 0, "Content of tag is wrong");
        ptr = get_tag_start(ptr, tagName, &close_tag);
        fail_unless(ptr != NULL, "Failed getting xml long 1 tag close");
        fail_unless(close_tag, "Tag should be a closing tag");

        ptr = get_tag_end(ptr, &short_tag);
        fail_unless(ptr != NULL, "Faild getting xml1 long tag end");
        fail_unless(short_tag == 0, "xml 1 long should NOT be a short tag");
        

}
END_TEST


/*************************************************************/
Suite *mapper_suite(void)
{
  Suite *s = suite_create("ParserHelper");
  TCase *tc_core = tcase_create("Core");
  TCase *tc_xml = tcase_create("xml");

  
  suite_add_tcase (s, tc_core);
  suite_add_tcase (s, tc_xml);

  tcase_add_test(tc_core, test_split_str);
  tcase_add_test(tc_core, test_split_single_str);
  tcase_add_test(tc_core, test_split_empty_str);
  tcase_add_test(tc_core, test_split_large_str);

  tcase_add_test(tc_xml, test_get_tag_start);
  tcase_add_test(tc_xml, test_get_tag_attr);
  tcase_add_test(tc_xml, test_bad_get_tag_attr);
  tcase_add_test(tc_xml, test_get_tag_end);
  tcase_add_test(tc_xml, test_parse_short_xml);
  tcase_add_test(tc_xml, test_parse_long_xml);
  
  return s;
}

int main(void)
        {
  int nf;
  
  Suite *s = mapper_suite();
  SRunner *sr = srunner_create(s);
  
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
