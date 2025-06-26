/**
 * collectd - src/lfs.c
 * Copyright (C) 2025  Katsuhiko Ono
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Katsuhiko Ono <kono at ddn.com>
 **/

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "collectd.h"
#include "utils/common/common.h"
#include "plugin.h"
#include "syslog.h"
#include "collectd.h"
#include "filedata_config.h"
#include "filedata_read.h"
#include "filedata_common.h"
#include <sys/types.h>
#include <regex.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include <pwd.h>
#include <fcntl.h>
#include <poll.h>

#define START_FILE_SIZE (1048576)
#define MAX_FILE_SIZE   (1048576 * 1024)
#define LFS_MAX_LENGTH (1024)
#define LFS "/usr/bin/lfs"
struct filedata_configs *filedata_lfs_configs;
static int run_command(const char *cmd, char **buf, ssize_t *data_size)
{
    int bufsize = START_FILE_SIZE;
    char *filebuf;
    FILE *fp;
    ssize_t offset = 0;
    int ret = 0;

    filebuf = calloc(1, bufsize);
    if (filebuf == NULL) {
        FERROR("failed to allocate memory");
        return -1;
    }

    FINFO("running command: \"%s\"\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        FERROR("failed to run command: \"%s\"\n", cmd);
        ret = -ENOMEM;
        goto out_free;
    }

    while (fgets(filebuf + offset, bufsize - offset, fp)
            != NULL) {
        offset += strlen(filebuf + offset);
        if (bufsize <= offset + 1) {
            char *p;

            FINFO("buffer size(%d) is not enough, offset: %ld",
                  bufsize, offset);
            bufsize *= 2;
            if (bufsize > MAX_FILE_SIZE) {
                FERROR("too much output(%d), skipping",
                       bufsize);
                ret = -1;
                goto out_close;
            }
            p = realloc(filebuf, bufsize);
            if (p == NULL) {
                FERROR("not enough memory");
                ret = -1;
                goto out_close;
            }
            filebuf = p;
        }
    }
    FINFO("command [%s] output: \"%s\", length %ld\n", cmd, filebuf, offset);
out_close:
    pclose(fp);
out_free:
    if (ret) {
        free(filebuf);
    } else {
        *buf = filebuf;
        *data_size = offset;
    }
    return ret;
}

static int run_command_against_pools(const char *path, char **buf, ssize_t *data_size)
{
    int bufsize = START_FILE_SIZE;
    ssize_t	output_size = 0;
    char	*data, *data2, *filebuf;
    int	 num_of_pools = 0, num_of_mount_points = 0;
    int	 ret;
    char *tk, pool_list[1024][256], pools[1024][256], df_lines[1024][256], mount_points[1024][256];
    filebuf = calloc(1, bufsize);
    if (filebuf == NULL) {
        FERROR("failed to allocate memory");
        return -1;
    }

    ret = run_command("df -t lustre", &data,
                      &output_size);
    if (ret)
        return ret;

    if (output_size < 14) {
        free(data);
        return ret;
    }
    tk = strtok(data, "\n");
    tk = strtok(NULL, "\n");
    while (tk) {
        strcpy(df_lines[num_of_mount_points], tk);
        num_of_mount_points++;
        tk = strtok(NULL, "\n");
    }
    for (int i = 0; i < num_of_mount_points; i++) {
        tk = strtok(df_lines[i], " ");
        if (tk) {
            for (int j = 0; j < 5; j++) {
                tk = strtok(NULL, " ");
                if (!tk)
                    break;
            }
            if (tk)
                strcpy(mount_points[i], tk);
        }
    }
    free(data);
    int nbytes = 0;
    for(int i = 0; i < num_of_mount_points; i++) {
        num_of_pools = 0;
        char cmd[LFS_MAX_LENGTH];
        snprintf(cmd, sizeof(cmd), LFS" pool_list %s\n", mount_points[i]);
        ret = run_command(cmd, &data, &output_size);
        if (ret) {
            free(data);
            return ret;
        }
        if (output_size < 14) {
            free(data);
            continue;
        }
        tk = strtok(data, "\n");
        tk = strtok(NULL, "\n");
        while (tk) {
            strcpy(pool_list[num_of_pools], tk);
            num_of_pools++;
            tk = strtok(NULL, "\n");
        }
        if (num_of_pools < 1) {
            free(data);
            continue;
        }
        for (int j = 0; j < num_of_pools; j++) {
            tk = strtok(pool_list[j], ".");
            if (tk) {
                tk = strtok(NULL, ".");
                strcpy(pools[j], tk);
            }
        }
        for(int j = 0; j < num_of_pools; j++) {
            snprintf(cmd, sizeof(cmd),
                     LFS" %s %s|grep -v '^$'|grep -v ^UUID|sed \"s/^/%s /\"\n",
                     path, pools[j], pools[j]);
            ret = run_command(cmd, &data2, &output_size);
            if (ret) {
                free(data2);
                continue;
            }
            memcpy(filebuf + nbytes, data2, output_size);
            nbytes += output_size;
            free(data2);
        }
        free(data);
    }

    if (ret) {
        free(filebuf);
    } else {
        *buf = filebuf;
        *data_size = nbytes;
    }
    return ret;
}

static int lfs_read_file(const char *path, char **buf, ssize_t *data_size,
                         void *fd_private_data)
{
    int ret = 0;
    if (strcmp(path + 1, "df") == 0) {
        ret = run_command(LFS" df\n", buf, data_size);
        return ret;
    } else if (strcmp(path + 1, "df -i") == 0) {
        ret = run_command(LFS" df -i\n", buf, data_size);
        return ret;
    } else if (strcmp(path + 1, "df --pool") == 0) {
        ret = run_command_against_pools(path +1, buf, data_size);
        return ret;
    } else if (strcmp(path + 1, "df -i --pool") == 0) {
        ret = run_command_against_pools(path + 1, buf, data_size);
        return ret;
    }
    return ret;
}

static int lfs_read(void)
{
    if (filedata_lfs_configs == NULL) {
        FERROR("lfs plugin is not configured properly");
        return -1;
    }
    filedata_lfs_configs->fc_definition.fd_query_times++;
    return filedata_entry_read(filedata_lfs_configs->fc_definition.fd_root, "/");
}

static int lfs_config_internal(oconfig_item_t *ci)
{
    filedata_lfs_configs = filedata_config(ci, NULL);
    if (filedata_lfs_configs == NULL) {
        FERROR("lfs plugin: failed to configure lfs");
        return -EINVAL;
    }

    filedata_lfs_configs->fc_definition.fd_read_file = lfs_read_file;
    return 0;
}

void module_register(void)
{
    plugin_register_complex_config("lfs", lfs_config_internal);
    plugin_register_read("lfs", lfs_read);
}
