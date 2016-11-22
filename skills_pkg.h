/* skills_pkg.h -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CCSERVER_SKILLS_PACKAGE_H
#define CCSERVER_SKILLS_PACKAGE_H
#include <memory>
#include <map>
#include <vector>
#include <google/protobuf/descriptor.h>


typedef std::shared_ptr<google::protobuf::Message> message_ptr;

class skills_package;

class Callback //: public std::noncopyable
{
    public:
        //Callback(Callback const& other)=delete;
        //Callback& operator=(Callback const& other)=delete; 
        virtual ~Callback() {};
        virtual message_ptr onMessage(message_ptr& message) const = 0;
};

/*
template<typename To, typename From>
inline std::shared_ptr<To> down_pointer_cast(const std::shared_ptr<From>& f)
{
    //return ::boost::static_pointer_cast<To>(f);
    return dynamic_cast<To>(f);
}*/

//T must be based of google::protobuf::Message!
template <typename T>
class CallbackT : public Callback
{
    public:
        typedef std::function<message_ptr (std::shared_ptr<T>& message)> message_callback_fun;
        CallbackT(const message_callback_fun& callback)
               : callback_(callback)
               {
               }

    virtual message_ptr onMessage(message_ptr& message) const
    {
        std::shared_ptr<T> concrete = dynamic_pointer_cast<T>(message);
        assert(concrete != NULL);
        return callback_(concrete);
    }

 private:
    message_callback_fun callback_;
};

class skills_package
{
    public:
        typedef std::function<message_ptr (message_ptr& message)> message_callback_fun;
        explicit skills_package(const message_callback_fun& defaultCb)
                 : defaultCallback_(defaultCb)
                 {
                 }

        message_ptr on_message(message_ptr& message) const
        {
            CallbackMap::const_iterator it = callbacks_.find(message->GetDescriptor());
            if (it != callbacks_.end())
            {
                return it->second->onMessage(message);
            }           
            return defaultCallback_(message);
        }

        template<typename T>
        void register_msg_skill(const typename CallbackT<T>::message_callback_fun& callback)
        {
            std::shared_ptr<CallbackT<T> > pd(new CallbackT<T>(callback));
            callbacks_[T::descriptor()] = pd;
        }
    private:
        typedef std::map<const google::protobuf::Descriptor*, std::shared_ptr<Callback> > CallbackMap;       
        CallbackMap callbacks_;
        message_callback_fun defaultCallback_;
};

#endif