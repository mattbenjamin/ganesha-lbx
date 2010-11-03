/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box, Inc.
 * Contributor : Matt Benjamin <matt@linuxbox.com>
 *
 * Portions copyright CEA/DAM/DIF  (2008)
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

#ifndef __NFS_BACKCHAN_H
#define __NFS_BACKCHAN_H

/**
 * \file    nfs_backchan.h
 * \brief   Header supporting backchannel operations
 *
 * nfs_backchan.h : Header supporting backchannel operations
 *
 */

/**
 * 
 * Construct a backchannel for a new session
 *
 */

extern int32_t
nfs_backchan_create(nfs41_session_t *, nfs_worker_data_t *,
		    nfs_client_id_t *);

/*
 * Find an inactive slot, updated for call
 */

extern int32_t
nfs_backchan_next_seqid_and_slot(nfs41_session_backchan_t *,
				 slotid4 *, slotid4 *, sequenceid4 *);

/*
 * Check the continuity of the back channel
 */
extern int32_t nfs41_backchan_cbnull(nfs41_session_t *, uint32_t);
extern int32_t nfs41_backchan_cbseq_null(nfs41_session_t *, uint32_t);


#endif /* __NFS_BACKCHAN_H */
