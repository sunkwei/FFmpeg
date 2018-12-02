#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/avcodec.h"
#include "libavformat/url.h"
#include "avfilter.h"
#include "internal.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct CallAIContext {
    const AVClass *class;

    char *aiurl;  // unix socket name，default "/tmp/zonekey-aisrv.socket"
    int width, height; // the resolution for ai detection

    AVCodecContext *enc_ctx;  // AVFrame 压缩为 mjpeg 之后，再 post 到 aisrv
    AVCodec *enc;
    AVPacket pkg;       // for encoded data

} CallAIContext;

#define OFFSET(x) offsetof(CallAIContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_FILTERING_PARAM

static const AVOption callai_options[] = {
    { "aiurl", "the url(post) of AI srv", OFFSET(aiurl), AV_OPT_TYPE_STRING, {.str="http://localhost:9901/post_img"}, CHAR_MIN, CHAR_MAX, FLAGS },
    { "width", "the width of image for AI detection", OFFSET(width), AV_OPT_TYPE_INT, {.i64=1920 }, 0, 3840, FLAGS },
    { "height", "the height of image for AI detection", OFFSET(height), AV_OPT_TYPE_INT, {.i64=1080 }, 0, 2160, FLAGS },
    { NULL },
};

AVFILTER_DEFINE_CLASS(callai);

static int init(AVFilterContext *ctx)
{
    CallAIContext *priv = ctx->priv;

    // aisrv 必须为 http: 前缀
    size_t prefix_len = strspn(priv->aiurl, "htp");
    if (strncmp("http", priv->aiurl, prefix_len)) {
        av_log(ctx, AV_LOG_ERROR, "unsupported aiurl: %s\n", priv->aiurl);
        return -1;
    }

    // 实例化 jpeg encoder
    priv->enc = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!priv->enc) {
        av_log(ctx, AV_LOG_ERROR, "cannot find AV_CODEC_ID_MJPEG encoder!!!!\n");
        return -1;
    }
    priv->enc_ctx = avcodec_alloc_context3(priv->enc);
    priv->enc_ctx->width = priv->width;
    priv->enc_ctx->height = priv->height;
    priv->enc_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    priv->enc_ctx->time_base = (AVRational){1, 1};
    priv->enc_ctx->gop_size = 0;
    priv->enc_ctx->i_quant_factor = 10;

    avcodec_open2(priv->enc_ctx, priv->enc, NULL);

    av_init_packet(&priv->pkg);

    return 0;
}

static void uninit(AVFilterContext *ctx)
{
    CallAIContext *priv = ctx->priv;
    // 释放 jpeg encoder
    if (priv->enc_ctx) {
        avcodec_free_context(&priv->enc_ctx);
    }

    if (priv->enc) {
        avcodec_close(priv->enc);
        priv->enc = 0;
    }

    return 0;
}

static int query_formats(AVFilterContext *ctx)
{
    // 仅仅支持 YUVJ420P
    AVFilterFormats *formats = 0;
    ff_add_format(&formats, AV_PIX_FMT_YUVJ420P);
    return ff_set_common_formats(ctx, formats);
}

static int encode_frame(CallAIContext *ctx, AVFrame *frame)
{
    int rc = avcodec_send_frame(ctx->enc_ctx, frame);
    if (rc < 0) {
        av_log(ctx, AV_LOG_ERROR, "avcodec_send_frame !!\n");
        return rc;
    }

    rc = avcodec_receive_packet(ctx->enc_ctx, &ctx->pkg);
    if (rc < 0) {
        av_log(ctx, AV_LOG_ERROR, "avcodec_receive_package !!\n");
        return rc;
    }

    av_log(ctx, AV_LOG_INFO, "frame pts: %lld, pkg pts: %lld\n", frame->pts, ctx->pkg.dts);

    return 0;
}

static int get_ai_result(CallAIContext *ctx)
{
    // en, the mjpeg data in ctx->pkg
    // 此处使用 http POST 发送 jpg，接收分析结果 ...
    URLContext *url = 0;

    AVDictionary *ops = 0;

    char headers[128];
    snprintf(headers, sizeof(headers), "Content-Length:%d\r\nContent-Type:%s\r\n", ctx->pkg.size, "image/jpeg");
    av_dict_set(&ops, "headers", headers, 0);
    int rc = ffurl_open(&url, ctx->aiurl, AVIO_FLAG_WRITE, 0, &ops);
    av_dict_free(&ops);

    rc = ffurl_write(url, ctx->pkg.data, ctx->pkg.size);

    char results[1024]; // ???
    rc = ffurl_read(url, results, sizeof(results)-1);
    if (rc < 0) {
        av_log(ctx, AV_LOG_ERROR, "ffurl_read err???\n");
        return rc;
    }
    results[rc] = 0;
    av_log(ctx, AV_LOG_INFO, "got: %s\n", results);

    ffurl_close(url);
    av_packet_unref(&ctx->pkg);
    return 0;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *frame)
{
    AVFilterContext *ctx = inlink->dst;
    CallAIContext *priv = ctx->priv;
    av_log(ctx, AV_LOG_INFO, "width: %d, height: %d, fmt: %d\n", frame->width, frame->height, frame->format);

    if (encode_frame(priv, frame) >= 0) {
        get_ai_result(priv);
    } 

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
    .init = init,
    .uninit = uninit,
    .query_formats = query_formats,
    .inputs = callai_inputs,
};
