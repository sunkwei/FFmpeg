/*
 * Copyright (c) 2012 Stefano Sabatini
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 *  to load extension dynamic library filter
 */

#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/timestamp.h"
#include "avfilter.h"
#include "loadlib.h"
#include "internal.h"
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#define dlopen(fname, flags) LoadLibrary(fname)
#define dlclose(h) FreeLibrary(h)
#define dlsym(h, name) GetProcAddress(h, name)
#else
#include <dlfcn.h>
typedef void* HMODULE;
#endif /* _WIN32 */

typedef struct LoadLibContext {
    const AVClass *class;
    char *fname;    

    ExtContext ext;
    HMODULE mod;
    ll_init func_init;
    ll_uninit func_uninit;
    ll_filter func_filter;
    ll_query_format func_query_format;

} LoadLibContext;

#define OFFSET(x) offsetof(LoadLibContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption loadlib_options[] = {
    { "fname", "the full filename of module loading", OFFSET(fname), AV_OPT_TYPE_STRING, { .str = "" }, CHAR_MIN, CHAR_MAX, FLAGS },
    { NULL }
};

AVFILTER_DEFINE_CLASS(loadlib);

static int loadlib_init(AVFilterContext *ctx)
{
    LoadLibContext *priv = (LoadLibContext*)ctx->priv;
    av_log(ctx, AV_LOG_DEBUG, "to load %s\n", priv->fname);
    priv->mod = dlopen(priv->fname, RTLD_LAZY);
    if (!priv->mod) {
        av_log(ctx, AV_LOG_ERROR, "cannot load %s module\n", priv->fname);
        return AVERROR(errno);
    }

    priv->func_init = (ll_init)dlsym(priv->mod, "vf_init");
    priv->func_uninit = (ll_uninit)dlsym(priv->mod, "vf_uninit");
    priv->func_filter = (ll_filter)dlsym(priv->mod, "vf_filter");
    priv->func_query_format = (ll_query_format)dlsym(priv->mod, "vf_query_format");

    if (!priv->func_query_format || !priv->func_filter) {
        av_log(ctx, AV_LOG_ERROR, "the ext library %s MUST supply func of 'vf_query_format' and 'vf_filter'\n", priv->fname);
        return -1;
    }

    if (priv->func_init) {
        priv->ext.user_data = 0;
        return priv->func_init(ctx, &priv->ext);
    }

    return 0;
}

static void loadlib_uninit(AVFilterContext *ctx)
{
    LoadLibContext *priv = (LoadLibContext*)ctx->priv;
    if (priv->mod) {
        if (priv->func_uninit) {
            priv->func_uninit(ctx, &priv->ext);
        }
        dlclose(priv->mod);
        priv->mod = 0;
    }
}

static int query_formats(AVFilterContext *ctx)
{
    LoadLibContext *priv = (LoadLibContext*)ctx->priv;
    return priv->func_query_format(ctx, &priv->ext);
}

#define SET_META(key, value) \
    av_dict_set_int(metadata, key, value, 0);

static int filter_frame(AVFilterLink *inlink, AVFrame *frame)
{
    AVFilterContext *ctx = inlink->dst;
    LoadLibContext *priv = ctx->priv;
    return priv->func_filter(inlink, frame, &priv->ext);
}

static const AVFilterPad loadlib_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .filter_frame = filter_frame,
    },
    { NULL }
};

static const AVFilterPad loadlib_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_DATA,
    },
    { NULL }
};

AVFilter ff_vf_loadlib = {
    .name          = "loadlib",
    .description   = NULL_IF_CONFIG_SMALL("to load externsion library."),
    .priv_size     = sizeof(LoadLibContext),
    .priv_class    = &loadlib_class,
    .init          = loadlib_init,
    .uninit        = loadlib_uninit,
    .query_formats = query_formats,
    .inputs        = loadlib_inputs,
//    .outputs       = loadlib_outputs,
    .flags         = AVFILTER_FLAG_SUPPORT_TIMELINE_GENERIC,
};
