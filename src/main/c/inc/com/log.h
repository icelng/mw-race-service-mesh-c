

#include "syslog.h"

#define log_emerg(fmt, args...) \
    do{ \
        if(log_get_level() >= LOG_EMERG){ \
            syslog(LOG_EMERG, fmt, ##args); \
        } \
    }while(0)

#define log_alert(fmt, args...) \
    do{ \
        if(log_get_level() >= LOG_ALERT){ \
            syslog(LOG_ALERT, fmt, ##args); \
        } \
    }while(0)

#define log_err(fmt, args...) \
    do{ \
        if(log_get_level() >= LOG_ERR){ \
            syslog(LOG_ERR, fmt, ##args); \
        } \
    }while(0)

#define log_warning(fmt, args...) \
    do{ \
        if(log_get_level() >= LOG_WARNING){ \
            syslog(LOG_WARNING, fmt, ##args); \
        } \
    }while(0)

#define log_notice(fmt, args...) \
    do{ \
        if(log_get_level() >= LOG_NOTICE){ \
            syslog(LOG_NOTICE, fmt, ##args); \
        } \
    }while(0)

#define log_info(fmt, args...) \
    do{ \
        if(log_get_level() >= LOG_INFO){ \
            syslog(LOG_INFO, fmt, ##args); \
        } \
    }while(0)

#define log_debug(fmt, args...) \
    do{ \
        if(log_get_level() >= LOG_DEBUG){ \
            syslog(LOG_DEBUG, fmt, ##args); \
        } \
    }while(0)

int log_init(int level, const char *log_ident, const char *log_file_path);
int log_out(int level, char *fmt, ...);
int log_set_level(int level);
int log_get_level();
