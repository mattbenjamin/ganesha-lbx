
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif

#include <sys/cdefs.h>

/*
 * svc_vc.c, Server side for Connection Oriented based RPC. 
 *
 * Actually implements two flavors of transporter -
 * a tcp rendezvouser (a listner and connection establisher)
 * and a record/tcp stream.
 */
#include <pthread.h>
/* #include <reentrant.h> */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <Rpc_com_tirpc.h>
#include "stuff_alloc.h"
#include "RW_Lock.h"
#include "nfs_core.h"
#include <pthread.h>
#include "nfs_core.h"

int getpeereid(int s, uid_t * euid, gid_t * egid);
int fridgethr_get( pthread_t * pthrid, void *(*thrfunc)(void*), void * thrarg ) ;

pthread_mutex_t *mutex_cond_xprt;
pthread_cond_t *condvar_xprt;
int *etat_xprt;

extern rw_lock_t svc_fd_lock;
extern fd_set svc_fdset;

extern void *rpc_tcp_socket_manager_thread(void *);
bool_t Rendezvous_request(SVCXPRT *, struct rpc_msg *);

/* Ganesha custom */

static void map_ipv4_to_ipv6(sin, sin6)
struct sockaddr_in *sin;
struct sockaddr_in6 *sin6;
{
  sin6->sin6_family = AF_INET6;
  sin6->sin6_port = sin->sin_port;
  sin6->sin6_addr.s6_addr32[0] = 0;
  sin6->sin6_addr.s6_addr32[1] = 0;
  sin6->sin6_addr.s6_addr32[2] = htonl(0xffff);
  sin6->sin6_addr.s6_addr32[3] = *(uint32_t *) & sin->sin_addr;
}

bool_t Rendezvous_request(xprt, msg)
SVCXPRT *xprt;
struct rpc_msg *msg;
{
  int sock, flags;
  struct cf_rendezvous *r;
  struct cf_conn *cd;
  struct sockaddr_storage addr;
  struct sockaddr_in6 sin6;
  socklen_t len;
  struct __rpc_sockinfo si;
  SVCXPRT *newxprt;
  fd_set cleanfds;

  pthread_t sockmgr_thrid;
  int rc = 0;

  assert(xprt != NULL);
  assert(msg != NULL);

  r = (struct cf_rendezvous *)xprt->xp_p1;
 again:
  len = sizeof(struct sockaddr_storage);
  if((sock = accept(xprt->xp_fd, (struct sockaddr *)(void *)&addr, &len)) < 0)
    {
      LogCrit(COMPONENT_DISPATCH, "Error in accept xp_fd=%u line=%u file=%s, errno=%u", xprt->xp_fd,
             __LINE__, __FILE__, errno);
      if(errno == EINTR)
        goto again;
      /*
       * Clean out the most idle file descriptor when we're
       * running out.
       */
      if(errno == EMFILE || errno == ENFILE)
        {
          cleanfds = svc_fdset;
          __svc_clean_idle(&cleanfds, 0, FALSE);
          goto again;
        }
      return (FALSE);
    }
  /*
   * make a new transporter (re-uses xprt)
   */

  newxprt = makefd_xprt(sock, r->sendsize, r->recvsize);
  if(addr.ss_family == AF_INET)
    {
      map_ipv4_to_ipv6((struct sockaddr_in *)&addr, &sin6);
    }
  else
    {
      memcpy(&sin6, &addr, len);
    }
  newxprt->xp_rtaddr.buf = Mem_Alloc(len);
  newxprt->xp_rtaddr.maxlen = newxprt->xp_rtaddr.len = len;
  if(newxprt->xp_rtaddr.buf == NULL)
      return (FALSE);

  if(addr.ss_family == AF_INET)
    memcpy(newxprt->xp_rtaddr.buf, &addr, len);
  else
    memcpy(newxprt->xp_rtaddr.buf, &sin6, len);
#ifdef PORTMAP
  if(sin6.sin6_family == AF_INET6 || sin6.sin6_family == AF_LOCAL)
    {
      memcpy(&newxprt->xp_raddr, newxprt->xp_rtaddr.buf, sizeof(struct sockaddr_in6));
      newxprt->xp_addrlen = sizeof(struct sockaddr_in6);
    }
#endif                          /* PORTMAP */
  if(__rpc_fd2sockinfo(sock, &si) && si.si_proto == IPPROTO_TCP)
    {
      len = 1;
      /* XXX fvdl - is this useful? */
      setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &len, sizeof(len));
    }

  cd = (struct cf_conn *)newxprt->xp_p1;

  cd->recvsize = r->recvsize;
  cd->sendsize = r->sendsize;
  cd->maxrec = r->maxrec;

  if(cd->maxrec != 0)
    {
      flags = fcntl(sock, F_GETFL, 0);
      if(flags == -1)
        return (FALSE);
      /*if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
         return (FALSE); */
      if(cd->recvsize > cd->maxrec)
        cd->recvsize = cd->maxrec;
      cd->nonblock = TRUE;
      __xdrrec_setnonblock(&cd->xdrs, cd->maxrec);
    }
  else
    cd->nonblock = FALSE;
  gettimeofday(&cd->last_recv_time, NULL);

  FD_CLR(newxprt->xp_fd, &svc_fdset);
  etat_xprt[newxprt->xp_fd] = 0;

  if((rc =
      fridgethr_get(&sockmgr_thrid, rpc_tcp_socket_manager_thread,
                     (void *)(newxprt->xp_fd))) != 0)
    return FALSE;

  return (FALSE);               /* there is never an rpc msg to be processed */
}
