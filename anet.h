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
#ifndef CCSERVER_ANET_H
#define CCSERVER_ANET_H

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>       //for strerror
#include <errno.h>        //for strerror
#include <stdio.h>        //for snprintf
#include "sds.h"

#define ANET_OK            0
#define ANET_ERR           -1
#define ANET_PENDING_WRITE 1
#define ANET_ERR_LEN       256  //for errmsg len;

#define NET_IP_STR_LEN           46           //INET6_ADDRSTRLEN is 46, but we need to be sure 
#define DEFAULT_IOBUF_LEN        512          //
#define PROTO_MAX_IOBUF_LEN      65536        //64kb max query buffer. read max buff size;
#define PROTO_MAX_CLIENTBUF_LEN  1048576      //max pending query buffer size;


//utility function
int anet_non_block(char *err, int fd);
int anet_block(char *err, int fd);
int anet_set_reuse_addr(char *err, int fd);
int anet_v6_only(char *err, int s);
int anet_bind_listen(char *err, int s, struct sockaddr *sa, socklen_t len, int backlog);
int anet_tcp_server(char *err, int port, const char *bindaddr, int af, int backlog);
int anet_accept(char *err, int fid, char *ip, size_t ip_len, int *port);
int anet_epoll_in_del(char *err, int efid, int fid);
int anet_epoll_in_add(char *err, int efid, int fid);
int anet_write_sds_to_fid(char *err, int fid, sds sds_buf);
int anet_read_fid_to_sds(char *err, int fid, sds sds_buf);
#ifdef __cplusplus 
int anet_write_sds_to_fid(char *err, int fid, cpp_sds& cpp_sds_buf);
int anet_read_fid_to_sds(char *err, int fid, cpp_sds& cpp_sds_buf);
#endif

int anet_tcp_block_connect(char *err, int *ret_conn, const char *addr, int port);
int anet_tcp_noblock_connect(char *err, int *ret_conn, const char *addr, int port);
int anet_tcp_noblock_bind_connect(char *err, int *ret_conn, const char *addr, int port,const char *source_addr);
//if failed, call anet_tcp_noblock_connect auto. 
int anet_tcp_noblock_best_bind_connect(char *err, int *ret_conn, const char *addr, int port, const char *source_addr);
int anet_check_noblock_connect(char *err, int cfd);

#endif