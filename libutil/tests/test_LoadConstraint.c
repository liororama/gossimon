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
#include <unistd.h>
#include <string.h>


#include <LoadConstraints.h>


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



START_TEST (test_lc_basic)
{
	loadConstraint_t lc;
	
	lc = lcNew();
	fail_unless(lc != NULL, "Failed to create lc object");

	int size = lcHostNum(lc);
	fail_unless(size == 0 , "Number of hosts in new object != 0");
	
	lcDestroy(lc);
}
END_TEST

char *testHostList[] = {"mos1", "mos2", "mos7", "mos8", "mos11"};

int createLC(loadConstraint_t  lc, char **nodesArrArg, int nodes,
	     int itemsPerNode, int timeout)
{
	nodeLCInfo_t      ni;
	int               i, j;
	struct  timeval   duration;

	ni.minMemory = 10;
	ni.minProcesses = 1;

	char **nodesArr;
	if(!nodesArrArg) {
		
		char   smallBuff[50];
		nodesArr = malloc(nodes * sizeof(char *));
		for(i=1 ; i<=nodes ; i++) {
			sprintf(smallBuff, "mos%d", i);
			nodesArr[i-1] = strdup(smallBuff);
		}
	} else {
		nodesArr = nodesArrArg;
	}
	
	for(j=0 ; j<itemsPerNode ; j++) {
		
		for(i=0 ; i< nodes ; i++) {
			
			if(timeout > 0) {
				duration.tv_sec = 5;
				duration.tv_usec = 0;
				lcAddTimeConstraint(lc, nodesArr[i], NULL, &duration, &ni); 
			}
			else {
				int pid = getpid();
				lcAddControlledConstraint(lc, nodesArr[i], pid, &ni); 
			}
		}
	}
	return 1;
}


START_TEST (test_lc_add_time)
{
	loadConstraint_t  lc;

	lc = lcNew();
	fail_unless(lc != NULL, "Failed to create lc object");
	
	createLC(lc, testHostList, 5, 2, 5);
	
	int size = lcHostNum(lc);
	fail_unless(size == 5 , "Size is not 5");
	
	//lcPrintf(lc);
	lcDestroy(lc);
}
END_TEST


START_TEST (test_lcAddControlledConstraint)
{
	loadConstraint_t  lc;
	lc = lcNew();
	fail_unless(lc != NULL, "Failed to create lc object");

	createLC(lc, testHostList, 5, 2, 0);

	int size = lcHostNum(lc);
	fail_unless(size == 5 , "Size is not 5");
	
	//lcPrintf(lc);
	lcDestroy(lc);
}
END_TEST


START_TEST (test_lcGetNodeLC)
{
	loadConstraint_t  lc;
	nodeLCInfo_t      ni;
	
	lc = lcNew();
	fail_unless(lc != NULL, "Failed to create lc object");

	createLC(lc, testHostList, 5, 2, 5);
	
	int size = lcHostNum(lc);
	fail_unless(size == 5 , "Size is not 5");
	lcGetNodeLC(lc, "mos2", &ni);
	fail_unless(ni.minProcesses == 2, "Processes minumum was not aggregated correctly");
	fail_unless(ni.minMemory == 20, "Memory minumum was not aggregated correctly");

	lcDestroy(lc);
}
END_TEST



START_TEST (test_lcVerify)
{
	loadConstraint_t  lc;
	nodeLCInfo_t      ni;
	int               i, j;
	struct  timeval   start;
	struct  timeval   duration;
	lc = lcNew();
	fail_unless(lc != NULL, "Failed to create lc object");

	gettimeofday(&start, NULL);
	start.tv_sec -= 10;

	// All constraintes are obsolete 
	for(j=0 ; j<2 ; j++) {
		for(i=0 ; i<5 ; i++) {
			duration.tv_sec = 5;
			duration.tv_usec = 0;
			ni.minMemory = 10;
			ni.minProcesses = 1;
			
			lcAddTimeConstraint(lc, testHostList[i], &start, &duration, &ni); 
		}
	}
	int size = lcHostNum(lc);
	fail_unless(size == 5 , "Size is not 5");

	lcVerify(lc);
	//lcPrintf(lc);
	
	size = lcHostNum(lc);
	fail_unless(size == 0 , "Size is not 0 after verify");
	
	lcDestroy(lc);
}
END_TEST


START_TEST (test_lcVerifyControlled)
{
	loadConstraint_t  lc;
	nodeLCInfo_t      ni;
	int               i;
	int               goodPid = getpid();
	int               badPid = -1;

	lc = lcNew();
	fail_unless(lc != NULL, "Failed to create lc object");

	// Adding constraints with good pid
	for(i=0 ; i<5 ; i++) {
		ni.minMemory = 10;
		ni.minProcesses = 1;
		lcAddControlledConstraint(lc, testHostList[i], goodPid, &ni); 
	}

	// Adding constraints with Bad pid
	for(i=0 ; i<5 ; i++) {
		ni.minMemory = 10;
		ni.minProcesses = 1;
		lcAddControlledConstraint(lc, testHostList[i], badPid, &ni); 
	}
	int size = lcHostNum(lc);
	fail_unless(size == 5 , "Size is not 5");

	//lcPrintf(lc);
	lcVerify(lc);
	//lcPrintf(lc);
	
	size = lcHostNum(lc);
	fail_unless(size == 5 , "Size is not 5 after verify");
	
	lcGetNodeLC(lc, "mos2", &ni);
	fail_unless(ni.minProcesses == 1, "Processes minumum was not aggregated correctly");
	fail_unless(ni.minMemory == 10, "Memory minumum was not aggregated correctly");
	
	//lcPrintf(lc);
	lcDestroy(lc);
}
END_TEST


char writeBuff[4096];

START_TEST (test_writeLC)
{
	loadConstraint_t  lc;
	int               res;

	lc = lcNew();
	fail_unless(lc != NULL, "Failed to create lc object");

	createLC(lc, NULL, 10, 3, 5);
	
	int size = lcHostNum(lc);
	fail_unless(size == 10 , "Size is not 5");
	
	res = writeLCAsBinary(lc, writeBuff, 4096);
	fail_unless(res != 0, "Failed to write lc to buffer");

	res = writeLCAsBinary(lc, writeBuff, 100);
	fail_unless(res == 0, "Writing lc to a small buffer did not fail");
	
	
	lcDestroy(lc);
}
END_TEST


START_TEST (test_buildLC)
{
	loadConstraint_t  lc, lc2;
	int               res;

	lc = lcNew();
	fail_unless(lc != NULL, "Failed to create lc object");

	createLC(lc, NULL, 10, 3, 5);
	
	int size = lcHostNum(lc);
	fail_unless(size == 10 , "Size is not 5");
	
	res = writeLCAsBinary(lc, writeBuff, 4096);
	fail_unless(res !=0 , "Failed to write lc to buffer");

	lc2 = buildLCFromBinary(writeBuff, res);
	fail_unless(lc2 != NULL, "Faild reading lc from buffer");

	int size2 = lcHostNum(lc2);
	fail_unless(size == size2, "Size of read lc not equal to orig size");
	
	lcDestroy(lc);
	lcDestroy(lc2);
}
END_TEST


START_TEST (test_stressLC)
{
	loadConstraint_t  lc, lc2;
	int               res;
	int               i;
	
	char *bigWriteBuff = malloc(1024*1024);

	for(i=0 ; i< 10 ; i++ ) {
		lc = lcNew();
		fail_unless(lc != NULL, "Failed to create lc object");
		
		createLC(lc, NULL, 500, 10, 5);
		
		int size = lcHostNum(lc);
		fail_unless(size == 500 , "Size is not 5");
		
		res = writeLCAsBinary(lc, bigWriteBuff, 1024*1024);
		fail_unless(res != 0, "Failed to write lc to buffer");

		lc2 = buildLCFromBinary(bigWriteBuff, res);
		fail_unless(lc2 != NULL, "Faild reading lc from buffer");
		
		int size2 = lcHostNum(lc2);
		fail_unless(size == size2, "Size of read lc not equal to orig size");
		
		lcDestroy(lc);
		lcDestroy(lc2);
	}
}
END_TEST


/*************************************************************/
Suite *mapper_suite(void)
{
	Suite *s = suite_create("LoadConstraint");
	TCase *tc_core = tcase_create("Core");
	
	suite_add_tcase (s, tc_core);
	
	tcase_add_test(tc_core, test_lc_basic);
	tcase_add_test(tc_core, test_lc_add_time);
	tcase_add_test(tc_core, test_lcGetNodeLC);
	tcase_add_test(tc_core, test_lcAddControlledConstraint);	
	tcase_add_test(tc_core, test_lcVerify);
	tcase_add_test(tc_core, test_lcVerifyControlled);
	tcase_add_test(tc_core, test_writeLC);
	tcase_add_test(tc_core, test_buildLC);
	tcase_add_test(tc_core, test_stressLC);
	
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
