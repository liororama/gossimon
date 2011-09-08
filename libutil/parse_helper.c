/*============================================================================
  gossimon - Gossip based resource usage monitoring for Linux clusters
  Copyright 2003-2010 Amnon Barak

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/

/******************************************************************************
 *
 * Author(s): Amar Lior
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <parse_helper.h>



char *eat_spaces(char *str) {
	while(isspace(*str))
		str++;
	return str;
}

void trim_spaces(char *str)
{
	int end = strlen(str) -1;

	while(isspace(str[end]))
		str[end--] = '\0';
}

int split_str(char *str, char **output, int outsize, const char *seperator)
{
        char *str_copy;
	char *tmp, *tmp_str;
	int pos = 0;

        
	if(outsize <1)
		return 0;
	if(outsize == 1) {
		output[0] = NULL;
		return 0;
	}
	
	if(strlen(str) == 0) {
		output[0] = NULL;
		return 0;
	}
        
        str_copy = strdup(str);
        if(!str_copy)
                return 0;
        tmp_str = str_copy;
        
	while((tmp = strsep(&tmp_str, seperator)) && (pos < outsize -1))
	{
                if(strlen(tmp) == 0)
                        continue;
                output[pos++] = strdup(tmp);
	}
	output[pos] = NULL;
        free(str_copy);
        return pos;
}

/* Split a string to multiple string using a seperator */
int split_str_alloc(char *str, char ***resArr, const char *seperator)
{
    int itemsFound = 0;
    char *strCopy = strdup(str);
    char *tmpStr = strCopy;

    //fprintf(stderr, "Starting split2\n[%s]\n[%s]\n", str, seperator);
    int index = 0;
    while (tmpStr) {
        char *tmp = strsep(&tmpStr, seperator);
        if (!tmp || *tmp == '\0') break;
        itemsFound++;
        while (tmpStr && *tmpStr != '\0' && strchr(seperator, *tmpStr))
            tmpStr++;
        if (!tmpStr || *tmpStr == '\0') break;
    }

    *resArr = malloc(sizeof (char*) * (itemsFound + 1));

    strcpy(strCopy, str);
    tmpStr = strCopy;

    index = 0;
    while (tmpStr) {
        char *tmp = strsep(&tmpStr, seperator);
        if (!tmp || *tmp == '\0') break;

        (*resArr)[index++] = strdup(tmp);

        while (tmpStr && *tmpStr != '\0' && strchr(seperator, *tmpStr))
            tmpStr++;
        if (!tmpStr || *tmpStr == '\0') break;

    }

    free(strCopy);
    return itemsFound;
}


// Coying the first line from data to buff up to buff len characters.
// If no line after buff_len characters than we force a line.
// We assume that line seperator is "\n" and that data is null terminated
// We return a pointer to the next characters after the first newline we detect
// in data.
char *buff_get_line(char *buff, int buff_len, char *data)
{
    int    size_to_copy;
    char  *ptr;
    int    curr_line_size = 0;
    
    if(buff_len < 1)
	return NULL;

    // Skeeping spaces at the start
    while(isspace(*data)) {
	    data++;
	    curr_line_size++;
	    if(curr_line_size >= buff_len)
		    return NULL;
    }
    
    ptr = index(data, '\n');
    if(!ptr)
    {
	// we assume that data is one line and we copy it to buff
	strncpy(buff,  data, buff_len -1);
	buff[buff_len - 1] = '\0';
	return NULL;
    }

    // we found a line and we copy it
        
    size_to_copy = ptr - data;
    if(size_to_copy >= buff_len)
	size_to_copy = buff_len -1 ;
    memcpy(buff, data, size_to_copy);
    buff[size_to_copy] = '\0';
    return (++ptr);
}

char *get_upto_tag_start(char *str, char *upto_str)
{
        char *ptr = str;
        
        while(*ptr != '\0' && *ptr != '<') {
                ptr++;
        }
        // Could not find the tag
        if(*ptr == '\0') 
                return NULL;
        int upto_len = ptr - str;
        strncpy(upto_str, str, upto_len);
        upto_str[upto_len] = '\0';
        return ptr;
}

char *get_tag_start(char *str, char *tag_name, int *close_tag)
{
        char *ptr;
        char  buff[128];

        ptr = str;
        ptr = eat_spaces(ptr);

        if(*ptr != '<' ) {
                return NULL;
        }
        
        ptr++;
        ptr = eat_spaces(ptr);
        if(*ptr == '/') {

                *close_tag = 1;
                ptr++;
                ptr = eat_spaces(ptr);
        }
        else {
                *close_tag = 0;
        }
        char *tagNameStart = ptr;
        while(*ptr != '>' && *ptr != '\0' && !isspace(*ptr))
                ptr ++;

        if(*ptr == '\0')
                return NULL;

        strncpy(buff, tagNameStart, ptr-tagNameStart);
        buff[ptr - tagNameStart] = '\0';
        strcpy(tag_name, buff);

        return ptr;
}

char *get_tag_attr(char *str, char *attr_name, char *attr_val, int *end)
{
        char  attrBuff[256];
        char  valBuff[256];
        char *ptr = str;
        // Should be inside a tag
        
        ptr = eat_spaces(ptr);

        
        if( *ptr == '/' || *ptr == '>' ) {
                *end = 1;
                return ptr;
        }

        // Looking for the = sign
        char *nameStart = ptr;
        
        while(*ptr != '/' && *ptr != '>' && *ptr != '=' && ptr != '\0')
                ptr++;

        if(*ptr == '/' || *ptr == '>') {
                *end = 1;
                return NULL;
        }
        if(*ptr == '\0') {
                *end = 0;
                return NULL;
        }
        // Getting the name 
        strncpy(attrBuff, nameStart, ptr - nameStart);
        attrBuff[ptr - nameStart] = '\0';
        trim_spaces(attrBuff);
        strcpy(attr_name, attrBuff);

        ptr++;

        // Lokking for the value
        ptr = eat_spaces(ptr);
        int  quoteMode = 0;
        if(*ptr == '"') {
                quoteMode = 1;
                ptr++;
        }
        
        char *valStart = ptr;
        while(*ptr != '/' && *ptr != '>' &&
              ((quoteMode && *ptr != '"') || (!quoteMode && !isspace(*ptr))) &&
              ptr != '\0')
                ptr++;
        
        if(*ptr == '\0') {
                *end = 0;
                return NULL;
        }
        
        if(quoteMode) {
                if( *ptr != '"') {
                        *end = 0;
                        return NULL;
                }
        } 
              
        // Getting the value 
        strncpy(valBuff, valStart, ptr - valStart);
        valBuff[ptr - valStart] = '\0';

        if(!quoteMode) {
                trim_spaces(valBuff);
                if(strlen(valBuff) == 0)
                        return NULL;
        }
        
        strcpy(attr_val, valBuff);
        

        if(quoteMode) ptr++;
        *end = 0;
        return ptr;
}

// Get past the tag end / >
char *get_tag_end(char *str, int *short_tag) {
        char *ptr = str;

        *short_tag = 0;
        ptr = eat_spaces(ptr);
        if(*ptr == '/') {
                *short_tag = 1;
                ptr++;
                ptr = eat_spaces(ptr);
                if(*ptr != '>')
                        return NULL;
                return ++ptr;
        }
        else if(*ptr == '>') {
                return ++ptr;
        }
        else
                return NULL;
}

