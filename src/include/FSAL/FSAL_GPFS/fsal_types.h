/*
 *
 *
 * Copyright CEA/DAM/DIF  (2008)
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
 * ---------------------------------------
 */

/**
 * \file    fsal_types.h
 * \author  $Author: leibovic $
 * \date    $Date: 2006/02/08 12:45:27 $
 * \version $Revision: 1.19 $
 * \brief   File System Abstraction Layer types and constants.
 *
 *
 *
 */

#ifndef _FSAL_TYPES_SPECIFIC_H
#define _FSAL_TYPES_SPECIFIC_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * FS relative includes
 */

#include "config_parsing.h"
#include "err_fsal.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */

#ifndef _ATFILE_SOURCE
#define _ATFILE_SOURCE
#endif  /* _ATFILE_SOURCE */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/*
 * labels in the config file
 */

#define CONF_LABEL_FS_SPECIFIC   "GPFS"

/* -------------------------------------------
 *      POSIX FS dependant definitions
 * ------------------------------------------- */

#define FSAL_MAX_NAME_LEN   NAME_MAX
#define FSAL_MAX_PATH_LEN   PATH_MAX

#define FSAL_NGROUPS_MAX  32

/* prefered readdir size */
#define FSAL_READDIR_SIZE 2048

/** object name.  */

typedef struct fsal_name__
{
  char name[FSAL_MAX_NAME_LEN];
  unsigned int len;
} fsal_name_t;

/** object path.  */

typedef struct fsal_path__
{
  char path[FSAL_MAX_PATH_LEN];
  unsigned int len;
} fsal_path_t;

#define FSAL_NAME_INITIALIZER {"",0}
#define FSAL_PATH_INITIALIZER {"",0}

#define FSAL_GPFS_HANDLE_LEN 64
#define FSAL_GPFS_FSHANDLE_LEN 64

static const fsal_name_t FSAL_DOT = { ".", 1 };
static const fsal_name_t FSAL_DOT_DOT = { "..", 2 };

typedef struct
{
  char handle_val[FSAL_GPFS_HANDLE_LEN] ;
  unsigned int handle_len ;
  ino_t inode ;
} fsal_handle_t;  /**< FS object handle */

/** Authentification context.    */

typedef struct fsal_cred__
{
  uid_t user;
  gid_t group;
  fsal_count_t nbgroups;
  gid_t alt_groups[FSAL_NGROUPS_MAX];
} fsal_cred_t;

typedef struct fsal_export_context_t
{
  char mount_point[FSAL_MAX_PATH_LEN];
  char mnt_handle_val[FSAL_GPFS_HANDLE_LEN] ;
  char mnt_fshandle_val[FSAL_GPFS_FSHANDLE_LEN] ;
  
  unsigned int mnt_handle_len;         /* for optimizing concatenation */
  unsigned int mnt_fshandle_len;         /* for optimizing concatenation */
  unsigned int dev_id ;
} fsal_export_context_t;

#define FSAL_EXPORT_CONTEXT_SPECIFIC( _pexport_context ) (uint64_t)((_pexport_context)->dev_id)

typedef struct
{
  fsal_cred_t credential;
  fsal_export_context_t *export_context;
} fsal_op_context_t;

#define FSAL_OP_CONTEXT_TO_UID( pcontext ) ( pcontext->credential.user )
#define FSAL_OP_CONTEXT_TO_GID( pcontext ) ( pcontext->credential.group )

typedef struct fs_specific_initinfo__
{
  char gpfs_mount_point[MAXPATHLEN] ;
} fs_specific_initinfo_t;

/**< directory cookie */
typedef struct fsal_cookie__
{
  off_t cookie;
} fsal_cookie_t;

static const fsal_cookie_t FSAL_READDIR_FROM_BEGINNING = { 0 };

typedef struct fsal_lockdesc__
{
  struct flock flock;
} fsal_lockdesc_t;


/* Directory stream descriptor. */

typedef struct fsal_dir__
{
  int fd ;
  fsal_op_context_t context;    /* credential for accessing the directory */
  fsal_path_t path;
  unsigned int dir_offset ;
  fsal_handle_t handle;
} fsal_dir_t;

typedef struct fsal_file__
{
  int fd;
  int ro;                       /* read only file ? */
} fsal_file_t;

#define FSAL_FILENO( p_fsal_file )  ( (p_fsal_file)->fd )

#endif                          /* _FSAL_TYPES__SPECIFIC_H */