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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#include "host_list.h"
#include "cluster_list.h"

int cl_init(clusters_list_t *cl)
{
  if(!cl)
    return 0;

  cl->cl_ncluster = 0;
  cl->cl_curr = 0;
  cl->cl_list = NULL;
  return 1;
}


int cl_free(clusters_list_t *cl)
{
  if(!cl)
    return 0;

  if(cl->cl_list)
    {
      free(cl->cl_list);
      cl->cl_list = NULL;
    }
  cl->cl_ncluster = 0;
  cl->cl_curr = 0;
  return 1;
}

int cl_size(clusters_list_t *cl)
{
  if(!cl)
    return 0;
  return cl->cl_ncluster;
}


void cl_print(clusters_list_t *cl)
{
  int i;
	
  if(!cl)
    {
      printf("Cluster list is empty\n");
      return;
    }


  for(i=0 ; i<cl->cl_ncluster ; i++)
    {
      printf("Cluster %s\n", cl->cl_list[i].name);
      mh_print(&(cl->cl_list[i].host_list));
    }
}

// The str is leagal if it only contain , and machines name which can contain
// a-zA-Z0-9 - _
int is_cluster_str_legal(char *str)
{
  int i, len;

  len = strlen(str);
  for(i=0 ; i<len ; i++)
    {
      if(isalnum(str[i]))
        continue;
      if(str[i] == '-' || str[i] == ',' || str[i] == '_' ||
         str[i] == '.')
        continue;
      return 0;
    }
  return 1;
}

int is_valid_machine_name(char *name)
{
  int i, len;
	
  len = strlen(name);
  for(i=0 ; i<len ; i++)
    {
      if(isalnum(name[i]))
        continue;
      if(name[i] == '-' || name[i] == '_'|| name[i] =='.')
        continue;
      return 0;
    }

  // A range is not a valid name but mos1.cs.huji.ac.il is a valid name
  if(strstr(name, ".."))
    return 0;
	
  return 1;
}


int cl_set_from_str(clusters_list_t *cl, char *str)
{
  char *comma_delim = ",";
  char *tmp_str;
  char *token;
  int  cluster_num, cluster_pos;
	
  if(!is_cluster_str_legal(str))
    return(0);

  // We seperate the list to single machine names and we init the
  // host list with this name

  // First counting the clusters
  tmp_str = strdup(str);
  cluster_num = 0;
  while((token = strsep(&tmp_str, comma_delim)))
    {
      if(is_valid_machine_name(token))
        cluster_num++;
    }
  //printf("Got %d clusters\n", cluster_num);
	
  // Allocating memory for the cluster list;

  cl->cl_list = malloc(sizeof(cluster_entry_t) * cluster_num);
  if(!cl->cl_list)
    return 0;

  cl->cl_ncluster = cluster_num;
	
  tmp_str = strdup(str);
  cluster_pos = 0;
  while((token = strsep(&tmp_str, comma_delim)))
    {
      if(!is_valid_machine_name(token))
        continue;

      //printf("TOKEN: %s \n", token);
      mh_init(&(cl->cl_list[cluster_pos].host_list));
      if(!mh_set(&(cl->cl_list[cluster_pos].host_list), token))
        {
          //printf("Failed setting host list from: %s\n", token);
          goto failed;
        }
      strncpy(cl->cl_list[cluster_pos].name, token, MAX_CLUSTER_NAME);
      cluster_pos++;
    }
  return 1;
 failed:
  free(cl->cl_list);
  return 0;
	
}

/**************************************************************************
 * Reading the cluster list from a file in the following format:
 *
 *  # is a comment
 *    empty lines are ignored
 *  clustername = cluster_range
 **************************************************************************/

int cl_set_from_file(clusters_list_t *cl, char *fn)
{
  FILE *f;
  int  max_size = 100, pos;
  char buf[513];
  char cluster_name[50];
  char range_str[512];
	
	
	
  if(!(f = fopen(fn, "r")))
    return 0;

  cl->cl_list = malloc(sizeof(cluster_entry_t) * max_size);
  if(!cl->cl_list)
    return 0;

  pos = 0;
  while(fgets(buf, 512, f))
    if(buf[0] && buf[0] != '#' && buf[0] != '\n')
      {
        buf[512] = '\0';
        if(sscanf(buf, "%s = %s", cluster_name, range_str) == 2)
          {
            //printf("Got %s %s\n", cluster_name, range_str);
            mh_init(&(cl->cl_list[pos].host_list));
            if(!mh_set(&(cl->cl_list[pos].host_list), range_str))
              {
                //printf("Failed setting host list from: %s\n", range_str);
                goto failed;
              }
            strncpy(cl->cl_list[pos].name, cluster_name, MAX_CLUSTER_NAME);
            pos++;
          }
        else if (sscanf(buf, "%s", cluster_name) == 1)
          {
            //printf("Got single name %s\n", cluster_name);
            mh_init(&(cl->cl_list[pos].host_list));
            if(!mh_set(&(cl->cl_list[pos].host_list), cluster_name))
              {
                //printf("Failed setting host list from: %s\n", cluster_name);
                goto failed;
              }
            strncpy(cl->cl_list[pos].name, cluster_name, MAX_CLUSTER_NAME);
            pos++;
          }
        else {
          //printf("Unknownd format\n");
          return 0;
        }
      }

  cl->cl_ncluster = pos;
  return 1;
	
 failed:
  free(cl->cl_list);
  return 0;
}


int cl_set_single_cluster(clusters_list_t *cl, char *cluster_str)
{
  if(!cl)
    return 0;

  cl->cl_list = malloc(sizeof(cluster_entry_t));
  if(!cl->cl_list)
    return 0;

  cl->cl_ncluster = 1;
  cl->cl_curr = 0;
  mh_init(&(cl->cl_list[0].host_list));
  if(!mh_set(&(cl->cl_list[0].host_list), cluster_str))
    {
      //printf("Failed setting host list from: %s\n", cluster_str);
      goto failed;
    }
  strncpy(cl->cl_list[0].name, cluster_str, MAX_CLUSTER_NAME);
  return 1;
	
 failed:
  free(cl->cl_list);
  return 0;
}


cluster_entry_t *cl_curr(clusters_list_t *cl)
{
  if(!cl)
    return NULL;
	
  return &cl->cl_list[cl->cl_curr];
}

cluster_entry_t *cl_next(clusters_list_t *cl)
{
  if(!cl)
    return NULL;
	
  cl->cl_curr = (cl->cl_curr + 1) % cl->cl_ncluster;
  return &cl->cl_list[cl->cl_curr];
}
cluster_entry_t *cl_prev(clusters_list_t *cl)
{
  if(!cl)
    return NULL;
	
  if(cl->cl_curr == 0)
    cl->cl_curr = cl->cl_ncluster - 1;
  else 
    cl->cl_curr = (cl->cl_curr - 1) % cl->cl_ncluster;
  return &cl->cl_list[cl->cl_curr];
}

/* void test_set_from_file(char *fname) */
/* { */
/*   int i;	 */
/*   clusters_list_t cl; */

/*   cl_init(&cl); */
/*   if(!cl_set_from_file(&cl, fname)) */
/*     { */
/*       //printf("Error in set from file\n"); */
/*       exit(1); */
/*     } */
/*   for(i=0 ; i<cl_size(&cl) ; i++) */
/*     { */
/*       cluster_entry_t *ce; */
/*       ce = cl_next(&cl); */

/*       //printf("------------------->Next: %s\n", ce->name); */
/*       mh_print(&ce->host_list); */
/*     } */
		
/* } */

/* void test_set_from_str(char *str) */
/* { */
/*   int i; */
/*   clusters_list_t cl; */


/*   cl_init(&cl); */
/*   //printf("cluster list is:%s\n", str); */
/*   for(i=0 ; i<2*cl_size(&cl) ; i++) */
/*     { */
/*       cluster_entry_t *ce; */
/*       ce = cl_next(&cl); */
		
/*       //printf("------------------->Next: %s ", ce->name); */
/*       mh_print(&ce->host_list); */
/*     } */
	
/* } */


/* void test_single_cluster(char *str) */
/* { */
/*   int i; */
/*   clusters_list_t cl; */
/*   cluster_entry_t *ce; */
	
/*   cl_init(&cl); */
/*   if(!cl_set_single_cluster(&cl, str)) */
/*     { */
/*       //printf("Error in set single cluster from %s\n", str); */
/*       exit(1); */
/*     } */
/*   for(i=0 ; i<cl_size(&cl) ; i++) */
/*     { */
/*       ce = cl_next(&cl); */
/*       mh_print(&ce->host_list); */
/*     } */
/* } */

/* void test_prev_next(char *fname) */
/* { */
/*   char comm[] = {'n', 'p', 'n', 'n', 'n', 'p', 'p', 'p', 'e'}; */
/*   int i; */
/*   clusters_list_t cl; */
	
/*   cluster_entry_t *ce; */
/*   cl_init(&cl); */
/*   if(!cl_set_from_file(&cl, fname)) */
/*     { */
/*       //printf("Error in set from file\n"); */
/*       exit(1); */
/*     } */

/*   i=0; */
/*   while(comm[i] != 'e') */
/*     { */
/*       if(comm[i] == 'n') */
/*         { */
/*           ce = cl_next(&cl); */
/*           //printf("Next: %s\n", ce->name); */
/*         } */
/*       else if (comm[i] == 'p') */
/*         { */
/*           ce = cl_prev(&cl); */
/*           //printf("Prev: %s\n", ce->name); */
/*         } */
/*       i++; */
/*     } */
/* } */
/* #if 0 */
/* int main(int argc, char **argv) */
/* { */
/*   int i; */
/*   clusters_list_t cl; */
/*   mon_hosts_t *mh; */

/*   //test_set_from_file(argv[1]); */

/*   //test_prev_next(argv[1]); */
	
/*   test_single_cluster(argv[1]); */
	
	
/*   /\* */

/*   printf("\n\n\nChecking the prev:\n"); */
/*   for(i=0 ; i<2*cl_size(&cl) ; i++) */
/*   { */
/*   cluster_entry_t *ce; */
/*   ce = cl_prev(&cl); */

/*   printf("------------------->Prev: %s ", ce->name); */
/*   mh_print(&ce->host_list); */
/*   } */
/*   *\/ */
/*   return 0; */
/* } */
/* #endif  */
