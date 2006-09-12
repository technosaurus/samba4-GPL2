/* 
   Copyright (C) Jelmer Vernooij 2005 <jelmer@samba.org>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __SOCKET_WRAPPER_H__
#define __SOCKET_WRAPPER_H__

int swrap_socket(int domain, int type, int protocol);
int swrap_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int swrap_connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen);
int swrap_bind(int s, const struct sockaddr *myaddr, socklen_t addrlen);
int swrap_getpeername(int s, struct sockaddr *name, socklen_t *addrlen);
int swrap_getsockname(int s, struct sockaddr *name, socklen_t *addrlen);
int swrap_getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen);
int swrap_setsockopt(int s, int  level,  int  optname,  const  void  *optval, socklen_t optlen);
ssize_t swrap_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
ssize_t swrap_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t swrap_recv(int s, void *buf, size_t len, int flags);
ssize_t swrap_send(int s, const void *buf, size_t len, int flags);
int swrap_close(int);

#ifdef SOCKET_WRAPPER_REPLACE

#ifdef accept
#undef accept
#endif
#define accept(s,addr,addrlen)		swrap_accept(s,addr,addrlen)

#ifdef connect
#undef connect
#endif
#define connect(s,serv_addr,addrlen)	swrap_connect(s,serv_addr,addrlen)

#ifdef bind
#undef bind
#endif
#define bind(s,myaddr,addrlen)		swrap_bind(s,myaddr,addrlen)

#ifdef getpeername
#undef getpeername
#endif
#define getpeername(s,name,addrlen)	swrap_getpeername(s,name,addrlen)

#ifdef getsockname
#undef getsockname
#endif
#define getsockname(s,name,addrlen)	swrap_getsockname(s,name,addrlen)

#ifdef getsockopt
#undef getsockopt
#endif
#define getsockopt(s,level,optname,optval,optlen) swrap_getsockopt(s,level,optname,optval,optlen)

#ifdef setsockopt
#undef setsockopt
#endif
#define setsockopt(s,level,optname,optval,optlen) swrap_setsockopt(s,level,optname,optval,optlen)

#ifdef recvfrom
#undef recvfrom
#endif
#define recvfrom(s,buf,len,flags,from,fromlen) 	  swrap_recvfrom(s,buf,len,flags,from,fromlen)

#ifdef sendto
#undef sendto
#endif
#define sendto(s,buf,len,flags,to,tolen)          swrap_sendto(s,buf,len,flags,to,tolen)

#ifdef recv
#undef recv
#endif
#define recv(s,buf,len,flags)		swrap_recv(s,buf,len,flags)

#ifdef send
#undef send
#endif
#define send(s,buf,len,flags)		swrap_send(s,buf,len,flags)

#ifdef socket
#undef socket
#endif
#define socket(domain,type,protocol)	swrap_socket(domain,type,protocol)

#ifdef close
#undef close
#endif
#define close(s)			swrap_close(s)
#endif

#endif /* __SOCKET_WRAPPER_H__ */
