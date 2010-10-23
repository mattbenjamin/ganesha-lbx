/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010, The Linux Box Corporation
 * Contributor : Matt Benjamin <matt@linuxbox.com>
 *
 * Some portions Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * ------------- 
 */
#include <string.h>
#include <stdint.h>
#include "config.h"
#include "global_config.h"

/**
 * Package init, MUST be called once, race-free, before any client can use
 * the global configuration
 */
int32_t
global_config_init(char *file_path)
{
    int32_t code = 0;

    memset(&global_config, 0, sizeof(global_config_t));
    pthread_rwlock_init(&global_config.rwlock, NULL /* defaults */);
    code = global_config_reload(file_path);

    return (code);
    
}    /* global_config_init */


int32_t
global_config_reload(char *file_path)
{
    int32_t code = 0;

    code = pthread_rwlock_wrlock(&global_config.rwlock);
    if (!code) {
        /* dispose of any existing parsed configuration */
        if (global_config.config_struct)
            config_Free(global_config.config_struct);

        /* presumably because global_config.file_path has never
         * been assigned */
        if (file_path)
            strcpy(global_config.file_path, file_path);

        global_config.config_struct =
            config_ParseFile(global_config.file_path);

        pthread_rwlock_unlock(&global_config.rwlock);
    }

    return (code);

}    /* global_config_reload */


config_file_t 
get_global_config(uint32_t flags)
{
    int32_t code;
    config_file_t config_struct = NULL;

    code = pthread_rwlock_wrlock(&global_config.rwlock);
    if (!code) {
        config_struct = global_config.config_struct;
        global_config.readers++;
    }

    return (config_struct);

}    /* get_global_config */


put_global_config(uint32_t flags)
{
    int32_t code;

    global_config.readers--;
    code = pthread_rwlock_unlock(&global_config.rwlock);

    return (code);

}    /* put_global_config */

