/* api_pb.h -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef API_PB_H
#define API_PB_H
#include <google/protobuf/descriptor.h>
#include "bankhall.pb.h"

typedef std::shared_ptr<google::protobuf::Message> message_ptr;
typedef std::shared_ptr<ccserver::withdraw> withdraw_msg_ptr;
typedef std::shared_ptr<ccserver::deposit> deposit_msg_ptr;
typedef std::shared_ptr<ccserver::outsourcing> outsourcing_msg_ptr;

#endif
