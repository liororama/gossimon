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

#ifndef __MSX_EASY_ARGS
#define __MSX_EASY_ARGS


#define KILOBYTE	1024
#define MEGABYTE	(KILOBYTE*KILOBYTE)
#define GIGABYTE	(KILOBYTE*MEGABYTE)

/* this is the argument type that ends the argument list */
#define ARGUMENT_END_LIST		0x0000
/* these are the arguments specification: short is a character 
 * notation. full is a -- notation. at least one should be on */
#define ARGUMENT_FULL			0x0001
#define ARGUMENT_SHORT			0x0002
#define ARGUMENT_BOTH			0x0003
#define ARGUMENT_UNNAMED		0x0004
/* there are the argument types: flag is an argument on/off,
 * and the rest are with a parameter. at least one should be on */
#define ARGUMENT_FLAG			0x0008
#define ARGUMENT_STRING			0x0010
#define ARGUMENT_NUMERICAL		0x0020
#define ARGUMENT_BYTESIZE		0x0040
#define ARGUMENT_DOUBLE			0x0080
#define ARGUMENT_IP			0x0100
/* usage handler is a function the prints the usage */
#define ARGUMENT_USAGE_HANDLER		0x0200
/* this is a flag that if true, suggest argument is 
 * optional. in that case, a null pointer will be
 * transfered to the handler */
#define ARGUMENT_OPTIONAL		0x0400
#define ARGUMENT_VARIABLE		0x0800

#define ARGUMENT_TYPES			0x03f8
#define ARGUMENT_NEED_PARAMETER		0x01f0

#define MAX_OPTIONS			100
#define COMMAND_LINE_PARSING_ERROR_CODE	1
#define BASE_NUMBER_FOR_LONG_OPTIONS	400

typedef int (argument_handler)(void *);
struct argument {
	int			type;
	char			short_name;
	char			*long_name;
	void			*handler;	/* pointer to a handler function (with one pointer parameter) */
};						/* or pointer for the variable itself and ARGUMENT_VARIABLE */

extern char *easy_args_env_prefix;

int parse_command_line( int argc , char **argv , struct argument arguments [] );

#endif /* __MSX_EASY_ARGS */
