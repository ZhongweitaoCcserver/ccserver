/* codec.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include "codec.h"
#include <zlib.h> 

//out_sds 
int message_code(sds *out_sds, google::protobuf::Message& message, char *err_msg)
{
    const std::string& typeName = message.GetTypeName();
    uint32_t nameLen = static_cast<uint32_t>(typeName.size()+1);// include '\0'
    uint32_t checkSum = 0;
    uint32_t byte_size = static_cast<uint32_t>(message.ByteSize());
    size_t msg_len = sizeof(nameLen) + nameLen + byte_size + sizeof(checkSum);
    
    if ( sdsavail(*out_sds) < (msg_len + sizeof(uint32_t)) )
    {
        if ( (sdsavail(*out_sds) + msg_len + sizeof(uint32_t))  > MAX_IOBUF_LEN )
        {
            strcpy(err_msg, "message too big!");
            return CODEC_ERR;
        }
        *out_sds = sdsMakeRoomFor(*out_sds, msg_len + sizeof(uint32_t));
        
    }
    size_t qblen = sdslen(*out_sds);
    char *data = *out_sds + qblen;
    
    uint32_t msg_len_be32 = htobe32(static_cast<uint32_t>(msg_len));
    memcpy(data, &msg_len_be32, sizeof(msg_len_be32) ),  data += sizeof(msg_len_be32);//head.
    
    uint32_t len_be32 = htobe32(nameLen);
    memcpy(data, &len_be32, sizeof(len_be32) ),  data += sizeof(len_be32);
    memcpy(data, typeName.c_str(), static_cast<size_t>(nameLen) ),  data += nameLen; // include '\0'
    
    // code copied from MessageLite::SerializeToArray() and MessageLite::SerializePartialToArray().
    //GOOGLE_DCHECK(message.IsInitialized()) << InitializationErrorMessage("serialize", message);
    uint8_t* start = reinterpret_cast<uint8_t*>(data);
    uint8_t* end = message.SerializeWithCachedSizesToArray(start);  data += byte_size;
    if ( static_cast<uint32_t>(end - start) != byte_size )
    {
        strcpy(err_msg, "Serialize With Cached Sizes To Array Error !");
        return CODEC_ERR;
    }
    
    const Bytef* check_pos = reinterpret_cast<const Bytef*>(*out_sds + qblen + sizeof(msg_len_be32));
    int check_len = static_cast<int>(data - (*out_sds + qblen + sizeof(msg_len_be32)) );
    checkSum = static_cast<uint32_t>(::adler32(1, check_pos, check_len) );
    memcpy(data, &checkSum, sizeof(checkSum) ), data += sizeof(checkSum);
    
    int nread = static_cast<int>(msg_len + sizeof(uint32_t) );
    sdsIncrLen(*out_sds, nread);
    return CODEC_OK;
}

static google::protobuf::Message* createMessage(const std::string& typeName)
{
    google::protobuf::Message* message = nullptr;
    const google::protobuf::Descriptor* descriptor =
                  google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
    if (descriptor)
    {
        const google::protobuf::Message* prototype =
                  google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype)
        {
            message = prototype->New();
        }
    }
    
    return message;
}

int32_t asInt32(const char* buf)
{
  int32_t be32 = 0;
  ::memcpy(&be32, buf, sizeof(be32));
  return static_cast<int32_t>(be32toh(be32));
}

#include <iostream>
using namespace std;
//there are many message in in_sds, a new message will start from  in_sds+*len;
message_ptr message_decode(const sds in_sds, size_t *len, char *err_msg)
{
    size_t in_sds_len = sdslen(in_sds) - *len;
    if (in_sds_len == 0 )
    {
        return nullptr;
    }
    
    //check length first!
    if ( in_sds_len <  sizeof(uint32_t) ) //include in_sds_len < 0;
    {
        strcpy(err_msg, "Invalid Length!");
        return nullptr;
    }
    
    uint32_t msg_len_be32 = 0;
    char *new_msg_pos = in_sds + *len;
    memcpy(&msg_len_be32, new_msg_pos, sizeof(uint32_t) );
    uint32_t msg_len = be32toh(msg_len_be32);
    //check length first!
    if ( in_sds_len < ( static_cast<size_t>(msg_len) + sizeof(uint32_t) ) )
    {
        strcpy(err_msg, "Invalid Length!");
        return nullptr;
    }
    
    // check sum
    uint32_t oriCheckSum = 0;
    //char *pos = new_msg_pos + sizeof(msg_len) + msg_len - sizeof(oriCheckSum);  equal to (new_msg_pos + msg_len)
    char *pos_unpack = new_msg_pos + msg_len ;
    memcpy(&oriCheckSum, pos_unpack, sizeof(oriCheckSum) );
    
    pos_unpack = new_msg_pos + sizeof(msg_len);
    uint32_t checkSum = static_cast<uint32_t>(adler32(1, reinterpret_cast<const Bytef*>(pos_unpack), 
                                               static_cast<int>(msg_len - sizeof(oriCheckSum) ) ) );
    //to skip per-version's bug!.
    if ( (checkSum != oriCheckSum) && (asInt32(reinterpret_cast<const char*>(&checkSum)) != static_cast<int32_t>(oriCheckSum) ) )
    //if (checkSum != oriCheckSum) 
    {
        strcpy(err_msg, "CheckSum Error!");
        return nullptr;
    }
     
    // get message type name
    uint32_t len_be32 = 0;
    memcpy(&len_be32, pos_unpack, sizeof(uint32_t) ), pos_unpack += sizeof(uint32_t);
    uint32_t nameLen = be32toh(len_be32);
    
    //if (nameLen >= 2 && nameLen <= len - 2*kHeaderLen)
    if (nameLen < 2 || nameLen >= ( msg_len - 2*sizeof(uint32_t) ) )  //too small or too big(no space for message).
    {
        strcpy(err_msg, "Invalid Length!");
        return nullptr;        
    }
    
    std::string typeName(pos_unpack, pos_unpack + nameLen - 1); pos_unpack += nameLen; //string construct will add '\0' auto.
      
    // create message object
    message_ptr message = std::shared_ptr<google::protobuf::Message>(createMessage(typeName) );   
    if (message == nullptr)
    {
        strcpy(err_msg, "Unknown Message Type!");
        return nullptr; 
    }
    // parse from buffer
    int32_t dataLen = static_cast<int32_t>(msg_len - nameLen - 2*sizeof(uint32_t) );
    //cout << "message->ParseFromArray(pos_unpack, dataLen) :" << __FILE__ << " line:" << __LINE__ << endl;
    //cout << "message->ParseFromArray(pos_unpack, dataLen) msg_len:" << msg_len << " nameLen:" << nameLen << " name:" << typeName << " dataLen:" << dataLen << endl;
    if (! message->ParseFromArray(pos_unpack, dataLen) )
    {
        //cout << "message->ParseFromArray(pos_unpack, dataLen) :" << __FILE__ << " line:" << __LINE__ << endl;
        strcpy(err_msg, "Parse Error!");
        return nullptr;
    }
    //cout << "message->ParseFromArray(pos_unpack, dataLen) :" << __FILE__ << " line:" << __LINE__ << endl;
    //
    *len += sizeof(uint32_t) + static_cast<size_t>(msg_len);
      
    return message;
}


#ifdef __cplusplus 
int message_decode(cpp_sds& in_cpp_sds, msg_queue& dec_msg_queue, char *err_msg)
{    
    size_t unpack_len = 0;
    int msg_cnt = 0;
    while(1)
    {   
        message_ptr  new_msg_ptr = message_decode(in_cpp_sds, &unpack_len, err_msg);
        if (nullptr == new_msg_ptr){
            break;
        }
        dec_msg_queue.push(new_msg_ptr);
        ++msg_cnt;
    }

    size_t remain_len = in_cpp_sds.size() - unpack_len;   
    if (remain_len == 0){
        in_cpp_sds.clear();
    }else if (unpack_len != 0 ){
        in_cpp_sds.cpylen(in_cpp_sds.c_sds + unpack_len, remain_len);
    }    
    
    return msg_cnt;
}

int message_code(cpp_sds& out_cpp_sds, google::protobuf::Message& message, char *err_msg)
{
    return message_code(&(out_cpp_sds.c_sds), message, err_msg);
}

message_ptr message_decode(cpp_sds& in_cpp_sds, size_t *unpack_len, char *err_msg)
{
    return message_decode(in_cpp_sds.c_sds, unpack_len, err_msg);
}

/*bug!!
int message_code(cpp_sds& out_sds, google::protobuf::Message& message, char *err_msg)
{
    sds out_data = out_sds.c_sds();            
    int ret = message_code(&out_data, message, err_msg);
    out_sds.reset(out_data);
    
    return ret;
}

message_ptr message_decode(cpp_sds& in_sds, char *err_msg)
{
    size_t unpack_len = 0;
    sds in_data = in_sds.c_sds();    
    message_ptr ret = message_decode(in_data, &unpack_len, err_msg);
    
    size_t remain_len = in_sds.size() - unpack_len;   
    if (remain_len == 0){
        in_sds.clear();
    }else if (unpack_len != 0 ){//still have some data to be handle(data is not enough to a full message.), need to read next time.   
        in_sds.cpylen(in_data+unpack_len, remain_len);
    }
    
    return ret;
}*/
#endif


#ifdef CODEC_TEST_MAIN
#include <iostream>
using namespace std;
#include <string>
using std::string;
//#include "sds.h"
#include "codec.h"


int test_codec()
{
    int amt = 1;
    int failed_cnt = 0;
    size_t len = 0;
    CPP_RAII_SDS(out_sds);
    char err_msg[CODEC_ERR_MSG_MAXLEN];
    
    ++amt;
    ccserver::withdraw  msg;
    msg.set_bus_msg_id("WITHDRAW01001");  
    msg.set_serial_no("SERIALNO1201001");                 
    msg.set_card_no("CARD_NO_999999998");                 
    msg.set_amount(amt);                 
    msg.set_customer("mymind");                 
    msg.set_time("sdfsdfdsfdfsdf");  
    
    int ret = message_code(&out_sds, msg, err_msg);
    if (ret != CODEC_OK)
    {
        cout << "case_1: FAILED! message_code normal test" << endl;
        ++failed_cnt;
    }
    
    message_ptr new_msg_ptr = message_decode(out_sds, &len, err_msg);
    //new_msg_ptr->PrintDebugString();
    if ( new_msg_ptr ==nullptr ||  new_msg_ptr->DebugString() != msg.DebugString())
    {
        cout << "case_2: FAILED! message_code message_decode not equal to the old. " << endl;
        ++failed_cnt;
    }
    
    //change typename,
    out_sds[10] = 'g'; //
    len = 0;
    new_msg_ptr = message_decode(out_sds, &len, err_msg);
    if ( new_msg_ptr !=nullptr || strcmp(err_msg, "CheckSum Error!") !=0 )
    {
        cout << "case_3: " << err_msg << endl;
        cout << "case_3: FAILED! CheckSum Error test. " << endl;
        ++failed_cnt;
    }
    
    //sds out_sds2 = sdsempty();
    CPP_RAII_SDS(out_sds2);
    int ret2 = message_code(&out_sds2, msg, err_msg);
    if (ret2 != CODEC_OK)
    {
        cout << "case_4: FAILED! message_code normal test" << endl;
        ++failed_cnt;
    }
    
    ++amt;                 
    msg.set_amount(amt); 
    ret2 = message_code(&out_sds2, msg, err_msg);
    if (ret2 != CODEC_OK)
    {
        cout << "case_5: FAILED! message_code normal test" << endl;
        ++failed_cnt;
    }    
 
    //there are two msg in out_sds2.
    len = 0;
    new_msg_ptr = message_decode(out_sds2, &len, err_msg);
    if ( new_msg_ptr ==nullptr )
    {
        cout << "case_6: FAILED! " << err_msg << endl;
        ++failed_cnt;
    }
    cout << "---------------- " << endl;
    new_msg_ptr->PrintDebugString();
    
    new_msg_ptr = message_decode(out_sds2, &len, err_msg);
    std::shared_ptr<ccserver::withdraw> msg_w = dynamic_pointer_cast<ccserver::withdraw>(new_msg_ptr);
    if ( new_msg_ptr ==nullptr || msg_w->amount() !=3 )
    {
        cout << "case_7: FAILED! " << err_msg << endl;
        ++failed_cnt;
    }
    cout << "---------------- " << endl;
    msg_w->PrintDebugString();
    cout << "**************** " << endl;
    
    cout << "---Batch Test C--- " << endl;
    cout << "---amt:" << amt << "--- " << endl;
    sdsclear(out_sds2);
    for (int i=0; i< 500; ++i)
    {
        ++amt;                 
        msg.set_amount(amt); 
        ret2 = message_code(&out_sds2, msg, err_msg);
    }
    cout << "---amt:" << amt << "--- " << endl;
    size_t unpack_len = 0;
    int last_msg_amt = 0;
    int msg_cnt = 0;
    int cur_msg_cnt = 0;
    while(1){
        unpack_len = 0;
        cur_msg_cnt = 0;
        for (;cur_msg_cnt < 10; ++cur_msg_cnt)
        {
            new_msg_ptr = message_decode(out_sds2, &unpack_len, err_msg);
            if (new_msg_ptr == nullptr)
                break;
            std::shared_ptr<ccserver::withdraw> msg_w = dynamic_pointer_cast<ccserver::withdraw>(new_msg_ptr);
            last_msg_amt = msg_w->amount();
            ++msg_cnt;
        }
        if (new_msg_ptr == nullptr)
            break;
        
        size_t remain_len = sdslen(out_sds2) - unpack_len;
        out_sds2 = sdscpylen(out_sds2, out_sds2 + unpack_len, remain_len);
    }
    if ( last_msg_amt != amt ) ++failed_cnt;
    cout << "---msg_cnt:" << msg_cnt << "--- " << endl;
    cout << "---last_msg_amt:" << last_msg_amt << "--- " << endl;
    cout << "-Batch Test C End-- " << endl;
   
    cout << "---Batch Test CPP--- " << endl;
    cout << "---amt:" << amt << "--- " << endl;
    cpp_sds a_cpp_sds;
    for (int i=0; i< 500; ++i)
    {
        ++amt;                 
        msg.set_amount(amt); 
        //ret2 = message_code(a_cpp_sds.c_sds, msg, err_msg);
        ret2 = message_code(a_cpp_sds, msg, err_msg);
    }
    cout << "---amt:" << amt << "--- " << endl;
    msg_cnt = 0;
    while(1){
        cur_msg_cnt = 0;
        size_t up_len = 0;
        for (;cur_msg_cnt < 10; ++cur_msg_cnt)
        {   
            //new_msg_ptr = message_decode(a_cpp_sds, &up_len, err_msg);
            new_msg_ptr = message_decode(a_cpp_sds, &up_len, err_msg);
            if (new_msg_ptr == nullptr)
                break;
            std::shared_ptr<ccserver::withdraw> msg_w = dynamic_pointer_cast<ccserver::withdraw>(new_msg_ptr);
            last_msg_amt = msg_w->amount();
            ++msg_cnt;
        }
        char *pos = a_cpp_sds.c_sds + up_len;
        size_t remain_len = a_cpp_sds.size() - up_len;        
        a_cpp_sds.cpylen(pos, remain_len);
        if (new_msg_ptr == nullptr)
            break;
    }
    if ( last_msg_amt != amt ) ++failed_cnt;
    cout << "---msg_cnt:" << msg_cnt << "--- " << endl;
    cout << "---last_msg_amt:" << last_msg_amt << "--- " << endl;
    cout << "-Batch Test CPP End-- " << endl;
    
    
    if (failed_cnt == 0)
    {
        cout << "codec_test: ALL PASSED!" << endl;
    }  
    else
    {
        cout << "codec_test failed_cnt: "<<  failed_cnt <<"!" << endl;
    }

    //sdsfree(out_sds);
    //sdsfree(out_sds2);
    
    return 0;    
}

int main()
{
    return test_codec();
}
#endif

#ifdef SKILLS_PACKAGE_TEST_MAIN
#include <iostream>
using namespace std;
#include <string>
using std::string;
#include "sds.h"
#include "codec.h"
#include "bankhall_skills.h"
#include "skills_pkg.h"

int main()
{
    skills_package sk_pkg(onUnknownMessageType);
    
    sk_pkg.register_msg_skill<ccserver::withdraw>(cash_withdraw);
    sk_pkg.register_msg_skill<ccserver::deposit>(cash_deposit);
    sk_pkg.register_msg_skill<ccserver::outsourcing>(on_outsourcing);
    
    std::shared_ptr<google::protobuf::Message> msg_ptr(new ccserver::withdraw);
    std::shared_ptr<ccserver::withdraw> msg_w = dynamic_pointer_cast<ccserver::withdraw>(msg_ptr);
    
    msg_w->set_bus_msg_id("WITHDRAW01001");  
    msg_w->set_serial_no("SERIALNO1201001");                 
    msg_w->set_card_no("CARD_NO_999999998");                 
    msg_w->set_amount(1000);                 
    msg_w->set_customer("mymind");                 
    msg_w->set_time("sdfsdfdsfdfsdf");  
    
    sk_pkg.on_message(msg_ptr);
    
    return 0;
}
#endif
