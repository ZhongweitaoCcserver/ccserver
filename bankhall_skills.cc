/* bankhall_skills.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "bankhall_skills.h"
#include "log.h"
using namespace std;


message_ptr onUnknownMessageType(message_ptr& message)
{
    //CCSERVERLOG(CLL_NOTICE,  "onUnknownMessageType :%s", message->GetTypeName().c_str());
    //sleep_nanosec(10000); //1000,000 = one second,
    //message->PrintDebugString();
    return message;
}

message_ptr cash_withdraw(withdraw_msg_ptr& message)
{
    //cout << "on_withdraw: " << message->GetTypeName() << endl;  
    //sleep_nanosec(10000); //
    //message->PrintDebugString();
    return message;
}

message_ptr cash_deposit(deposit_msg_ptr& message)
{
    //LOG_INFO << "on_deposit: " << deposit_message->GetTypeName() << " : ";
    //sleep_nanosec(10000); //
    //message->PrintDebugString();
    return message;
}

message_ptr on_outsourcing(outsourcing_msg_ptr& message)
{
    /*
    cout << "on_outsourcing: " << outs_msg->GetTypeName() << " : " << endl;
	std::string str_withdraw = outs_msg->data();
	
	outs_msg->PrintDebugString();
	cout << "/////////////////////" << endl;
	
    ProtobufCodec::ErrorCode errorCode2 = ProtobufCodec::kNoError;
	const int32_t len_withdraw = static_cast<int32_t>(str_withdraw.size());
    //withdrawPtr  new_withdraw_msg = muduo::down_pointer_cast<withdrawPtr> ( ProtobufCodec::parse(str_withdraw.c_str(), len_withdraw, &errorCode2));
	MessagePtr  new_withdraw_msg = ProtobufCodec::parse(str_withdraw.c_str(), len_withdraw, &errorCode2);
    cout << "haha" << endl;
    dispatcher.onProtobufMessage(new_withdraw_msg);
    cout << "Bdgdfdgd" << endl;  
	
	return nullptr;
    */
    return nullptr;
}






