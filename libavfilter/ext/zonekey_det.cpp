#include <stdio.h>
#include <opencv2/opencv.hpp>
#include "../loadlib.h"
#include <cc++/thread.h>

class Context
{
public:
    explicit Context()
    {

    }
    ~Context()
    {

    }
};


/** XXX: 需要使用分析服务来完成分析，否则，每个 ffmpeg 进程都启动一个模型，会导致内存很快耗光
 *      因此将图片发送到“分析服务”分析，接收分析结果
 */



extern "C"
int vf_init(AVFilterContext *ctx, ExtContext *ext)
{
    ext->user_data = new Context();
    return 0;
}

extern "C"
void vf_uninit(AVFilterContext *ctx, ExtContext *ext)
{
    delete (Context*)ext->user_data;
}

extern "C"
int vf_query_format(AVFilterContext *ctx, ExtContext *ext)
{
    // 仅仅支持 BGR24
    AVFilterFormats *formats = 0;
    static const enum AVPixelFormat fmts[] = {
        AV_PIX_FMT_BGR24,
    };

    for (int i = 0; i < sizeof(fmts)/sizeof(AVPixelFormat); i++) {
        int ret = ff_add_format(&formats, fmts[i]);
        if (ret < 0) {
            return ret;
        }
    }

    int rc = ff_set_common_formats(ctx, formats);
    av_log(ctx, AV_LOG_ERROR, "%s:%d calling, %d\n", __func__, __LINE__, rc);
    return 0;
}

extern "C"
int vf_filter(AVFilterLink *inlink, AVFrame *frame, ExtContext *ext)
{
    AVFilterContext *src = inlink->src, *me = inlink->dst;
    av_log(me, AV_LOG_ERROR, "%s:%d calling width=%d, height=%d, fmt=%d\n", 
            __func__, __LINE__, frame->width, frame->height, frame->format);

    return 0;
}
