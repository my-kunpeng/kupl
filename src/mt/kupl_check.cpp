/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
 *
 * KUPL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *        http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "kupl_check.h"
#include <climits>
#include "utils/debug/kupl_log.h"
#include "utils/sys/kupl_math.h"
#include "utils/sys/kupl_compiler.h"

#define KUPL_MIN_CHUNK 1
#define KUPL_MAX_CHUNK (INT64_MAX - 1)

int kupl_check_range(kupl_nd_range_t *range, kupl_loop_policy_type_t policy)
{
    int dim = range->dim;
    if (kupl_unlikely((dim < 1) || (dim > KUPL_MAX_DIM_SIZE))) {
        return kupl_log_error_return(ERROR, "Invalid range dim");
    }

    // check the total chunks do not overflow INT_MAX
    int64_t remain_chunks = KUPL_MAX_CHUNK;
    for (int i = 0; i < dim; i++) {
        int64_t lower = range->nd_range[i].lower;
        int64_t upper = range->nd_range[i].upper;
        int64_t step = range->nd_range[i].step;
        int64_t chunksize = 1;
        if (kupl_unlikely((upper <= lower && step > 0) || (upper >= lower && step < 0) || (step == 0))) {
            return kupl_log_error_return(WARN, "the range input is not support!");
        }
        switch (policy) {
            case KUPL_LOOP_POLICY_STATIC:
                // POLICY_STATIC do not care about blocksize currently
                chunksize = step;
                range->nd_range[i].blocksize = 1;
                break;
            case KUPL_LOOP_POLICY_DYNAMIC:
            case KUPL_LOOP_POLICY_TASK:
            default:
                // replace the default blocksize with the real blocksize
                if (kupl_likely(range->nd_range[i].blocksize == 0)) {
                    range->nd_range[i].blocksize = 1;
                }
                int64_t blocksize = range->nd_range[i].blocksize;
                if (kupl_unlikely((step > 0 && KUPL_MAX_CHUNK / blocksize < step)) ||
                    (step < 0 && KUPL_MAX_CHUNK / blocksize < -step)) {
                    return kupl_log_error_return(ERROR, "the range input is not support! blocksize: %ld", blocksize);
                }
                chunksize = blocksize * step;
                if (kupl_unlikely((chunksize > 0 && INT64_MAX - chunksize < upper) ||
                                  (chunksize < 0 && INT64_MIN - chunksize > upper))) {
                    return kupl_log_error_return(ERROR, "the range input is not support! blocksize: %ld", blocksize);
                }
        }
        remain_chunks /= kupl_divup(upper - lower, chunksize);
    }
    if (kupl_unlikely(remain_chunks == 0)) {
        return kupl_log_error_return(ERROR, "The total blocks is out of range!");
    }
    return KUPL_OK;
}
