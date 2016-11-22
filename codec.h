/* codec.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CODEC_H
#define CODEC_H

#include <memory>
#include <queue>
#include "api_pb.h"
#include "sds.h"

#define MAX_IOBUF_LEN (64*1024)

#define CODEC_OK 0
#define CODEC_ERR -1
#define CODEC_ERR_MSG_MAXLEN 128


int message_code(sds *out_sds, google::protobuf::Message& message, char *err_msg);
message_ptr message_decode(const sds in_sds, size_t *len, char *err_msg);

#ifdef __cplusplus 
int message_code(cpp_sds& out_cpp_sds, google::protobuf::Message& message, char *err_msg);
message_ptr message_decode(cpp_sds& in_cpp_sds, size_t *unpack_len, char *err_msg);
typedef std::queue<message_ptr> msg_queue;
int message_decode(cpp_sds& in_cpp_sds, msg_queue& dec_msg_queue, char *err_msg);
#endif

#endif
