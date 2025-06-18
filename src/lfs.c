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
#include "collectd.h"
#include "utils/common/common.h"
#include "plugin.h"
#include "filedata_config.h"
#include "filedata_read.h"


#define START_FILE_SIZE (1048576)
#define MAX_FILE_SIZE   (1048576 * 1024)
#define LFS_MAX_LENGTH (1024)
#define LFS_PATH "/usr/bin/lfs"

struct filedata_configs *lfs_config_g;
char pool_index[LFS_MAX_LENGTH];

static int run_command(const char *cmd, char **buf, ssize_t *data_size)
{
    int bufsize = START_FILE_SIZE;
    char *filebuf;
    FILE *fp;
    ssize_t offset = 0;
    int ret = 0;

    filebuf = calloc(1, bufsize);
    if (filebuf == NULL) {
        ERROR("failed to allocate memory");
        return -1;
    }

    INFO("running command: \"%s\"\n", cmd);
    /* Execute the command */
    fp = popen(cmd, "r");
    if (fp == NULL) {
        ERROR("failed to run command: \"%s\"\n", cmd);
        ret = -ENOMEM;
        goto out_free;
    }

    while (fgets(filebuf + offset, bufsize - offset, fp)
            != NULL) {
        offset += strlen(filebuf + offset);
        if (bufsize <= offset + 1) {
            char *p;

            INFO("buffer size(%d) is not enough, offset: %ld",
                 bufsize, offset);
            bufsize *= 2;
            if (bufsize > MAX_FILE_SIZE) {
                ERROR("too much output(%d), skipping",
                      bufsize);
                ret = -1;
                goto out_close;
            }
            p = realloc(filebuf, bufsize);
            if (p == NULL) {
                ERROR("not enough memory");
                ret = -1;
                goto out_close;
            }
            filebuf = p;
        }
    }
    INFO("command [%s] output: \"%s\", length %ld", cmd, filebuf, offset);
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

static int lfs_read_file(const char *path, char **buf, ssize_t *data_size,
                         void *fd_private_data)
{
    char cmd[LFS_MAX_LENGTH];
    int ret;

    /* Prepare request command, skipping leading / */
    snprintf(cmd, sizeof(cmd), LFS_PATH" %s\n", path + 1);

    INFO("lfs command: \"%s\"\n", cmd);
    ret = run_command(cmd, buf, data_size);
    return ret;
}

static int lfs_read(void)
{
    if (lfs_config_g == NULL) {
        ERROR("lfs plugin is not configured properly");
        return -1;
    }

    if (!lfs_config_g->fc_definition.fd_root->fe_active)
        return 0;

    lfs_config_g->fc_definition.fd_query_times++;
    return filedata_entry_read(lfs_config_g->fc_definition.fd_root, "/");
}

static int lfs_config_internal(oconfig_item_t *ci)
{
    lfs_config_g = filedata_config(ci, NULL);
    if (lfs_config_g == NULL) {
        ERROR("failed to configure lfs");
        return -1;
    }
    lfs_config_g->fc_definition.fd_read_file = lfs_read_file;
    return 0;
}

void module_register(void)
{
    plugin_register_complex_config("lfs", lfs_config_internal);
    plugin_register_read("lfs", lfs_read);
} /* void module_register */
