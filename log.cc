/* log.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include "log.h"
#include "util.h"


FILE* logger::_fp_log = stdout;
string logger::_log_file_name = "\0";
std::shared_ptr<logger> logger::_logger_ptr;
int logger::_verbosity = LL_DEBUG;
 
void logger::set_log_file_name(string &file_name)
{
     _log_file_name = file_name;
     FILE *fp;
     fp = fopen(_log_file_name.c_str(), "a");
     if (!fp) return;
     _fp_log = fp;
     _logger_ptr = std::shared_ptr<logger>(new logger);
}

logger::~logger()
{
    if (_fp_log != stdout)
        fclose(_fp_log);
}

void logger::set_log_level(int verbosity)
{
    _verbosity = verbosity;
}
 
 /* Low level logging. To use only for very big messages, otherwise
 * log() is to prefer. */
void logger::_log_row(int level, const char *msg) 
{
    if ((level&0xff) < _verbosity )
        return;
        
    const char *c = ".-*#";
    char buf[64];
    int rawmode = (level & LL_RAW);

    if (rawmode) {
        fprintf(_fp_log,"%s",msg);
    } else {
        int off;
        struct timeval tv;
        pid_t pid = getpid();

        gettimeofday(&tv,NULL);
        off = strftime(buf,sizeof(buf),"%d %b %H:%M:%S.",localtime(&tv.tv_sec));
        snprintf(buf+off,sizeof(buf)-off,"%03d",(int)tv.tv_usec/1000);
        fprintf(_fp_log,"%d: %s %c %s\n", static_cast<int>(pid), buf, c[level], msg);
    }
    
    fflush(_fp_log);
}

/* Like log_row() but with printf-alike support. This is the function that
 * is used across the code. The raw version is only used in order to dump
 * the INFO output on crash. */
void logger::log(int level, const char *fmt, ...) 
{
    if ((level&0xff) < _verbosity )
        return;
    
    va_list ap;
    char msg[LOG_MAX_LEN];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    
    logger::_log_row(level, msg);
}

void logger::mylog(int level, const char *file_name, int line, const char *fmt, ...) 
{
    if ((level&0xff) < _verbosity )
        return;
    
    va_list ap;
    char msg[LOG_MAX_LEN];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    size_t len = strlen(msg);
    snprintf(msg+len, sizeof(msg) - len, " --%s:%d", basename(const_cast<char *>(file_name)), line);
    
    logger::_log_row(level, msg);
}


/* Log a fixed message without printf-alike capabilities, in a way that is
 * safe to call from a signal handler.
 *
 * We actually use this only for signals that are not fatal from the point
 * of view of Redis. Signals that are going to kill the server anyway and
 * where we need printf-alike features are served by log(). */
void logger::log_from_handler(int level, const char *msg) 
{
    bool log_to_stdout = (_fp_log == stdout);
    int fd = log_to_stdout ? STDOUT_FILENO :
                         open(_log_file_name.c_str(), O_APPEND|O_CREAT|O_WRONLY, 0644);
                         
    if ((level&0xff) < _verbosity )
        return;

    char buf[64];
    
    if (fd == -1) return;
    ll2string(buf,sizeof(buf),getpid());
    if (write(fd,buf,strlen(buf)) == -1) goto err;
    if (write(fd,":signal-handler (",17) == -1) goto err;
    ll2string(buf,sizeof(buf),time(NULL));
    if (write(fd,buf,strlen(buf)) == -1) goto err;
    if (write(fd,") ",2) == -1) goto err;
    if (write(fd,msg,strlen(msg)) == -1) goto err;
    if (write(fd,"\n",1) == -1) goto err;
err:
    if (!log_to_stdout) close(fd);
}

