#ifndef CONFIG_H
#define CONFIG_H

#include <linux/types.h>

/* Helper macro to get environment variable or use default */
#define GET_ENV_INT(env_var, default_val) \
    ({ \
        char *env_str = getenv(#env_var); \
        int val = (default_val); \
        if (env_str) { \
            val = simple_strtol(env_str, NULL, 10); \
        } \
        val; \
    })

#define GET_ENV_STR(env_var, default_val) \
    ({ \
        char *env_str = getenv(#env_var); \
        const char *val = (default_val); \
        if (env_str) { \
            val = env_str; \
        } \
        val; \
    })

/* Configuration parameters with environment variable support */
#define DEVICE_NAME "price_feed"
#define CLASS_NAME "price_class"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 4096
#endif

#ifndef MAX_TICKERS
#define MAX_TICKERS 256
#endif

#ifndef TICKER_LEN
#define TICKER_LEN 8
#endif

#ifndef PRICE_ENTRY_SIZE
#define PRICE_ENTRY_SIZE 12
#endif

#ifndef CONFIG_PATH
#define CONFIG_PATH "/etc/price_driver/config.csv"
#endif

#ifndef MAX_LINE_LEN
#define MAX_LINE_LEN 256
#endif

#ifndef MAX_DELTA_PERCENT
#define MAX_DELTA_PERCENT 5
#endif

#ifndef GENERATION_TIMEOUT_MS
#define GENERATION_TIMEOUT_MS 100
#endif

#endif /* CONFIG_H */