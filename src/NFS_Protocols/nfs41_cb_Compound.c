/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 * Portions copyright Linux Box Corporation (2010)
 * contributor :  Matt Benjamin
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
 * \file    nfs4_cb_Compound.c
 * \author  $Author: deniel $
 * \date    $Date: 2008/03/11 13:25:44 $
 * \version $Revision: 1.24 $
 * \brief   Routines used for managing the NFS4/CB COMPOUND functions.
 *
 * nfs4_cb_Compound.c : Routines used for managing the NFS4/CB COMPOUND functions.
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
#include "nfsv41.h"
#include "mount.h"
#include "nfs_core.h"
#include "nfs41_backchan.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"

/* XXX */
typedef struct nfs4_cb_desc__
{
  char *name;
  unsigned int val;
  int (*funct) (struct nfs_cb_argop4 *, compound_data_t *, struct nfs_cb_resop4 *);
} nfs4_cb_desc_t;

/* This array maps the operation number to the related position in array optab4 */
const int cbtab4index[] = { 0, 0, 0, 0, 1, 2 };

static const nfs4_cb_desc_t cbtab4[] = {
  {"OP_CB_GETATTR", NFS4_OP_CB_GETATTR, nfs4_cb_getattr},
  {"OP_CB_RECALL", NFS4_OP_CB_RECALL, nfs4_cb_recall},
  {"OP_CB_ILLEGAL", NFS4_OP_CB_ILLEGAL, nfs4_cb_illegal},
};

/* XXX */
#define NFS4_CB_SEQUENCE_NOP          0

typedef struct nfs4_cb_tag
{
    uint32_t ix;
    char *val;
    uint32_t len;
} nfs4_cb_tag_t;

static const nfs4_cb_tag_t cbtagtab4[] = {
    {NFS4_CB_SEQUENCE_NOP, "Ganesha CB_SEQUENCE NOP", 23}
};

static inline void
cb_compound_init(CB_COMPOUND4args *cba, nfs_cb_argop4 *cba_un, uint32_t un_len,
		 uint32_t tagix)
{
    memset(cba, 0, sizeof(CB_COMPOUND4args)); /* XDRS */

    cba->minorversion = 1;
    cba->argarray.argarray_len = un_len;
    cba->argarray.argarray_val =  (nfs_cb_argop4 *) cba_un;

    /* optional tag */    
    cba->tag.utf8string_val = cbtagtab4[tagix].val;
    cba->tag.utf8string_len = cbtagtab4[tagix].len;

} /* cb_compount_init */

static inline void
cb_compound_resinit(CB_COMPOUND4res *cbr)
{
    memset(cbr, 0, sizeof(CB_COMPOUND4res));
}

/**
 * Send an empty callback sequence operation on the supplied session.
 * This operation may be used to check the continuity of the backchannel,
 * perhaps with a prior CB_NULL probe. 
 */

enum clnt_stat
rpc_cbseq_null(nfs41_session_t *session, CLIENT *clnt)
{
    enum clnt_stat rs;
    CB_COMPOUND4args cba[1];
    CB_COMPOUND4res cbr[1];
    nfs_cb_argop4 cba_un[1];
    CB_SEQUENCE4args *acbs;

    struct timeval CB_TIMEOUT = {15, 0};

    cb_compound_init(cba, cba_un, 1 /* <- _len */, NFS4_CB_SEQUENCE_NOP);

    /* Enclosing a sequence */
    cba_un->argop = NFS4_OP_CB_SEQUENCE;
    acbs = (CB_SEQUENCE4args *) &(cba_un->nfs_cb_argop4_u);

    if(nfs_backchan_next_seqid_and_slot(session->back_chan,
					&(acbs->csa_slotid),
					&(acbs->csa_highest_slotid),
					&(acbs->csa_sequenceid)))
       return RPC_CANTSEND;

    /* Assign session and computed slot information */
    memcpy(acbs->csa_sessionid, session->session_id, NFS4_SESSIONID_SIZE);
    acbs->csa_cachethis = FALSE;

    cb_compound_resinit(cbr);

    rs = clnt_call(clnt,
		   CB_COMPOUND,
		   (xdrproc_t) xdr_CB_COMPOUND4args, cba,
		   (xdrproc_t) xdr_CB_COMPOUND4res, cbr,
		   CB_TIMEOUT);
	
    return (rs);

} /* rpc_cbseq_null */

/**
 * Send an arbitrary callback sequence operation on the supplied session.
 * This operation may be used to check the continuity of the backchannel,
 * perhaps with a prior CB_NULL probe. 
 */

enum clnt_stat
rpc_cbseq_dynamic(nfs41_session_t *session, CLIENT *clnt, nfs_cb_argop4 *args,
		  uint32_t args_len, uint32_t ctag)
{
    enum clnt_stat rs;
    CB_COMPOUND4args cba[1];
    CB_COMPOUND4res cbr[1];
    CB_SEQUENCE4args *acbs;

    struct timeval CB_TIMEOUT = {15, 0};

    cb_compound_init(cba, args, args_len, ctag);

    acbs = (CB_SEQUENCE4args *) &(args->nfs_cb_argop4_u);

    if(nfs_backchan_next_seqid_and_slot(session->back_chan,
					&(acbs->csa_slotid),
					&(acbs->csa_highest_slotid),
					&(acbs->csa_sequenceid)))
       return RPC_CANTSEND;

    /* Assign session and computed slot information */
    memcpy(acbs->csa_sessionid, session->session_id, NFS4_SESSIONID_SIZE);
    acbs->csa_cachethis = FALSE;

    cb_compound_resinit(cbr);

    rs = clnt_call(clnt,
		   CB_COMPOUND,
		   (xdrproc_t) xdr_CB_COMPOUND4args, cba,
		   (xdrproc_t) xdr_CB_COMPOUND4res, cbr,
		   CB_TIMEOUT);

    /* XXX -- freeres?  pass OUT cbr? */

    return (rs);

} /* rpc_cbseq_null */
