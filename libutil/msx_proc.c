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


#include "msx_proc.h"

int
msx_is_mosix() 
{
	return(!access("/proc/mosix/", 0));
}

int
msx_is_configured() 
{
	return(msx_read("/proc/mosix/admin/mospe") == -1 ? 0: 1);
}
 
int
msx_readval(char *fn, int *val)
{
	int fd = open(fn, 0);
	int r;
	char num[30];

	if(fd == -1)
		return(0);
	num[sizeof(num)-1] = '\0';
	r = read(fd, num, sizeof(num)-1);
	close(fd);
	return(r >= 0 && sscanf(num, "%d", val) == 1);
}

int
msx_readval2(char *fn, int *val1, int *val2)
{
	int fd = open(fn, 0);
	int r;
	char num[30];

	if(fd == -1)
		return(0);
	num[sizeof(num)-1] = '\0';
	r = read(fd, num, sizeof(num)-1);
	close(fd);
	if(r < 0)
		return(0);
	r = sscanf(num, "%d %d", val1, val2);
	if(r == 1)
		*val2 = 0;
	else if(r != 2)
		return(0);
	return(1);
}

int
msx_write(char *fn, int val)
{
	int fd = open(fn, 1);
	int l, r;
	char num[30];

	if(fd == -1)
		return(0);
	sprintf(num, "%d\n", val);
	r = write(fd, num, l = strlen(num));
	close(fd);
	return(r == l);
}

int
msx_write2(char *fn, int val1, int val2)
{
	int fd = open(fn, 1);
	int l, r;
	char num[30];

	if(fd == -1)
		return(0);
	sprintf(num, "%d %d\n", val1, val2);
	r = write(fd, num, l = strlen(num));
	close(fd);
	return(r == l);
}

int
msx_replace(char *fn, int val)
{
	int fd = open(fn, 2);
	int l, r;
	char num[30];
	int ret;

	if(fd == -1)
		return(0);
	num[sizeof(num)-1] = '\0';
	r = read(fd, num, sizeof(num)-1);
	if(r < 0 || sscanf(num, "%d", &ret) != 1)
		return(-1);
	sprintf(num, "%d\n", val);
	r = write(fd, num, l = strlen(num));
	close(fd);
	return(r == l ? ret : -1);
}

int
msx_read(char *fn)
{
	int val;

	return(msx_readval(fn, &val) ? val : -1);
}

int
msx_readnode(int node, char *item)
{
	char fn[40];
	int val;

	if(!node && !(node = msx_read("/proc/mosix/admin/mospe")))
		return(-1);
	sprintf(fn, "/proc/mosix/nodes/%d/%s", node, item);
	if(!msx_readval(fn, &val))
		return(-1);
	if(val >= 0)
		return(val);
	errno = -val;
	return(-1);
}

long long
msx_readnodemem(int node, char *item)
{
	int units;
	static int mult;
	FILE *ver;
	char line[100];
	int a, b, c;

	if((units = msx_readnode(node, item)) == -1)
		return(-1);
	result:
	if(mult)
		return(units * (long long)mult);
	mult = 4096;
	ver = fopen("/proc/mosix/admin/version", "r");
	if(!ver)
		goto result;
	if(!fgets(line, sizeof(line), ver))
		goto close;
	if(sscanf(line, "Mosix Version %d.%d.%d", &a, &b, &c) == 3 &&
		(a <= 1 || (a == 1 && b < 6)))
	{
		fclose(ver);
		if(!(ver = fopen("/proc/version", "r")))
			goto result;
		if(!fgets(line, sizeof(line), ver))
			goto close;
		if(sscanf(line, "Linux version %d.%d.%d", &a, &b, &c) == 3 &&
			(a < 2 || (a == 2 && b < 4) ||
			(a == 2 && b == 4 && c < 18)))
			mult = 1;
	}
	close:
	fclose(ver);
	goto result;
}

int
msx_writeproc(int pid, char *item, int val)
{
	char fn[30];

	sprintf(fn, "/proc/%d/%s", pid, item);
	return(msx_write(fn, val));
}

int
msx_readproc(int pid, char *item)
{
	char fn[30];

	sprintf(fn, "/proc/%d/%s", pid, item);
	return(msx_read(fn));
}

int
msx_readdata(char *fn, void *into, int max, int size)
{
	int fd = open(fn, 0);
	int r;

	if(fd == -1)
		return(0);
	r = read(fd, into, max * size);
	close(fd);
	if(r == -1 || (r % size))
		return(0);
	return(r / size);
}

int
msx_count_ints(char *fn)
{
	int fd = open(fn, 0);
	int r;
	char buf[4096];
	char *p = buf, *np;
	int n = 0;

	if(fd == -1)
		return(0);
	r = read(fd, buf, sizeof(buf));
	close(fd);
	if(r < 0)
		return(-1);
	if(r == sizeof(buf))
	{
		errno = E2BIG;
		return(-1);
	}
	buf[r] = '\0';
	for(p = buf ; strtol(p, &np, 0) , np != p ; p = np)
		n++;
	return(n);
}

int
msx_fill_ints(char *fn, int *into, int max)
{
	int fd = open(fn, 0);
	int r;
	char buf[4096];
	char *p = buf, *np;
	int n = 0;

	if(fd == -1)
		return(0);
	r = read(fd, buf, sizeof(buf));
	close(fd);
	if(r < 0)
		return(-1);
	if(r == sizeof(buf))
	{
		errno = E2BIG;
		return(-1);
	}
	buf[r] = '\0';
	for(p = buf ; n < max && (into[n] = strtol(p, &np, 0) , np != p) ; p=np)
		n++;
	return(n);
}


int msx_readstr(char *fn, char *buff, int *size) 
{
	int fd = open(fn, 0);
	int r;

	if(fd == -1)
		return(0);

	r = read(fd, buff, *size-1);
	close(fd);
	if(r == -1)
		return 0;
	buff[*size -1] = '\0';
	*size = r;
	return 1;
}

