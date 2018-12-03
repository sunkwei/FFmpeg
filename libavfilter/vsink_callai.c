#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/avstring.h"
#include "libavcodec/avcodec.h"
#include "libavformat/url.h"
#include "avfilter.h"
#include "internal.h"
#include <sys/types.h>
#include <unistd.h>

typedef struct AIResultResponse {
    int status_code;
    int content_length;
    char *body;
} AIResultResponse;

typedef struct CallAIContext {
    const AVClass *class;

    char *token;    // 唯一标识，用于区分不同 ffmpeg 的数据 ...

    char *aiurl;  // unix socket name，default "/tmp/zonekey-aisrv.socket"
    int width, height; // the resolution for ai detection

    char *result_srv_url;   // 分析结果提交到指定的服务器 ...

    AVCodecContext *enc_ctx;  // AVFrame 压缩为 mjpeg 之后，再 post 到 aisrv
    AVCodec *enc;
    AVPacket pkg;       // for encoded data

    AIResultResponse ai_res;

} CallAIContext;

#define OFFSET(x) offsetof(CallAIContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_FILTERING_PARAM

static const AVOption callai_options[] = {
    { "token", "the token id for instance", OFFSET(token), AV_OPT_TYPE_STRING, {.str=""}, CHAR_MIN, CHAR_MAX, FLAGS},
    { "aiurl", "the url(post) of AI server", OFFSET(aiurl), AV_OPT_TYPE_STRING, {.str="http://localhost:9901/post_img"}, CHAR_MIN, CHAR_MAX, FLAGS },
    { "rsurl", "the url(post) of Result server", OFFSET(result_srv_url), AV_OPT_TYPE_STRING, {.str="http://localhost:9902/post_result"}, CHAR_MIN, CHAR_MAX, FLAGS },
    { "width", "the width of image for AI detection", OFFSET(width), AV_OPT_TYPE_INT, {.i64=1920 }, 0, 3840, FLAGS },
    { "height", "the height of image for AI detection", OFFSET(height), AV_OPT_TYPE_INT, {.i64=1080 }, 0, 2160, FLAGS },
    { NULL },
};

AVFILTER_DEFINE_CLASS(callai);

static int init(AVFilterContext *ctx)
{
    CallAIContext *priv = ctx->priv;

    priv->ai_res.body = 0;
    priv->ai_res.content_length = 0;

    // aisrv 必须为 http: 前缀
    size_t prefix_len = strspn(priv->aiurl, "htp");
    if (strncmp("http", priv->aiurl, prefix_len)) {
        av_log(ctx, AV_LOG_ERROR, "unsupported aiurl: %s\n", priv->aiurl);
        return -1;
    }

    // rsurl 必须为 http: 前缀
    prefix_len = strspn(priv->result_srv_url, "htp");
    if (strncmp("http", priv->result_srv_url, prefix_len)) {
        av_log(ctx, AV_LOG_ERROR, "unsupported rsurl: %s\n", priv->result_srv_url);
        return -1;
    }

    // token，如果没有指定，则生成： "g-%d", getpid()
    if (strlen(priv->token) == 0) {
        char token[64];
        snprintf(token, sizeof(token), "g-%d", getpid());
        priv->token = av_strdup(token);
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

    av_freep(priv->token);


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

    av_log(ctx, AV_LOG_INFO, "frame pts: %lld, pkg pts: %lld, pkg size: %d\n", frame->pts, ctx->pkg.dts, ctx->pkg.size);

    return 0;
}

static int save_ai_result(CallAIContext *ctx)
{
    // 此时 ctx->ai_res.body 中为分析结果，json 格式
    return 0;
}

/* 以下代码模仿 http.c */
static int http_get_line(URLContext *s, char *line, int line_size)
{
    int ch;
    char *q;

    q = line;
    for (;;) {
        int rc = ffurl_read(s, &ch, 1);
        if (rc < 0) {
            av_log(s, AV_LOG_ERROR, "ffurl_read err, %s\n", av_err2str(rc));
            return rc;
        }
        if (ch == '\n') {
            if (q > line && q[-1] == '\r')
                q--;
            *q = '\0';

            return 0;
        }
        else {
            if ((q - line) < line_size - 1) {
                *q++ = ch;
            }
        }
    }
}

static int process_header(CallAIContext *ctx, const char *line)
{
    if (strlen(line) == 0) {
        return 0;   // 空行，结束 ...
    }

    if (ctx->ai_res.status_code == -1) {
        // 解析 HTTP/1.1 200 OK
        char *p = line, *end;
        while (!av_isspace(*p) && *p) p++; // 跳过 HTTP/1.1
        while (av_isspace(*p) && *p) p++;   // 跳过 200 之前的空格
        ctx->ai_res.status_code = strtol(p, &end, 10);
        if (ctx->ai_res.status_code != 200) {
            av_log(ctx, AV_LOG_ERROR, "process_header: status code=%d\n", ctx->ai_res.status_code);
            return -1;
        }
    }
    else {
        // key: value
        char *cpos = strchr(line, ':');
        char *key, *value;
        if (!cpos) {
            return 1;
        }

        *cpos = 0;
        key = line;
        cpos++;
        while (av_isspace(*cpos) && *cpos) cpos++; // 跳过 : 之后的空格
        value = cpos;

        if (!av_strcasecmp(key, "Content-Length")) {
            ctx->ai_res.content_length = strtol(value, 0, 10);
        }
    }

    return 1;
}

static int http_read_headers(CallAIContext *ctx, URLContext *s)
{
    char line[1024];
    av_log(ctx, AV_LOG_INFO, "========== http_read_headers ======\n");
    for (;;) {
        int rc = http_get_line(s, line, sizeof(line));
        if (rc < 0)
            return rc;
        
        av_log(ctx, AV_LOG_INFO, "header='%s'\n", line);

        rc = process_header(ctx, line);
        if (rc == 0) {
            // 找到空行，结束 header
            break;
        }
        else if (rc < 0) {
            // 错误！
            return -1;
        }
    }

    return 0;
}

/* 以上代码模仿 http.c */

static void aires_reset(AIResultResponse *res)
{
    res->status_code = -1;  // 等待解析 start line
    res->content_length = 0;
    if (res->body)
        av_freep(res->body);
}

static int get_ai_result(CallAIContext *ctx)
{
    // en, the mjpeg data in ctx->pkg
    // 此处使用 http POST 发送 jpg，接收分析结果 ...
    URLContext *url = 0;
    AVDictionary *ops = 0;
    int rc;
    char headers[128];

    snprintf(headers, sizeof(headers), "Content-Length:%d\r\nContent-Type:%s\r\n", ctx->pkg.size, "image/jpeg");
    av_dict_set(&ops, "headers", headers, 0);
    av_dict_set(&ops, "chunked_post", "0", 0);
    //av_dict_set(&ops, "send_expect_100", "1", 0);
    av_dict_set(&ops, "icy", "0", 0);

    rc = ffurl_open(&url, ctx->aiurl, AVIO_FLAG_READ_WRITE, 0, &ops);
    av_dict_free(&ops);

    if (rc < 0) {
        av_log(ctx, AV_LOG_ERROR, "cannot open ai url: %s\n", ctx->aiurl);
        goto done;
    }

    rc = ffurl_write(url, ctx->pkg.data, ctx->pkg.size);

    aires_reset(&ctx->ai_res);

    // 这里解析 ai srv 返回
    rc = http_read_headers(ctx, url);
    if (rc < 0) {
        av_log(ctx, AV_LOG_ERROR, "cannot get AI results\n");
        goto done;
    }

    // save body
    if (ctx->ai_res.content_length > 0) {
        ctx->ai_res.body = av_mallocz(ctx->ai_res.content_length);
        rc = ffurl_read_complete(url, ctx->ai_res.body, ctx->ai_res.content_length);
    }

done:

    ffurl_close(url);
    av_packet_unref(&ctx->pkg);
    return rc;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *frame)
{
    AVFilterContext *ctx = inlink->dst;
    CallAIContext *priv = ctx->priv;
    av_log(ctx, AV_LOG_INFO, "width: %d, height: %d, fmt: %d\n", frame->width, frame->height, frame->format);

    if (encode_frame(priv, frame) >= 0) {
        if (get_ai_result(priv) >= 0) {
            save_ai_result(priv);
        }
    }

    // as sink
    av_frame_free(&frame);

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
