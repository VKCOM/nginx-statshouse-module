/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <ngx_core.h>

#include "ngx_statshouse_tl.h"
#include "ngx_statshouse_stat.h"


#define NGX_STATSHOUSE_TL_TINY_STRING_LEN    253
#define NGX_STATSHOUSE_TL_BIG_STRING_MARKER  0xfe
#define NGX_STATSHOUSE_TL_TAG                0x56580239


static size_t  ngx_statshouse_tl_string_padding(const ngx_str_t *str);
static size_t  ngx_statshouse_tl_string_len(const ngx_str_t *str);
static void  ngx_statshouse_tl_string(ngx_buf_t *buf, const ngx_str_t *str);

static size_t  ngx_statshouse_tl_int32_len();
static void  ngx_statshouse_tl_int32(ngx_buf_t *buf, int32_t n);
static size_t  ngx_statshouse_tl_uint32_len();
static void  ngx_statshouse_tl_uint32(ngx_buf_t *buf, uint32_t n);
static size_t  ngx_statshouse_tl_int64_len();
static void  ngx_statshouse_tl_int64(ngx_buf_t *buf, int64_t n);
static size_t  ngx_statshouse_tl_double_len();
static void  ngx_statshouse_tl_double(ngx_buf_t *buf, double n);

static size_t  ngx_statshouse_tl_metric_len(const ngx_statshouse_stat_t *stat);
static void  ngx_statshouse_tl_metric(ngx_buf_t *buf, const ngx_statshouse_stat_t *stat);


static size_t
ngx_statshouse_tl_string_padding(const ngx_str_t *str)
{
    size_t  len;

    len = str->len;
    if (len <= NGX_STATSHOUSE_TL_TINY_STRING_LEN) {
        len++;
    }

    return -len % 4;
}


static size_t
ngx_statshouse_tl_string_len(const ngx_str_t *str)
{
    size_t  len;

    if (str->len <= NGX_STATSHOUSE_TL_TINY_STRING_LEN) {
        len = 1;
    } else {
        len = 4;
    }

    len += str->len;
    len += ngx_statshouse_tl_string_padding(str);

    return len;
}


static void
ngx_statshouse_tl_string(ngx_buf_t *buf, const ngx_str_t *str)
{
    static u_char  padding[sizeof(uint32_t)];
    size_t         padding_len;

    if (str->len <= NGX_STATSHOUSE_TL_TINY_STRING_LEN) {
        buf->last[0] = (uint8_t) (str->len);
        buf->last += 1;

    } else {
        buf->last[0] = (uint8_t) NGX_STATSHOUSE_TL_BIG_STRING_MARKER;
        buf->last[1] = (uint8_t) (str->len);
        buf->last[2] = (uint8_t) (str->len >> 8);
        buf->last[3] = (uint8_t) (str->len >> 16);

        buf->last += 4;
    }


    buf->last = ngx_cpymem(buf->last, str->data, str->len);

    padding_len = ngx_statshouse_tl_string_padding(str);
    if (padding_len > 0) {
        buf->last = ngx_cpymem(buf->last, padding, padding_len);
    }
}


static size_t
ngx_statshouse_tl_int32_len()
{
    return sizeof(int32_t);
}


static void
ngx_statshouse_tl_int32(ngx_buf_t *buf, int32_t n)
{
    buf->last = ngx_cpymem(buf->last, &n, sizeof(n));
}


static size_t
ngx_statshouse_tl_uint32_len()
{
    return sizeof(uint32_t);
}


static void
ngx_statshouse_tl_uint32(ngx_buf_t *buf, uint32_t n)
{
    buf->last = ngx_cpymem(buf->last, &n, sizeof(n));
}


static size_t
ngx_statshouse_tl_int64_len()
{
    return sizeof(int64_t);
}


static void
ngx_statshouse_tl_int64(ngx_buf_t *buf, int64_t n)
{
    buf->last = ngx_cpymem(buf->last, &n, sizeof(n));
}


static size_t
ngx_statshouse_tl_double_len()
{
    return sizeof(double);
}


static void
ngx_statshouse_tl_double(ngx_buf_t *buf, double n)
{
    buf->last = ngx_cpymem(buf->last, &n, sizeof(n));
}


static size_t
ngx_statshouse_tl_metric_len(const ngx_statshouse_stat_t *stat)
{
    size_t     len;
    ngx_int_t  i;

    len = ngx_statshouse_tl_int32_len(); // fieldmask
    len += ngx_statshouse_tl_string_len(&stat->name); // stat name

    len += ngx_statshouse_tl_uint32_len(); // keys count
    for (i = 0; i < stat->keys_count; i++) {
        len += ngx_statshouse_tl_string_len(&stat->keys[i].name);
        len += ngx_statshouse_tl_string_len(&stat->keys[i].value);
    }

    switch (stat->type) {
        case ngx_statshouse_mt_counter:
            len += ngx_statshouse_tl_double_len();
            break;

        case ngx_statshouse_mt_value:
            len += ngx_statshouse_tl_uint32_len(); // values count
            len += ngx_statshouse_tl_double_len() * stat->values_count; // values
            break;

        case ngx_statshouse_mt_unique:
            len += ngx_statshouse_tl_uint32_len(); // unique count
            len += ngx_statshouse_tl_int64_len() * stat->values_count; // unique
            break;
    }

    return len;
}


static void
ngx_statshouse_tl_metric(ngx_buf_t *buf, const ngx_statshouse_stat_t *stat)
{
    uint32_t   field_mask = 0;
    ngx_int_t  i;

    switch (stat->type) {
        case ngx_statshouse_mt_counter:
            field_mask |= (1 << 0);
            break;

        case ngx_statshouse_mt_value:
            field_mask |= (1 << 1);
            break;

        case ngx_statshouse_mt_unique:
            field_mask |= (1 << 2);
            break;
    }

    ngx_statshouse_tl_int32(buf, field_mask);
    ngx_statshouse_tl_string(buf, &stat->name);

    ngx_statshouse_tl_uint32(buf, stat->keys_count);
    for (i = 0; i < stat->keys_count; i++) {
        ngx_statshouse_tl_string(buf, &stat->keys[i].name);
        ngx_statshouse_tl_string(buf, &stat->keys[i].value);
    }

    switch (stat->type) {
        case ngx_statshouse_mt_counter:
            ngx_statshouse_tl_double(buf, stat->values[0].counter);
            break;

        case ngx_statshouse_mt_value:
            ngx_statshouse_tl_uint32(buf, stat->values_count);

            for (i = 0; i < stat->values_count; i++) {
                ngx_statshouse_tl_double(buf, stat->values[i].value);
            }
            break;

        case ngx_statshouse_mt_unique:
            ngx_statshouse_tl_uint32(buf, 1);
            for (i = 0; i < stat->values_count; i++) {
                ngx_statshouse_tl_int64(buf, stat->values[i].unique);
            }
            break;
    }
}


size_t
ngx_statshouse_tl_metrics_len(const ngx_statshouse_stat_t *stat, ngx_int_t count)
{
    size_t     len;
    ngx_int_t  i;

    len = ngx_statshouse_tl_uint32_len(); // tag
    len += ngx_statshouse_tl_uint32_len(); // field mask
    len += ngx_statshouse_tl_uint32_len(); // count
    for (i = 0; i < count; i++) {
        len += ngx_statshouse_tl_metric_len(&stat[i]);
    }

    return len;
}


void
ngx_statshouse_tl_metrics(ngx_buf_t *buf, const ngx_statshouse_stat_t *stat, ngx_int_t count)
{
    ngx_int_t  i;

    ngx_statshouse_tl_uint32(buf, NGX_STATSHOUSE_TL_TAG);
    ngx_statshouse_tl_uint32(buf, 0); // field mask
    ngx_statshouse_tl_uint32(buf, count);

    for (i = 0; i < count; i++) {
        ngx_statshouse_tl_metric(buf, &stat[i]);
    }
}
