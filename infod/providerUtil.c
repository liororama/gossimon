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


#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <math.h>
#include <sys/utsname.h>

#include <msx_debug.h>
#include <msx_error.h>
#include <parse_helper.h>
#include <provider.h>

int proc_file_buff_size = 0;
char *proc_file_buff = NULL;


// Coying the first line from data to buff up to buff len characters.
// If no line after buff_len characters than we force a line.
// We assume that line seperator is "\n" and that data is null terminated
// We return a pointer to the next characters after the first newline we detect
// in data.

char *sgets(char *buff, int buff_len, char *data) {
    int size_to_copy;
    char *ptr;

    if (buff_len < 1)
        return NULL;

    // Skipping spaces at the start
    while (isspace(*data))
        data++;

    ptr = index(data, '\n');
    if (!ptr) {
        // we assume that data is one line and we copy it to buff
        strncpy(buff, data, buff_len - 1);
        buff[buff_len - 1] = '\0';
        return NULL;
    }

    // we found a line and we copy it
    size_to_copy = ptr - data;
    if (size_to_copy >= buff_len)
        size_to_copy = buff_len - 1;
    memcpy(buff, data, size_to_copy);
    buff[size_to_copy] = '\0';
    return (++ptr);
}

int get_disk_io_info_24(disk_io_info_t *dio) {
    int res;
    char line[512];
    char *buff_ptr;
    char *line_ptr, *line_ptr2;


    if (!(res = read_proc_file("/proc/stat")))
        return 0;

    dio->read_bytes = 0;
    dio->write_bytes = 0;
    proc_file_buff[res] = '\0';
    buff_ptr = proc_file_buff;
    do {
        unsigned int tmp, read_bytes, write_bytes;
        buff_ptr = sgets(line, 512, buff_ptr);

        if (strncmp(line, "disk_io: ", 8) != 0)
            continue;
        //printf("Found: %s\n", line);

        // the disk_io: line was found but there is no data
        if (!(line_ptr = strstr(line, ":(")))
            break;
        line_ptr += 2;
        do {
            //printf("Line ptr: %s\n", line_ptr);
            sscanf(line_ptr, "%u,%u,%u,%u,%u)",
                    &tmp, &tmp, &read_bytes, &tmp, &write_bytes);
            //printf("Got %u %u\n", read_bytes, write_bytes);
            dio->read_bytes += read_bytes;
            dio->write_bytes += write_bytes;
            line_ptr2 = strstr(line_ptr, ":(");
            if (line_ptr2)
                line_ptr2 += 2;

            line_ptr = line_ptr2;
        } while (line_ptr);
    } while (buff_ptr);

    dio->read_bytes *= 512;
    dio->write_bytes *= 512;
    return 1;
}

int get_disk_io_info_26(disk_io_info_t *dio) {
    int res;
    char line[512];
    char dev_name[64];
    char *buff_ptr;


    if (!(res = read_proc_file("/proc/diskstats")))
        return 0;

    dio->read_bytes = 0;
    dio->write_bytes = 0;
    proc_file_buff[res] = '\0';
    buff_ptr = proc_file_buff;
    do {
        unsigned int tmp;
        unsigned long long read_sect, write_sect;
        buff_ptr = sgets(line, 512, buff_ptr);
        if (sscanf(line, "%4d %4d %s %u %u %llu %u %u %u %llu %u %u %u %u",
                &tmp, &tmp, dev_name,
                &tmp, &tmp, &read_sect, &tmp,
                &tmp, &tmp, &write_sect, &tmp,
                &tmp, &tmp, &tmp) != 14)
            continue;

        if (strstr(dev_name, "loop") ||
                strstr(dev_name, "ram"))
            continue;
        //printf("Got dev %s %llu %llu\n", dev_name, read_sect, write_sect);
        dio->read_bytes += read_sect * 512;
        dio->write_bytes += write_sect * 512;
    } while (buff_ptr);
    return 1;
}

// Check if we are in 2.4 or 2.6 and call the aproptiate function

int get_disk_io_info(disk_io_info_t *dio) {
    static int version = 0;

    if (!version) {
        if (access("/proc/diskstats", R_OK) == 0)
            version = 6;
        else
            version = 4;
    }

    if (version == 4)
        return get_disk_io_info_24(dio);
    return get_disk_io_info_26(dio);
}

int get_net_io_info(net_io_info_t *nio) {
    int res;
    char line[256];
    char *buff_ptr;
    char *line_ptr;

    if (!(res = read_proc_file("/proc/net/dev")))
        return 0;

    nio->rx_bytes = 0;
    nio->tx_bytes = 0;

    proc_file_buff[res] = '\0';
    buff_ptr = proc_file_buff;
    do {
        unsigned int rx_bytes, tx_bytes, tmp;
        buff_ptr = sgets(line, 256, buff_ptr);
        if (!(line_ptr = strchr(line, ':')))
            continue;

        *line_ptr = '\0';
        line_ptr++;
        // Skeeping the loop device
        if (strcmp(line, "lo") == 0)
            continue;

        // We have a non local device and we take it
        sscanf(line_ptr, "%u %u %u %u %u %u %u %u %u",
                &rx_bytes,
                &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp,
                &tx_bytes);
        //printf(" %s: %u %u\n", line, rx_bytes, tx_bytes);
        nio->rx_bytes += rx_bytes;
        nio->tx_bytes += tx_bytes;
    } while (buff_ptr);
    return 1;
}

void calc_io_rates(disk_io_info_t *d_old, disk_io_info_t *d_new,
        net_io_info_t *n_old, net_io_info_t *n_new,
        struct timeval *t_old, struct timeval *t_new,
        io_rates_t *rates) {
    int milli_sec;
    double sec_float;
    double total_io;

    bzero(rates, sizeof (io_rates_t));
    if (!t_old || !t_new)
        return;

    milli_sec = (t_new->tv_sec - t_old->tv_sec) * 1000;
    milli_sec += (t_new->tv_usec - t_old->tv_usec) / 1000;
    sec_float = milli_sec;
    sec_float /= 1000;


    if (n_new && n_old) {
        total_io = n_new->rx_bytes - n_old->rx_bytes;
        total_io /= 1024; // to K
        total_io /= sec_float; // to rate (Kbytes/sec
        rates->n_rx_kbsec = total_io;

        total_io = n_new->tx_bytes - n_old->tx_bytes;
        total_io /= 1024; // to K
        total_io /= sec_float; // to rate (Kbytes/sec
        rates->n_tx_kbsec = total_io;
    }
    if (d_new && d_old) {
        total_io = d_new->read_bytes - d_old->read_bytes;
        total_io /= 1024; // to K
        total_io /= sec_float; // to rate (Kbytes/sec
        rates->d_read_kbsec = total_io;

        total_io = d_new->write_bytes - d_old->write_bytes;
        total_io /= 1024; // to K
        total_io /= sec_float; // to rate (Kbytes/sec
        rates->d_write_kbsec = total_io;
    }
}

int read_proc_file(char *file) {
    int res;
    int fd;

    // Initial allocation of proc_file buff (called only once)
    if (proc_file_buff == NULL) {
        proc_file_buff = malloc(PROC_BUFF_SZ);
        if (!proc_file_buff) {
            debug_r("Error: allocating memory for proc_file_buff\n");
            return 0;
        }
        proc_file_buff_size = 1024;
    }

    /* open the file holding cpu information */
    if ((fd = open(file, O_RDONLY)) < 0) {
        debug_r("Error: Opening file [%s]\n%s\n", file, strerror(errno));
        return 0;
    }

    char *ptr = proc_file_buff;
    int size_left = proc_file_buff_size;
    int data_size = 0;
    while ((res = read(fd, ptr, size_left)) > 0) {
        ptr += res;
        size_left -= res;
        data_size += res;

        // Buffer is full - trying to increase
        if (size_left == 0) {
            int new_size = proc_file_buff_size * 2;

            proc_file_buff = realloc(proc_file_buff, new_size);
            if (!proc_file_buff) {
                debug_r("Error: increasing proc buffer\n");
                return 0;
            }

            size_left = proc_file_buff_size;
            proc_file_buff_size = new_size;
            // Setting ptr again since realloc might change base address
            ptr = proc_file_buff + data_size;
        }
    }

    close(fd);
    if (res == 0) {
        return data_size;
    }
    if (res == -1) {
        debug_r("Error: Reading proc file [%s] \n%s\n", file, strerror(errno));
        return 0;
    }
    return 0;
}

/*****************************************************************************
 * Getting cpu information such as speed in MHz and number of cpus
 *****************************************************************************/
int get_cpu_info(cpu_info_t *cpu_info) {

    int res = 0;
    int i = 0;
    char *ptr1 = NULL, *ptr2 = NULL;
    //*buff = NULL;
    double speed;
    int int_speed;

    bzero(cpu_info, sizeof (cpu_info_t));

    if (!read_proc_file(CPU_INFO_PATH)) {
        res = 0;
        goto exit;
    }
    debug_g("CPU info file size %d\n", res);

    proc_file_buff[ res - 1 ] = '\0';

    /* Get the number of cpus and their speed */
    for (ptr1 = proc_file_buff; *ptr1; ptr1++, i++) {
        if (strncmp(ptr1,
                CPU_MHZ_STR, strlen(CPU_MHZ_STR)) == 0) {
            debug_g("Detected cpu\n");
            for (ptr2 = ptr1; *ptr2; ptr2++)
                if (isdigit(*ptr2))
                    break;

            /* found a digit */
            if (*ptr2) {
                sscanf(ptr2, "%lf", &speed);
                int_speed = trunc(speed);
                // Taking the fastest cpu since each cpu might have a different 
                // speed (depending on the power governor).
                if (int_speed > cpu_info->mhz_speed)
                    cpu_info->mhz_speed = trunc(speed);
            }
            cpu_info->ncpus++;
        }
    }

    debug_g("Detected %d cpus speed %d\n", cpu_info->ncpus, cpu_info->mhz_speed);
    res = 1;

exit:
    return res;
}

int get_linux_loadavg(linux_load_info_t *l) {
    int res;

    if (!(res = read_proc_file(LOADAVG_PATH)))
        return 0;

    proc_file_buff[ res - 1 ] = '\0';
    res = sscanf(proc_file_buff, "%f %f %f",
            &l->load[0], &l->load[1], &l->load[2]);
    if (res != 3) {
        // HMM strange load avg is in different formant
        debug_r("Error: Parsing loadavg file\n%s\n");
        res = 0;
        goto exit;
    }
    res = 1;
exit:
    return res;
}

int detect_linux_loadavg2(int *use_loadavg2) {
    debug_ly(KCOMM_DEBUG, "========= Detecting loadavg2 ==============\n");
    if (access(LOADAVG2_PATH, R_OK) == 0)
        *use_loadavg2 = 1;
    else
        *use_loadavg2 = 0;

    return 1;
}

// Reading the /proc/loadavg2 file and returning the obtained load
// At the first element of the linux_load_info_t struct (the first element of
// the load value array)

int get_linux_loadavg2(linux_load_info_t *l) {
    int res;

    if (!(res = read_proc_file(LOADAVG2_PATH)))
        return 0;

    proc_file_buff[ res - 1 ] = '\0';
    res = sscanf(proc_file_buff, "load: %f", &l->load[0]);
    if (res != 1) {
        // HMM strange load avg is in different formant
        debug_r("Error: Parsing loadavg2 file\n%s\n");
        res = 0;
        goto exit;
    }
    res = 1;
exit:
    return res;
}

/*****************************************************************************
 * Checking if this process is inside a virtaul machine. For now using
 * the file /etc/vm and if the file exists and the content is 1 then we
 * are running inside a vm
 *****************************************************************************/
int running_on_vm() {
    int res = 0, val = 0;

    if (!read_proc_file(RUNNING_ON_VM_PATH))
        return 0;

    proc_file_buff[ res - 1 ] = '\0';
    res = sscanf(proc_file_buff, "%d", &val);
    if (res != 1) {
        // HMM strange: running on vm file not in format
        debug_r("Error: Parsing loadavg file\n%s\n");
        goto exit;
    }
    if (val == 1)
        res = 1;
exit:
    debug_lg(KCOMM_DEBUG, "Running on vm: %d\n", val);
    return res;

}

int parse_rpc_line(nfs_client_info_t *nci, char *line) {
    unsigned long calls, retrans, authrefresh;
    int res;

    res = sscanf(line, "rpc %ld %ld %ld", &calls, &retrans, &authrefresh);
    if (res != 3)
        return 0;

    nci->rpc_ops = calls;
    return 1;
}

int parse_nfs2_line(nfs_client_info_t *nci, char *line) {
    unsigned long getattr, read, write, other;
    unsigned long var1, var2, var3, var4, var5, var6, var7, var8;
    unsigned long var9, var10, var11, var12, var13, var14, var15;
    unsigned long var16, var17, var18, var19;
    int res;

    res = sscanf(line,
            "proc2 %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
            "%ld %ld %ld %ld %ld %ld %ld %ld %ld",
            &var1, &var2, &var3, &var4, &var5, &var6,
            &var7, &var8, &var9, &var10, &var11, &var12, &var13,
            &var14, &var15, &var16, &var17, &var18, &var19);
    if (res != 19)
        return 0;

    getattr = var3;
    read = var8;
    write = var10;

    other = var2 + var4 + var5 + var6 + var7 + var9 + var11 + var12 +
            var13 + var14 + var15 + var16 + var17 + var19;

    nci->read_ops += read;
    nci->write_ops += write;
    nci->getattr_ops += getattr;
    nci->other_ops += other;
    return 1;
}

int parse_nfs3_line(nfs_client_info_t *nci, char *line) {
    unsigned long getattr, read, write, other;
    unsigned long var1, var2, var3, var4, var5, var6, var7, var8;
    unsigned long var9, var10, var11, var12, var13, var14, var15;
    unsigned long var16, var17, var18, var19;
    unsigned long var20, var21, var22, var23;
    int res;

    res = sscanf(line,
            "proc3 %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
            "%ld %ld %ld %ld %ld %ld %ld %ld %ld "
            "%ld %ld %ld %ld ",
            &var1, &var2, &var3, &var4, &var5, &var6,
            &var7, &var8, &var9, &var10, &var11, &var12, &var13,
            &var14, &var15, &var16, &var17, &var18, &var19,
            &var20, &var21, &var22, &var23);
    if (res != 23)
        return 0;

    getattr = var3;
    read = var8;
    write = var9;

    other = var2 + var4 + var5 + var6 + var7 + var10 + var11 + var12 +
            var13 + var14 + var15 + var16 + var17 + var19 +
            var20 + var21 + var22 + var23;

    nci->read_ops += read;
    nci->write_ops += write;
    nci->getattr_ops += getattr;
    nci->other_ops += other;
    return 1;
}

int get_nfs_client_info(nfs_client_info_t *nci) {

    int res;
    char line[256];
    char *buff_ptr;

    if (!(res = read_proc_file("/proc/net/rpc/nfs")))
        return 0;

    proc_file_buff[res] = '\0';
    buff_ptr = proc_file_buff;
    do {
        buff_ptr = sgets(line, 256, buff_ptr);

        if (strncmp(line, "rpc", 3) == 0) {
            parse_rpc_line(nci, line);
        } else if (strncmp(line, "proc2", 5) == 0) {
            parse_nfs2_line(nci, line);
        } else if (strncmp(line, "proc3", 5) == 0) {
            parse_nfs3_line(nci, line);
        }
    } while (buff_ptr);
    return 1;
}

int calc_nfs_client_rates(nfs_client_info_t *nci_old,
        nfs_client_info_t *nci_new,
        struct timeval *t_old, struct timeval *t_new,
        nfs_client_info_t *nci_rates) {

    int milli_sec;
    double sec_float;
    double ops;


    if (!t_old || !t_new)
        return 0;
    if (!nci_old || !nci_old)
        return 0;

    bzero(nci_rates, sizeof (nfs_client_info_t));
    milli_sec = (t_new->tv_sec - t_old->tv_sec) * 1000;
    milli_sec += (t_new->tv_usec - t_old->tv_usec) / 1000;
    sec_float = milli_sec;
    sec_float /= 1000;


    ops = nci_new->rpc_ops - nci_old->rpc_ops;
    ops /= sec_float; // to rate 
    nci_rates->rpc_ops = ops;

    ops = nci_new->read_ops - nci_old->read_ops;
    ops /= sec_float; // to rate 
    nci_rates->read_ops = ops;

    ops = nci_new->write_ops - nci_old->write_ops;
    ops /= sec_float; // to rate 
    nci_rates->write_ops = ops;

    ops = nci_new->getattr_ops - nci_old->getattr_ops;
    ops /= sec_float; // to rate 
    nci_rates->getattr_ops = ops;

    ops = nci_new->other_ops - nci_old->other_ops;
    ops /= sec_float; // to rate 
    nci_rates->other_ops = ops;

    return 1;
}

void free_freeze_dirs(freeze_info_t *fi) {
    if (!fi)
        return;

    for (int i = 0; i < fi->freeze_dirs_num; i++) {
        free(fi->freeze_dirs[i]);
        fi->freeze_dirs[i] = NULL;
    }
    fi->freeze_dirs_num = 0;
    fi->total_mb = 0;
    fi->free_mb = 0;
}

/*****************************************************************************
 * Reading the given file and searching for lines that starts with /
 * Those lines are the freeze dir names
 *****************************************************************************/
int get_freeze_dirs(freeze_info_t *fi, char *freeze_conf_file) {
    int fd;
    int res;
    char line[512];
    char *line_ptr;
    char *buff_ptr;

    if (!fi || !freeze_conf_file)
        return 0;

    free_freeze_dirs(fi);

    if ((fd = open(freeze_conf_file, O_RDONLY)) == -1)
        return 0;
    if ((res = read(fd, proc_file_buff, 8192)) == -1) {
        close(fd);
        return 0;
    }
    close(fd);
    proc_file_buff[res] = '\0';
    buff_ptr = proc_file_buff;

    char *line_items[20];
    int max_items = 20, items = 0;
    do {
        buff_ptr = sgets(line, 512, buff_ptr);

        line_ptr = eat_spaces(line);
        if (*line_ptr != '/')
            continue;

        items = split_str(line_ptr, line_items, max_items, " \t");
        if (items < 1)
            continue;

        fi->freeze_dirs[fi->freeze_dirs_num] = strdup(line_items[0]);
        if (fi->freeze_dirs[fi->freeze_dirs_num])
            fi->freeze_dirs_num++;
        for (int i = 0; i < items; i++) {
            free(line_items[i]);
            line_items[i] = NULL;
        }
    } while (buff_ptr);
    for (int i = 0; i < items; i++) {
        free(line_items[i]);
        line_items[i] = NULL;
    }
    return 1;
}

/*****************************************************************************
 * Calculating the space of all the freeze dirs. We assume that each dir is on
 * different partition
 *****************************************************************************/
int get_freeze_space(freeze_info_t *fi) {
    struct statfs buf;
    double ratio;

    if (!fi)
        return 0;

    fi->free_mb = 0;
    fi->total_mb = 0;
    for (int i = 0; i < fi->freeze_dirs_num; i++) {
        if (statfs((const char *) fi->freeze_dirs[i], &buf) != -1) {
            // Taking the ratio in megabytes
            ratio = (double) (buf.f_bsize) / (double) (1024 * 1024);
            fi->total_mb += (double) buf.f_blocks * ratio;
            fi->free_mb += (double) buf.f_bavail * ratio;
        }
    }
    return 1;
}

/*****************************************************************************
 * Finding if the current machine is a 64bit machine or 32bit machine
 * if we get x86_64 arch then this is a 64bit machine otherwise 32bit
 *****************************************************************************/
int get_machine_arch_size(unsigned char *machine_arch_size) {
    struct utsname u;

    if (uname(&u) == -1)
        return 0;
    //printf("MachineArch: %s\n", u.machine);
    if (strcmp(u.machine, "x86_64") == 0) {
        //printf("Got 64bit arch\n");
        *machine_arch_size = 64;
    } else {
        //printf("Assuming  32bit arch\n");
        *machine_arch_size = 32;
    }
    return 1;
}

// Getting the kernel release (like 2.6.36 number)

int get_kernel_release(char *buff) {
    struct utsname unameInfo;

    uname(&unameInfo);
    strcpy(buff, unameInfo.release);
    return 1;
}

int parse_total_cpu_line(char *line, unsigned long *total_new, unsigned long *io_wait) {
    char *p = line;
    char *e;
    unsigned long v;
    p += 4; // Skip the cpu word + 1 space

    unsigned long cpuInfo[20];
    int cpuInfoItems = 0;

    *total_new = 0;

    for (;; p = e) {
        v = strtol(p, &e, 10);
        if (p == e)
            break;
        // process v here
        cpuInfo[cpuInfoItems++] = v;
        //debug_ly(KCOMM_DEBUG, "Detected integer %ld\n", v);
        *total_new += v;
    }
    *io_wait = cpuInfo[4];
    return 1;
}


// Getting /proc/stat info (iowait) 

int update_io_wait_percent(char *io_wait_percent) {
    //static struct timeval time_new, time_old;
    static unsigned long total_new = 0, total_old = 0;
    static unsigned long io_wait_new = 0, io_wait_old = 0;

    int size = PROC_BUFF_SZ;
    if (!read_proc_file(PROC_STAT_FILE)) {
        return 0;
    }

    char line[512];
    char *buff_ptr;


    //gettimeofday(&time_new, NULL);
    debug_lr(KCOMM_DBG, "Size of stat file %d\n", size);
    proc_file_buff[size] = '\0';
    buff_ptr = proc_file_buff;


    do {
        buff_ptr = sgets(line, 500, buff_ptr);

        if (strncmp(line, "cpu ", 4) == 0) {
            parse_total_cpu_line(line, &total_new, &io_wait_new);
            break;
        }
    } while (buff_ptr);

    float f1 = io_wait_new - io_wait_old;
    float f2 = total_new - total_old;

    debug_ly(KCOMM_DEBUG, "\n\n\n\n            IO f1 %.2f    f2 %.2f\n", f1, f2);

    *io_wait_percent = (int) ((f1 / f2) * 100);
    debug_ly(KCOMM_DEBUG, "IO Wait: %d  Percent %d\n", io_wait_new, *io_wait_percent);

    //time_old = time_new;
    total_old = total_new;
    io_wait_old = io_wait_new;

    return 1;
}
