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

/**
 * \file    nfs41_backchan.c
 * \brief   Support routines for NFSv41 backchannel calls
 *
 * nfs_backchan.c : Support routines for direct and deferred backchannel calls
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
#include <sys/select.h>
#include "HashData.h"
#include "HashTable.h"

#if defined( _USE_TIRPC )
#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <rpc/svc.h>
#include <rpc/clnt.h>
#include <rpc/clnt_stat.h>
#elif defined( _USE_GSSRPC )
#include <gssapi/gssapi.h>
#include <gssrpc/rpc.h>
#include <gssrpc/svc.h>
#include <gssrpc/pmap_clnt.h>
#else
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <rpc/pmap_clnt.h>
#endif

#include "log_macros.h"
#include "stuff_alloc.h"
#include "nfs23.h"
#include "nfs4.h"
#include "nfsv41.h"
#include "nfs_core.h"
#include "nfs41_session.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_file_handle.h"
#include "nfs_stat.h"


/**
 * 
 * Construct a dedicated backchannel for a new session, from the
 * connected session.  For correct operation, this function should be
 * called in BIND_CONN_TO_SESSION, which should ensure that the
 * session is backchanel only.
 */

int32_t
nfs41_backchan_create(nfs41_session_t *pnfs41_session,
		    nfs_worker_data_t *pworker,
		    nfs_client_id_t *pnfs_clientid)
{
    int32_t code = 0;
    nfs41_session_backchan_t *backchan;

#ifdef _USE_TIRPC
    backchan = pnfs41_session->back_chan;
    memset(backchan, 0, sizeof(nfs41_session_backchan_t));

    /* Hmm. */
    pnfs41_session->session_flags |= CREATE_SESSION4_FLAG_CONN_BACK_CHAN;
    pthread_mutex_init(&backchan->lock, NULL);
    backchan->flags = NFS41_BACKCHAN_FLAG_SET;
    backchan->current_slotid = 0;
    backchan->highest_slotid = 0;
    (backchan->slots[backchan->current_slotid]).sequence = 1;
    backchan->clnt =
	svc_vc_clnt_create(pworker->current_xprt,
			   pnfs_clientid->cb_program,
			   1 /* Errata ID: 2291 */,
			   SVC_VC_CLNT_CREATE_DEDICATED);
    backchan->last_check_time = (time_t) 0;
#else
    code = EINVAL;
#endif
    return (code);
}

/*
 * Find an inactive slot, updated for call
 */

int32_t
nfs_backchan_next_seqid_and_slot(nfs41_session_backchan_t *backchan,
				 slotid4 *cslot /* OUT */,
				 slotid4 *hslot /* OUT */,
				 sequenceid4 *seqid /* OUT */)
{
    slotid4 tid;
    int32_t rc = 1;
    int32_t ctr = NFS41_NB_SLOTS;
    nfs41_backchan_slot_t *slots = backchan->slots;

    pthread_mutex_lock(&backchan->lock);

    tid = backchan->current_slotid;
    while (ctr > 0) {
	tid = (tid++) % NFS41_NB_SLOTS;
	if (! slots[tid].flags & NFS41_SLOT_FLAG_ACTIVE)
	    break;
	ctr--;
    }

    if (!ctr)
	goto unlock;

    slots[tid].flags |= NFS41_SLOT_FLAG_ACTIVE;
    if (tid > backchan->highest_slotid)
	backchan->highest_slotid = tid;
    *cslot = tid;
    *hslot = backchan->highest_slotid;
    *seqid = (slots[tid].sequence)++;
    rc = 0;

unlock:
    pthread_mutex_unlock(&backchan->lock);

    return (rc);

} /* nfs_backchan_next_seqid_and_slot */

/**
 * 
 * Routine to set up an RPC call out-of-line
 *
 */

int32_t
nfs41_backhan_deferred_call(CLIENT *clnt)
{
    int32_t rc = EINVAL; /* NOT IMPLEMENTED YET */

    return (rc);

} /* nfs_backhan_direct_call */

extern enum clnt_stat rpc_cb_null(CLIENT *clnt);

/**
 * Call the CB_NULL procedure on connected session session (this may be
 * safely executed from CREATE_SESSION)
 */

int32_t
nfs41_backchan_cbnull(nfs41_session_t *session, uint32_t flags)
{
    int32_t rc = 0;
    struct nfs41_session_backchan *back_chan;
    enum clnt_stat cbst;
    CLIENT *clnt;

    back_chan = &(session->back_chan[0]);
    clnt = back_chan->clnt;

    if (clnt) {
	back_chan->last_check_time = time(NULL);
	back_chan->flags |= ~(NFS41_BACKCHAN_FLAG_CHECKED);
	cbst = rpc_cb_null(clnt);
	switch (cbst) {
	case RPC_SUCCESS:
	    back_chan->flags &= ~(NFS41_BACKCHAN_FLAG_DOWN);
	    break;
	default:
	    back_chan->flags |= NFS41_BACKCHAN_FLAG_DOWN;
	}
    }

    return (rc);

} /* nfs_backchan_cbnull*/

extern enum clnt_stat
rpc_cbseq_null(nfs41_session_t *session, CLIENT *clnt);

/**
 * Send an empty callback sequence operation on the supplied session.
 * This operation may be used to check the continuity of the backchannel,
 * perhaps with a prior CB_NULL probe. 
 */

int32_t
nfs41_backchan_cbseq_null(nfs41_session_t *session)
{
    int32_t rc = 0;
    struct nfs41_session_backchan *back_chan;
    enum clnt_stat cbst;
    CLIENT *clnt;

#if 0
    /* check backchannel state, will reprobe if last check
     * is older than configured window */
    if (!nfs41_backchan(session))
	goto out;
#endif

    back_chan = &(session->back_chan[0]);
    clnt = back_chan->clnt;

    cbst = rpc_cbseq_null(session, clnt);
    switch (cbst) {
    case RPC_SUCCESS:
	rc = NFS4_OK;
    default:
	rc = NFS4ERR_INVAL;
    }

out:
    return (rc);

} /* nfs_backchan_cbseq_null*/

int32_t
nfs41_backchan_cbseq_getattr(nfs41_session_t *session, ...)
{
    int32_t rc = 0;
    struct nfs41_session_backchan *back_chan;
    enum clnt_stat cbst;
    CLIENT *clnt;

#if 0
    /* check backchannel state, will reprobe if last check
     * is older than configured window */
    if (!nfs41_backchan(session))
	goto out;
#endif

    back_chan = &(session->back_chan[0]);
    clnt = back_chan->clnt;

#if 0
    cbst = rpc_cbseq_getattr(session, clnt, ...);
#endif
    switch (cbst) {
    case RPC_SUCCESS:
	rc = NFS4_OK;
    default:
	rc = NFS4ERR_INVAL;
    }

out:
    return (rc);

} /* nfs_backchan_cbseq_getattr */
