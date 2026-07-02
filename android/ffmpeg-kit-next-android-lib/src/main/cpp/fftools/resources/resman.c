/*
 * Original FFmpeg source:
 * Copyright (c) 2025 - softworkz
 *
 * FFmpegKitNext modifications:
 * Copyright (c) 2026 Taner Sener
 *
 * This modified file is part of FFmpegKitNext.
 * It is derived from FFmpeg's fftools/resources/resman.c at tag n8.1.2.
 *
 * The original FFmpeg source is licensed under the GNU Lesser General
 * Public License version 2.1 or later. FFmpegKitNext distributes this
 * modified file under the GNU Lesser General Public License version 3 or
 * later, as permitted by that original "or later" license.
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with FFmpegKitNext. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modification history:
 *
 * ffmpeg-kit changes by Taner Sener
 *
 * 07.2026
 * --------------------------------------------------------
 * - FFmpeg 8.1.2 resource manager changes migrated
 * - Uncompressed generated resources preserved for the flat platform build
 */

/**
 * @file
 * output writers for filtergraph details
 */

#include "config.h"

#include <string.h>


#include "resman.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavutil/dict.h"
#include "libavutil/common.h"

extern const unsigned char ff_graph_html_data[];
extern const unsigned int ff_graph_html_len;

extern const unsigned char ff_graph_css_data[];
extern const unsigned ff_graph_css_len;

static const FFResourceDefinition resource_definitions[] = {
    [FF_RESOURCE_GRAPH_CSS]   = { FF_RESOURCE_GRAPH_CSS,   "graph.css",   &ff_graph_css_data[0],   &ff_graph_css_len   },
    [FF_RESOURCE_GRAPH_HTML]  = { FF_RESOURCE_GRAPH_HTML,  "graph.html",  &ff_graph_html_data[0],  &ff_graph_html_len  },
};


static const AVClass resman_class = {
    .class_name = "ResourceManager",
};

typedef struct ResourceManagerContext {
    const AVClass *class;
    AVDictionary *resource_dic;
} ResourceManagerContext;

static AVMutex mutex = AV_MUTEX_INITIALIZER;

static ResourceManagerContext resman_ctx = { .class = &resman_class };


void ff_resman_uninit(void)
{
    ff_mutex_lock(&mutex);

    av_dict_free(&resman_ctx.resource_dic);

    ff_mutex_unlock(&mutex);
}


char *ff_resman_get_string(FFResourceId resource_id)
{
    ResourceManagerContext *ctx = &resman_ctx;
    FFResourceDefinition resource_definition = { 0 };
    AVDictionaryEntry *dic_entry;
    char *res = NULL;

    for (unsigned i = 0; i < FF_ARRAY_ELEMS(resource_definitions); ++i) {
        FFResourceDefinition def = resource_definitions[i];
        if (def.resource_id == resource_id) {
            resource_definition = def;
            break;
        }
    }

    av_assert1(resource_definition.name);

    ff_mutex_lock(&mutex);

    dic_entry = av_dict_get(ctx->resource_dic, resource_definition.name, NULL, 0);

    if (!dic_entry) {
        int dict_ret;

        dict_ret = av_dict_set(&ctx->resource_dic, resource_definition.name, (const char *)resource_definition.data, 0);
        if (dict_ret < 0) {
            av_log(ctx, AV_LOG_ERROR, "Failed to store resource in dictionary: %d\n", dict_ret);
            goto end;
        }

        dic_entry = av_dict_get(ctx->resource_dic, resource_definition.name, NULL, 0);

        if (!dic_entry) {
            av_log(ctx, AV_LOG_ERROR, "Failed to retrieve resource from dictionary after storing it\n");
            goto end;
        }
    }

    res = dic_entry->value;

end:
    ff_mutex_unlock(&mutex);
    return res;
}
