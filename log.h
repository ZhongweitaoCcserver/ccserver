/* log.h -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef LOGGER_H
#define LOGGER_H
#include <memory>
#include <string>
using std::string;
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h> //for basename __FILE_BASENAME__
#define __FILE_BASENAME__ basename(const_cast<char *>(__FILE__))
#include <string.h> //for strerror


#define LOG_MAX_LEN    1024 /* Default maximum length of syslog messages */
/* Log levels */
#define LL_DEBUG    0
#define LL_VERBOSE  1
#define LL_NOTICE   2
#define LL_WARNING  3
#define LL_RAW (1<<10) /* Modifier to log without timestamp */
#define CONFIG_DEFAULT_VERBOSITY LL_DEBUG

#define CCSERVERLOG logger::mylog
#define CLL_DEBUG   LL_DEBUG, __FILE__, __LINE__
#define CLL_VERBOSE LL_VERBOSE, __FILE__, __LINE__
#define CLL_NOTICE  LL_NOTICE, __FILE__, __LINE__
#define CLL_WARNING LL_WARNING, __FILE__, __LINE__
#define CLL_RAW     LL_RAW, __FILE__, __LINE__


class logger
{
    public:
        logger(){};
        ~logger();
        logger(logger const& other)=delete;
        logger& operator=(logger const& other)=delete; 
        
        static void set_log_file_name(string &file_name);
        static void set_log_level(int verbosity);
        
        static void mylog(int level, const char *file_name, int line, const char *fmt, ...);
        static void log(int level, const char *fmt, ...);
        static void log_from_handler(int level, const char *msg); 
    private:
        static void _log_row(int level, const char *msg);        
        static string _log_file_name;
        static int _verbosity;
        static FILE* _fp_log;
        static std::shared_ptr<logger> _logger_ptr;
};

#endif
