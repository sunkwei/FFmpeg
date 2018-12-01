#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "avfilter.h"
#include "internal.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct CallAIContext {
    const AVClass *class;

    char *uxsname;  // unix socket name，default "/tmp/zonekey-aisrv.socket"

} CallAIContext;

#define OFFSET(x) offsetof(CallAIContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_FILTERING_PARAM

static const AVOption callai_options[] = {
    { "uxsname", "the AF_UNIX sockaddr name, default /tmp/zonekey-aisrv.socket", OFFSET(uxsname), AV_OPT_TYPE_STRING, {.str="/tmp/zonekey-aisrv.socket"}, CHAR_MIN, CHAR_MAX, FLAGS },
    { NULL },
};

AVFILTER_DEFINE_CLASS(callai);

static int query_formats(AVFilterContext *ctx)
{
    // 仅仅处理 BGR24 格式
    AVFilterFormats *formats = 0;
    ff_add_format(&formats, AV_PIX_FMT_BGR24);
    return ff_set_common_formats(ctx, formats);
}

static int filter_frame(AVFilterLink *inlink, AVFrame *frame)
{
    AVFilterContext *ctx = inlink->dst;
    CallAIContext *priv = ctx->priv;
    av_log(ctx, AV_LOG_INFO, "width: %d, height: %d, fmt: %d\n", frame->width, frame->height, frame->format);
    return 0;
}

static const AVFilterPad callai_inputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
        .filter_frame = filter_frame,
    },
    { NULL },
};

AVFilter ff_vsink_callai = {
    .name = "callai",
    .description = NULL_IF_CONFIG_SMALL("video sink for calling AI srv"),
    .priv_size = sizeof(CallAIContext),
    .priv_class = &callai_class,
    .query_formats = query_formats,
    .inputs = callai_inputs,
};
