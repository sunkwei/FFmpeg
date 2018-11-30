#ifndef AVFILTER_LOADLIB_H
#define AVFILTER_LOADLIB_H

#ifdef __cplusplus
extern "C" {
#endif /* c++ */

#include "avfilter.h"
#include "internal.h"

typedef struct ExtContext
{
    void *user_data;

} ExtContext;

typedef int (*ll_init)(AVFilterContext *ctx, ExtContext *ext);
typedef void (*ll_uninit)(AVFilterContext *ctx, ExtContext *ext);
typedef int (*ll_filter)(AVFilterLink *inlink, AVFrame *frame, ExtContext *ext);
typedef int (*ll_query_format)(AVFilterContext *ctx, ExtContext *ext);


#ifdef __cplusplus
}
#endif /* c++ */

#endif /* AVFILTER_LOADLIB_H */
