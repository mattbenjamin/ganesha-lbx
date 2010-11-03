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
/**
 * \file    nfs4_op_bind_conn_to_session.c
 * \author  $Author: matt@linuxbox.com $
 * \date    $Date: 2010/09/23 18:13:00 $
 * \brief   Routines used for NFS4_OP_BIND_CONN_TO_SESSION
 *
 * nfs4_op_create_session.c :  Routines used for NFS4_OP_BIND_CONN_TO_SESSION
 *
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include "HashData.h"
#include "HashTable.h"
#ifdef _USE_GSSRPC
#include <gssrpc/types.h>
#include <gssrpc/rpc.h>
#include <gssrpc/auth.h>
#include <gssrpc/pmap_clnt.h>
#else
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/auth.h>
#include <rpc/pmap_clnt.h>
#endif

#include "log_macros.h"
#include "stuff_alloc.h"
#include "nfs23.h"
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_file_handle.h"
#include "nfs_tools.h"

extern time_t ServerBootTime;

/**
 *
 * nfs4_op_bind_conn_to_session:  The NFS4_OP_BIND_CONN_TO_SESSION operation.
 *
 * The NFS4_OP_BIND_CONN_TO_SESSION operation.
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 *
 * @return NFS4_OK if successful, other values show an error. 
 *
 * @see nfs4_Compound
 *
 */

int32_t
nfs41_op_bind_conn_to_session(struct nfs_argop4 *op, compound_data_t * data,
			      struct nfs_resop4 *resp)
{
    int32_t code = 0;
    nfs41_session_t *pnfs41_session = NULL;
    nfs_client_id_t *pnfs_clientid = NULL;

#define arg_BIND_CONN_TO_SESSION4 op->nfs_argop4_u.opbind_conn_to_session
#define res_BIND_CONN_TO_SESSION4 resp->nfs_resop4_u.opbind_conn_to_session

    LogDebug(COMPONENT_NFS_V4, "BIND_CONN_TO_SESSION");

    resp->resop = NFS4_OP_BIND_CONN_TO_SESSION;

    /* We are prepared to deal only with attempts to bind a connection
     * for the back channel */
    if (!arg_BIND_CONN_TO_SESSION4.bctsa_dir & CDFS4_BACK) {
	res_BIND_CONN_TO_SESSION4.bctsr_status =
	    NFS4ERR_CONN_NOT_BOUND_TO_SESSION;
	goto out;
    }

    code = nfs41_Session_Get(&(arg_BIND_CONN_TO_SESSION4.bctsa_sessid),
			     pnfs41_session);
    if (!code) {
	res_BIND_CONN_TO_SESSION4.bctsr_status = NFS4ERR_BADSESSION;
	goto out;
    }

    if(nfs_client_id_Get_Pointer(pnfs41_session->clientid, &pnfs_clientid) !=
       CLIENT_ID_SUCCESS) {
	res_BIND_CONN_TO_SESSION4.bctsr_status = NFS4ERR_STALE_CLIENTID;
	goto out;
    }

    code = nfs41_backchan_create(pnfs41_session, (nfs_worker_data_t *)
				 data->pclient->pworker, pnfs_clientid);
    if (code) {
	res_BIND_CONN_TO_SESSION4.bctsr_status =
	    NFS4ERR_CONN_NOT_BOUND_TO_SESSION;
	goto out;
    }

    res_BIND_CONN_TO_SESSION4.BIND_CONN_TO_SESSION4res_u.bctsr_resok4.bctsr_dir = CDFS4_BACK;
    res_BIND_CONN_TO_SESSION4.bctsr_status = NFS4_OK;

out:
    return (res_BIND_CONN_TO_SESSION4.bctsr_status);

} /* nfs41_op_create_session */

/**
 * nfs41_op_bind_conn_to_session_Free: frees what was allocared to handle nfs41_op_bind_conn_to_session.
 * 
 * Frees what was allocared to handle nfs41_op_bind_conn_to_session.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs41_op_bind_conn_to_session_Free(CREATE_SESSION4res * resp)
{
  /* To be completed */
  return;
}                               /* nfs41_op_bind_conn_to_session_Free */
