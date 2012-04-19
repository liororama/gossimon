/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/


#ifndef __PARSE_HELPER__
#define __PARSE_HELPER__

#ifdef  __cplusplus
extern "C" {
#endif


char *eat_spaces(char *str);
void trim_spaces(char *str);
int split_str(char *str, char **output, int outsize, const char *seperator);
int split_str_alloc(char *str, char ***output, const char *seperator);

// Interface of internal building functions that will be used by the different builders
char *buff_get_line(char *buff, int buff_len, char *data);


// Simple XML parsing
char *get_upto_tag_start(char *str, char *upto_str);
char *get_tag_start(char *str, char *tag_name, int *close_tag);
char *get_tag_attr(char *str, char *attr_name, char *attr_val, int *end);
char *get_tag_end(char *str, int *short_tag);


#ifdef  __cplusplus
}
#endif


#endif /* __UMOSIX_PARSE_HELPER__ */
