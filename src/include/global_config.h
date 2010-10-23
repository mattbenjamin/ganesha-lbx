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
#ifndef _GLOBAL_CONFIG_H
#define _GLOBAL_CONFIG_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "log_functions.h" /* MAXPATHLEN */
#include "config_parsing.h"

/**
 * Shared, protected configuration.  Routines actively consuming the
 * configuration must take shared locks, and a routine which is rebuilding
 * the configuration must take an exclusive lock.  Before initialization,
 * flags == GLOBAL_CONFIG_NONE, and after initialization, the flag bit
 * GLOBAL_CONFIG_INITIALIZED shall be set.  Should patterns of contention
 * develop, a thread which needs to rebuild the configuration can use CAS
 * (or similar) to set GLOBAL_CONFIG_RELOAD to request threads shared
 * locks.  Ideally, no shared lock will be outstanding for more than a few
 * milliseconds, obviating this communication.
 */

#define GLOBAL_CONFIG_NONE              0
#define GLOBAL_CONFIG_INITIALIZED       1
#define GLOBAL_CONFIG_RELOAD            2

typedef struct global_config
{
    uint32_t flags, readers;
    pthread_rwlock_t rwlock;
    char file_path[MAXPATHLEN];
    config_file_t config_struct;
} global_config_t;

extern global_config_t global_config;

#define GLOBAL_CONFIG_SHARED            1
#define GLOBAL_CONFIG_EXCLUSIVE         2

extern int32_t global_config_init(char *file_path);
extern int32_t global_config_reload(char *file_path);
extern config_file_t get_global_config(uint32_t flags);
extern void put_global_config(uint32_t flags);
extern void global_config_destroy();

#endif /* _GLOBAL_CONFIG_H */
