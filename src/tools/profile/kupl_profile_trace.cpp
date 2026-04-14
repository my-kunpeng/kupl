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
#include "kupl_profile_trace.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include "kupl.h"
#include "utils/time/kupl_time.h"
#include "utils/arch/kupl_atomic.h"
#include "utils/sys/kupl_hardware.h"
#include "utils/sys/kupl_compiler.h"
#include "utils/config/kupl_config.h"
#include "utils/debug/kupl_log.h"
#include "tools/struct/kupl_vla.h"

#define MAX_LENGTH_FILENAME 200
#define FLUSH_BUF_SIZE 1048576 // 1M
#define REMOVE_LAST_COMMA_FLAG (-2)
#define PTRACE_PATH_SIZE 100
#define TIME_BUF_SIZE 30

#ifdef ENABLE_KUPL_TRACE
struct profile_trace_event {
    const char* name = nullptr;
    uint64_t pid{};
    uint64_t tid{};
    uint64_t start{};
    uint64_t duration{};
};
// declear static funtion
static void destroy_executor_buffer();
static void outputstream_close();

static uint64_t g_ptrace_ts;
static char ptrace_path[PTRACE_PATH_SIZE] = {};
static int ptrace_executor_buffersize = 26214;
static profile_trace_event** trace_event = nullptr;
static int* executor_event_num = nullptr;
static FILE** executor_outputstream = nullptr;
static int ptrace_executor_count = 0;
static char** filepath = nullptr;
static int kupl_prof_level_trace;

int get_kupl_prof_level_trace()
{
    return kupl_prof_level_trace;
}

static int outputstream_open()
{
    const mode_t mk_mode = S_IRWXU | S_IRWXG | S_IRWXO;
    executor_outputstream = (FILE**)calloc((size_t)ptrace_executor_count, sizeof(FILE*));
    if (executor_outputstream == nullptr) {
        outputstream_close();
        return kupl_log_error_return(ERROR, "error in executor_outputstream malloc");
    }

    filepath = (char**)calloc((size_t)ptrace_executor_count, sizeof(char*));  // allocte filepath list memory
    if (filepath == nullptr) {
        outputstream_close();
        return kupl_log_error_return(ERROR, "error in filepath malloc");
    }

    if (mkdir(ptrace_path, mk_mode&(~umask(0))) == 0) {     // success create directory
        kupl_debug("Directory created");
    } else if (errno == EEXIST) {                            // multiple processes create a same directory
        kupl_debug("Directory already exists");
    } else {                                                // other error
        outputstream_close();
        return kupl_log_error_return(ERROR, "Directory create failed");
    }

    for (int eid = 0; eid < ptrace_executor_count; eid++) {
        filepath[eid] =
        (char*)malloc(sizeof(char) * MAX_LENGTH_FILENAME);  // allocte filepath memory one by one
        if (filepath[eid] == nullptr) {
            outputstream_close();
            kupl_log_error_return(ERROR, "error in filepath malloc");
        }
        sprintf(filepath[eid], "%sp_%d_e_%d", ptrace_path, getpid(), eid);  // assign filepath name
        executor_outputstream[eid] = fopen(filepath[eid], "w+");    // open file in w+ mode(create, write and read)
        if (executor_outputstream[eid] == nullptr) {
            outputstream_close();
            kupl_log_error_return(ERROR, "fail to open file: %s", filepath[eid]);
        }
    }
    return KUPL_OK;
}

static void outputstream_close()
{
    if (executor_outputstream == nullptr) {  // if executor_outputstream is null, filepath is null too
        return;
    }

    if (filepath == nullptr) {
        free(executor_outputstream); // it should free the executor_outputstream
        executor_outputstream = nullptr;
        return;
    }
    // free all filepath
    for (int eid = 0; eid < ptrace_executor_count; eid++) {
        if (filepath[eid] == nullptr) {
            continue;  // if this filepath is null, all the next filepath and outputstream is null
        }
        if (access(filepath[eid], F_OK) == 0) {  // check file exist
            if (remove(filepath[eid]) != 0) {   // try to remove file (may be no permission)
                kupl_error("cannot remove file");
            }
        } else {
            kupl_error("file not exist");
        }
        free(filepath[eid]);    // free filepath
        filepath[eid] = nullptr;
    }
    // close all outputstream
    for (int eid = 0; eid < ptrace_executor_count; eid++) {
        if (executor_outputstream[eid] == nullptr) {
            continue;
        }
        fclose(executor_outputstream[eid]);
    }
    // free two pointer
    free(filepath);
    filepath = nullptr;
    free(executor_outputstream);
    executor_outputstream = nullptr;
}

static int flush_buffer(int eid, int size)
{
    constexpr const char *format =
        "{\n"
        "\"cat\":\"function\",\n"
        "\"dur\":%Lf,\n"
        "\"name\":\"%s\",\n"
        "\"ph\":\"X\",\n"
        "\"pid\":%lu,\n"
        "\"tid\":%lu,\n"
        "\"ts\":%Lf\n"
        "}"
        ",\n";
    for (int n = 0; n < size; ++n) {
        if (fprintf(executor_outputstream[eid], format,
                    (long double)(trace_event[eid][n].duration / 1000),
                    trace_event[eid][n].name,
                    trace_event[eid][n].pid,
                    trace_event[eid][n].tid,
                    (long double)trace_event[eid][n].start / 1000) < 0) {
            kupl_log_error_return(ERROR, "Fail to write into file!");
        }
    }
    // flush buffer outside the loop to reduce io times
    if (fflush(executor_outputstream[eid]) == EOF) {
        kupl_log_error_return(ERROR, "Fail to write into file!");
    }
    return KUPL_OK;
}

static void stream_merging()
{
    kupl_debug("kupl profile trace begin stream merging");
    char jsonname[MAX_LENGTH_FILENAME];

    // get current time stamp
    char *time_buffer = (char*)calloc(TIME_BUF_SIZE, sizeof(char));
    if (time_buffer == nullptr) {
        kupl_error("there is no memory for time buffer");
        return;
    }
    kupl_timestamp(time_buffer, TIME_BUF_SIZE);
    if (strlen(time_buffer) == 0) {
        free(time_buffer);
        return;
    }
    // input jsonname buffer
    sprintf(jsonname, "%s%s_ptrace_%d.json", ptrace_path, time_buffer, getpid());
    free(time_buffer);
    FILE* out_json = fopen(jsonname, "w+");
    if (out_json == nullptr) {
        kupl_error("cannot open the json file");
        return;
    }
    if (fprintf(out_json, "{\"otherData\": {},\"traceEvents\":[\n") < 0) {
        goto write_err;
    }
    if (fflush(out_json) == EOF) {
        goto write_err;
    }
    for (int eid = 0; eid < ptrace_executor_count; eid++) {
        if (flush_buffer(eid, executor_event_num[eid]) == KUPL_ERROR) {
            goto close_file;
        }
        fseek(executor_outputstream[eid], 0, SEEK_SET);    // move to the head of file
        kupl_vla<char>buffer(FLUSH_BUF_SIZE);

        while (fgets(buffer.get_data(), FLUSH_BUF_SIZE, executor_outputstream[eid]) != nullptr) {
            fwrite(buffer.get_data(), sizeof(char), strlen(buffer.get_data()), out_json);
        }
        if (fflush(out_json) == EOF) {
            goto write_err;
        }
    }
    fseek(out_json, REMOVE_LAST_COMMA_FLAG, SEEK_CUR);  // remove last comma
    if (fprintf(out_json, "\n]}\n") < 0) {
        goto write_err;
    }
    goto close_file;
write_err:
    kupl_error("Fail to write into file!");
close_file:
    fclose(out_json);
}

static int create_executor_buffer()
{
    trace_event = (profile_trace_event **)malloc((size_t)ptrace_executor_count *
                                                            sizeof(profile_trace_event *));
    if (trace_event == nullptr) {
        kupl_log_error_return(ERROR, "There is no memory for trace event");
    }

    for (int eid = 0; eid < ptrace_executor_count; eid++) {
        trace_event[eid] =
        (profile_trace_event*)malloc((size_t)ptrace_executor_buffersize * sizeof(profile_trace_event));
        if (trace_event[eid] == nullptr) {
            destroy_executor_buffer();
            kupl_log_error_return(ERROR, "There is no memory for trace event");
        }
    }
    return KUPL_OK;
}

static void destroy_executor_buffer()
{
    if (trace_event == nullptr) {
        return;
    }
    for (int eid = 0; eid < ptrace_executor_count; eid++) {
        if (trace_event[eid] == nullptr) {
            continue;
        }
        free(trace_event[eid]);
        trace_event[eid] = nullptr;
    }
    free(trace_event);
    trace_event = nullptr;
}

int kupl_ptrace_init()
{
    kupl_prof_level_trace = strcmp(kupl_config_get_value_str(KUPL_PROF_LEVEL), "trace");
    if (kupl_prof_level_trace != 0) {
        return KUPL_OK;
    }
    if (strlen(kupl_config_get_value_str(KUPL_PTRACE_PATH)) >= PTRACE_PATH_SIZE - 1) {
        kupl_log_error_return(ERROR, "trace path is too long");
    }
    strcpy(ptrace_path, kupl_config_get_value_str(KUPL_PTRACE_PATH));
    ptrace_executor_buffersize = kupl_config_get_value(KUPL_PTRACE_THREAD_BUFFER_SIZE);
    kupl_debug("kupl profile trace init");
    g_ptrace_ts = kupl_now_ns();
    const kupl_host_info_t* host_info = kupl_get_host_info();
    ptrace_executor_count = host_info->avail_pu_cnt;
    if (ptrace_executor_count == 0) {
        kupl_log_error_return(ERROR, "ptrace executor count is 0");
    }
    executor_event_num = (int *)calloc((size_t)ptrace_executor_count, sizeof(int));
    if (executor_event_num == nullptr) {
        kupl_log_error_return(ERROR, "There is no memory for executor_event_num");
    }
    // if fail to open outputstream, it will free itself in function
    if (outputstream_open() == KUPL_ERROR) {
        kupl_error("kupl trace init failure by outputsream open");
        free(executor_event_num);
        executor_event_num = nullptr;
        return KUPL_ERROR;
    }
    if (create_executor_buffer() == KUPL_ERROR) {
        kupl_error("kupl trace init failure by create executor buffer");
        outputstream_close();
        free(executor_event_num);
        executor_event_num = nullptr;
        return KUPL_ERROR;
    }
    return KUPL_OK;
}

void kupl_ptrace_fini()
{
    if (kupl_prof_level_trace != 0) {
        return;
    }
    kupl_debug("start kupl_ptrace_fini");
    if (executor_event_num == nullptr) {    // init fail
        return;
    }
    stream_merging();
    outputstream_close();
    destroy_executor_buffer();
    free(executor_event_num);
    executor_event_num = nullptr;
}

void kupl_ptrace_record(kupl_ptrace_tag_t tag, uint64_t ts1, uint64_t ts2)
{
    if (ts1 < g_ptrace_ts || ts2 < g_ptrace_ts) {
        kupl_error("invalid record : timestamp is illegal");
        return;
    }

    thread_local int eid = kupl_get_executor_num();
    if (kupl_unlikely(eid < 0)) {
        kupl_error("invoke KUPL functions on threads not managed by KUPL");
        return;
    }
    trace_event[eid][executor_event_num[eid]].start = ts1;
    trace_event[eid][executor_event_num[eid]].duration = ts2 - ts1;
    trace_event[eid][executor_event_num[eid]].name = ptrace_tag_str(tag);
    trace_event[eid][executor_event_num[eid]].pid = (uint64_t)getpid();
    trace_event[eid][executor_event_num[eid]].tid = (uint64_t)eid;
    ++(executor_event_num[eid]);
    if (executor_event_num[eid] == ptrace_executor_buffersize) {
        kupl_debug("buffer to outputstream");
        executor_event_num[eid] = 0;
        if (flush_buffer(eid, ptrace_executor_buffersize) == KUPL_ERROR) {
            return;
        }
    }
}
#endif
