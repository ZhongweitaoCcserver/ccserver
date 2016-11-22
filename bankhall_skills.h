/* bankhall_skills.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef BANKHALL_SKILLS_H
#define BANKHALL_SKILLS_H
#include "api_pb.h"

 message_ptr cash_withdraw(withdraw_msg_ptr& message);
 message_ptr cash_deposit(deposit_msg_ptr& message);
 message_ptr on_outsourcing(outsourcing_msg_ptr& message);
 message_ptr onUnknownMessageType(message_ptr& message);


#endif

