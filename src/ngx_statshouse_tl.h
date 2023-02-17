/* Copyright 2022 V Kontakte LLC
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _NGX_STATSHOUSE_TL_H_INCLUDED_
#define _NGX_STATSHOUSE_TL_H_INCLUDED_

#include <ngx_core.h>
#include <ngx_statshouse_stat.h>


size_t  ngx_statshouse_tl_metrics_len(const ngx_statshouse_stat_t *stat, ngx_int_t count);
void  ngx_statshouse_tl_metrics(ngx_buf_t *buf, const ngx_statshouse_stat_t *stat, ngx_int_t count);


#endif
