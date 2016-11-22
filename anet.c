/* anet.c -- Basic TCP socket stuff made a bit less boring
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "anet.h"

static void _anet_set_err(char *err, const char *fmt, ...)
{
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, ANET_ERR_LEN, fmt, ap);
    va_end(ap);
}

static int _anet_set_block(char *err, int fd, int non_block) {
    int flags;

    /* Set the socket blocking (if non_block is zero) or non-blocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        _anet_set_err(err, "fcntl(F_GETFL): %s", strerror(errno));
        return ANET_ERR;
    }

    if (non_block)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) == -1) {
        _anet_set_err(err, "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anet_non_block(char *err, int fd) 
{
    return _anet_set_block(err, fd, 1);
}

int anet_block(char *err, int fd) 
{
    return _anet_set_block(err, fd, 0);
}


int anet_set_reuse_addr(char *err, int fd) 
{
    int yes = 1;
    /* Make sure connection-intensive things like the redis benckmark
     * will be able to close/open sockets a zillion of times */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        _anet_set_err(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}


int anet_v6_only(char *err, int fd) 
{
    int yes = 1;
    if (setsockopt(fd, IPPROTO_IPV6,IPV6_V6ONLY, &yes, sizeof(yes)) == -1) {
        _anet_set_err(err, "setsockopt: %s", strerror(errno));
        close(fd);
        return ANET_ERR;
    }
    return ANET_OK;
}

int anet_bind_listen(char *err, int s, struct sockaddr *sa, socklen_t len, int backlog) 
{
    if (bind(s, sa, len) == -1) {
        close(s);
        _anet_set_err(err, "bind: %s", strerror(errno));
        return ANET_ERR;
    }

    if (listen(s, backlog) == -1) {
        close(s);
        _anet_set_err(err, "listen: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anet_tcp_server(char *err, int port, const char *bindaddr, int af, int backlog)
{
    int s = -1;
    int rv = -1;
    char _port[6];  /* strlen("65535") */
    struct addrinfo hints, *servinfo, *p;

    snprintf(_port, 6, "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    /* No effect if bindaddr != NULL */

    if ((rv = getaddrinfo(bindaddr, _port, &hints, &servinfo)) != 0) 
    {
        _anet_set_err(err, "listen: %s", gai_strerror(rv));
        return ANET_ERR;
    }
    
    for (p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;

        if (af == AF_INET6 && (anet_v6_only(err, s) == ANET_ERR) ) goto error;
        if (anet_set_reuse_addr(err, s) == ANET_ERR) goto error;
        if (anet_bind_listen(err, s, p->ai_addr, p->ai_addrlen, backlog) == ANET_ERR) goto error;
        
        goto end;
    }

error:
    s = ANET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}

static int _anet_accept(char *err, int s, struct sockaddr *sa, socklen_t *len) 
{
    int fd;
    while(1) {
        fd = accept(s, sa, len);
        if (fd == -1) {
            if (errno == EINTR)
                continue;
            else {
                _anet_set_err(err, "accept: %s", strerror(errno));
                return ANET_ERR;
            }
        }
        break;
    }
    
    return fd;
}

int anet_accept(char *err, int fid, char *ip, size_t ip_len, int *port)
{
    int fd;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    if ((fd = _anet_accept(err, fid, (struct sockaddr*)&sa, &salen)) == ANET_ERR)
    {
        //_anet_set_err(err, "accept: %s", strerror(errno));
        return ANET_ERR;
    }

    if (sa.ss_family == AF_INET) 
    {
        struct sockaddr_in *s4 = (struct sockaddr_in *)&sa;
        if (ip) inet_ntop(AF_INET,(void*)&(s4->sin_addr),ip,ip_len);
        if (port) *port = ntohs(s4->sin_port);
    } 
    else 
    {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6,(void*)&(s6->sin6_addr),ip,ip_len);
        if (port) *port = ntohs(s6->sin6_port);
    }
    
    return fd;    
}

int anet_epoll_in_add(char *err, int efid, int fid)
{
    struct epoll_event ev;
    
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = fid;
    ev.events = EPOLLIN;  //no EPOLLET
    int ret = epoll_ctl(efid, EPOLL_CTL_ADD, fid, &ev);
    if (ret == -1){
        _anet_set_err(err, "epoll_ctl: %s", strerror(errno));
        return ANET_ERR;
    }
        
    return ANET_OK;    
}

int anet_epoll_in_del(char *err, int efid, int fid)
{
    struct epoll_event ev; 
    
    //BUGS. man epoll_wait infomation.
    //In  kernel  versions before 2.6.9, the EPOLL_CTL_DEL operation required a non-NULL pointer in event, even though this argument is ignored.
    //Since Linux 2.6.9, event can be specified as NULL when using EPOLL_CTL_DEL.  Applications that need to be portable to kernels before 2.6.9
    //should specify a non-NULL pointer in event
    ev.data.fd = fid;    //never mind, all the events related to fid will be delete.
    ev.events = EPOLLIN; //never mind, all the events related to fid will be delete.
    //int ret = epoll_ctl(efid, EPOLL_CTL_DEL, fid, NULL); it's ok, but a non-NULL is better to skip bug. see above.  
    int ret = epoll_ctl(efid, EPOLL_CTL_DEL, fid, &ev);  
    if (ret == -1){
        _anet_set_err(err, "epoll_ctl: %s", strerror(errno));
        return ANET_ERR;
    }
        
    return ANET_OK;      
}

#ifdef CCSERVER_PP_CLIENT
#include <atomic>
std::atomic<long> total_bytes_read(0);
std::atomic<long> total_msgs_read(0);
#endif
int anet_read_fid_to_sds(char *err, int fid, sds *sds_buf)
{
    char extrabuf[PROTO_MAX_IOBUF_LEN];
    struct iovec vec[2];
    
    size_t avail = sdsavail(*sds_buf);
    if ( avail == 0){
        *sds_buf = sdsMakeRoomFor(*sds_buf, DEFAULT_IOBUF_LEN);
    }

    size_t readlen = sdsavail(*sds_buf);  
    size_t qblen = sdslen(*sds_buf);

    vec[0].iov_base = *sds_buf+qblen;
    vec[0].iov_len = readlen;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);
    const int iovcnt = 2;
    
    ssize_t nread = 0;
    while(1)
    {
        nread = readv(fid, vec, iovcnt);
        if (nread < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
                return ANET_OK;//read again next time.
        }
#ifdef CCSERVER_PP_CLIENT
        total_bytes_read += nread;
        total_msgs_read += 1;
#endif        
        break;//read only once. break now!
    }

    if (nread < 0){
        _anet_set_err(err, "readv: %s", strerror(errno));
        return ANET_ERR;      
    }else if (nread == 0){
        _anet_set_err(err, "Client closed connection.");
        return ANET_ERR;
    } else if (static_cast<size_t>(nread) <= readlen){
        sdsIncrLen(*sds_buf, nread);
    } else {
        if ( (qblen + nread) > PROTO_MAX_CLIENTBUF_LEN) {
            _anet_set_err(err, "Closing client that reached max query buffer size: %d, nread:%.", PROTO_MAX_CLIENTBUF_LEN, nread);
            return ANET_ERR;
        }       
        sdsIncrLen(*sds_buf, readlen);
        *sds_buf = sdsMakeRoomFor(*sds_buf, nread - readlen);
        memcpy(*sds_buf + readlen, extrabuf, nread - readlen);
        sdsIncrLen(*sds_buf, nread - readlen);
    }    
    return ANET_OK;
}
#ifdef __cplusplus 
int anet_read_fid_to_sds(char *err, int fid, cpp_sds& cpp_sds_buf)
{ 
    return anet_read_fid_to_sds(err, fid, &(cpp_sds_buf.c_sds));
}
#endif

//the data will be remove after send to socket write buffer.
int anet_write_sds_to_fid(char *err, int fid, sds *sds_buf)
{
    size_t data_len = sdslen(*sds_buf);
    ssize_t remain_len = static_cast<ssize_t>(data_len);
    
    ssize_t nwritten = 0;
    char *data = *sds_buf;
    while(remain_len > 0)
    {
        nwritten = write(fid, data, remain_len);
        if (nwritten < 0) break; //? <=
        if (nwritten == remain_len)
        {   
            sdsclear(*sds_buf);
            return ANET_OK;
            break;
        }
        data += nwritten;
        remain_len -= nwritten;
    }
    
    //Pending write. 
    if (remain_len != static_cast<ssize_t>(data_len))
    {   //some data have been send, some remain;
        *sds_buf = sdscpylen(*sds_buf, data, remain_len);
    }
    
    if (nwritten == -1)
    {
        if ( (errno == EAGAIN) || (errno == EINTR) )
        {
            return ANET_PENDING_WRITE;
        } 
        else 
        {
            _anet_set_err(err, "write: %s", strerror(errno));
            return ANET_ERR;
        }
    }    
    return ANET_PENDING_WRITE;
}

#define ANET_CONNECT_NONE 0
#define ANET_CONNECT_NONBLOCK 1
#define ANET_CONNECT_BE_BINDING 2 /* Best effort binding. */

static int _anet_tcp_generic_connect(char *err, int *ret_conn, const char *addr, int port, const char *source_addr, int flags)
{
    int s = ANET_ERR, rv;
    char portstr[6];  /* strlen("65535") + 1; */
    struct addrinfo hints, *servinfo, *bservinfo, *p, *b;

    snprintf(portstr,sizeof(portstr),"%d",port);
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(addr,portstr,&hints,&servinfo)) != 0) {
        _anet_set_err(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    
    for (p = servinfo; p != NULL; p = p->ai_next) {
        /* Try to create the socket and to connect it.
         * If we fail in the socket() call, or on connect(), we retry with
         * the next entry in servinfo. */
        if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;
        if (anet_set_reuse_addr(err, s) == ANET_ERR) goto error;
        if (flags & ANET_CONNECT_NONBLOCK && anet_non_block(err,s) != ANET_OK)
            goto error;
        if (source_addr) {
            int bound = 0;
            /* Using getaddrinfo saves us from self-determining IPv4 vs IPv6 */
            if ((rv = getaddrinfo(source_addr, NULL, &hints, &bservinfo)) != 0)
            {
                _anet_set_err(err, "%s", gai_strerror(rv));
                goto error;
            }
            for (b = bservinfo; b != NULL; b = b->ai_next) {
                if (bind(s,b->ai_addr,b->ai_addrlen) != -1) {
                    bound = 1;
                    break;
                }
            }
            freeaddrinfo(bservinfo);
            if (!bound) {
                _anet_set_err(err, "bind: %s", strerror(errno));
                goto error;
            }
        }
        //If the connection or binding succeeds, zero is returned.  On error, -1 is returned, and errno is set appropriately.
        //EINPROGRESS
        //      The  socket  is nonblocking and the connection cannot be completed immediately.  It is possible to select(2) or poll(2) for comple‐
        //      tion by selecting the socket for writing.  After select(2) indicates writability, use getsockopt(2) to read the SO_ERROR option  at
        //      level SOL_SOCKET to determine whether connect() completed successfully (SO_ERROR is zero) or unsuccessfully (SO_ERROR is one of the
        //      usual error codes listed here, explaining the reason for the failure).
        //
        *ret_conn =  connect(s, p->ai_addr, p->ai_addrlen);     
        if (*ret_conn == -1) {
            /* If the socket is non-blocking, it is ok for connect() to
             * return an EINPROGRESS error here. */
            if (errno == EINPROGRESS && flags & ANET_CONNECT_NONBLOCK)
                goto end;
            close(s);
            s = ANET_ERR;
            continue;
        }

        /* If we ended an iteration of the for loop without errors, we
         * have a connected socket. Let's return to the caller. */
        goto end;
    }
    if (p == NULL)
        _anet_set_err(err, "creating socket: %s", strerror(errno));

error:
    if (s != ANET_ERR) {
        close(s);
        s = ANET_ERR;
    }

end:
    freeaddrinfo(servinfo);

    /* Handle best effort binding: if a binding address was used, but it is
     * not possible to create a socket, try again without a binding address. */
    if (s == ANET_ERR && source_addr && (flags & ANET_CONNECT_BE_BINDING)) {
        return _anet_tcp_generic_connect(err, ret_conn, addr, port, NULL, flags);
    } else {
        return s;
    }
}

int anet_tcp_block_connect(char *err, int *ret_conn, const char *addr, int port)
{
    return _anet_tcp_generic_connect(err, ret_conn, addr, port, NULL, ANET_CONNECT_NONE);
}

int anet_tcp_noblock_connect(char *err, int *ret_conn, const char *addr, int port)
{
    return _anet_tcp_generic_connect(err, ret_conn, addr, port, NULL, ANET_CONNECT_NONBLOCK);
}

int anet_tcp_noblock_bind_connect(char *err, int *ret_conn, const char *addr, int port, const char *source_addr)
{
    return _anet_tcp_generic_connect(err, ret_conn, addr, port, source_addr, ANET_CONNECT_NONBLOCK);
}

int anet_tcp_noblock_best_bind_connect(char *err, int *ret_conn, const char *addr, int port, const char *source_addr)
{
    return _anet_tcp_generic_connect(err, ret_conn, addr, port, source_addr, ANET_CONNECT_NONBLOCK|ANET_CONNECT_BE_BINDING);
}

int anet_check_noblock_connect(char *err, int cfd)
{
    int sockerr = 0;
    socklen_t errlen = sizeof(sockerr);
    
     /* Check for errors in the socket. */
    if (getsockopt(cfd, SOL_SOCKET, SO_ERROR, &sockerr, &errlen) == -1){
        sockerr = errno;
    }
    if (sockerr) {
        _anet_set_err(err, "%s", strerror(sockerr));
        return ANET_ERR;
    }
    
    return ANET_OK;
}

#ifdef __cplusplus 
int anet_write_sds_to_fid(char *err, int fid, cpp_sds& cpp_sds_buf)
{
    return anet_write_sds_to_fid(err, fid, &(cpp_sds_buf.c_sds));
}
#endif
