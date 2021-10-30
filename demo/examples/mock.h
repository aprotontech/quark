
#ifndef _QUARK_MOCK_H_
#define _QUARK_MOCK_H_

#include "quark/quark.h"

#define VD_TAG "VD"

#define RC_EXCEPT_SUCCESS(expr) { \
    int __err = (expr); \
    if (__err != 0) { \
        LOGW(VD_TAG, "check expr=%s, failed with error=%d", #expr, __err); \
        return __err; \
    } \
}



#endif