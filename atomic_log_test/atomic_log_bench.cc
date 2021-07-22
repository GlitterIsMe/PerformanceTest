//
// Created by YiwenZhang on 2021/7/22.
//
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#ifndef _WIN32

#include <unistd.h>

#endif

#include <string.h>
#include <iostream>
#include <benchmark/benchmark.h>
#include <thread>
#include <numeric>

#include "log.h"

/* size of the pmemlog pool -- 1 GB */
#define POOL_SIZE ((size_t)(1 << 30))
#define GLOBAL_LOG_NUM 16

const char path[] = "/pmem0/zyw/logfile";


const uint64_t key_begin_size = 8;
const uint64_t key_end_size = 256;
const uint64_t value_begin_size = 8;
const uint64_t value_end_size = 256;
const uint64_t nums = 1024 * 1024;
const uint64_t thread_end = 64;

NVMLog *global_log[GLOBAL_LOG_NUM];

/*
 * printit -- log processing callback for use with pmemlog_walk()
 */
static int
printit(const void *buf, size_t len, void *arg) {
    fwrite(buf, len, 1, stdout);
    return 0;
}

int single_thread_append(benchmark::State &state, uint64_t key_size, uint64_t value_size, int nums, NVMLog *plp) {
    /* create the pmemlog pool or open it if it already exists */
    state.PauseTiming();
    // simple append the same data
    std::string buf(key_size + value_size, '1');

    state.ResumeTiming();

    for (; nums > 0; nums--) {
        AllocRes res = plp->Alloc(buf.size());
        if (res.first == SUCCESS) {
            plp->Append(res.second, buf);
        }
    }
    return 0;
}

int multi_thread_test(uint64_t key_size, uint64_t value_size, int nums, NVMLog *plp) {
    /* create the pmemlog pool or open it if it already exists */
    /* each thread append is related to 1 log*/

    std::string buf(key_size + value_size, '1');

    for (; nums > 0; nums--) {

        AllocRes res = plp->Alloc(buf.size());
        if (res.first == SUCCESS) {
            plp->Append(res.second, buf);
        }
    }
    return 0;
}

int multi_thread_test_random(uint64_t key_size, uint64_t value_size, int nums) {
    /* create the pmemlog pool or open it if it already exists */
    std::string buf(key_size + value_size, '1');
    srand((unsigned int) (time(NULL)));

    for (; nums > 0; nums--) {
        NVMLog* plp = global_log[rand() % GLOBAL_LOG_NUM];
        AllocRes res = plp->Alloc(buf.size());
        if (res.first == SUCCESS) {
            plp->Append(res.second, buf);
        }
    }
    return 0;
}

char* map_pmem_file(const std::string& path, size_t mapped_size, size_t *mapped_len, int *is_pmem) {
    char* raw = (char*)pmem_map_file(path.c_str(), mapped_size, PMEM_FILE_CREATE, 0, mapped_len, is_pmem);
    return raw;
}

void init_global_log() {
    for (int i = 0; i < GLOBAL_LOG_NUM; i++) {
        std::string thread_log = path + std::to_string(i);
        size_t mapped_len;
        int is_pmem;
        char* tmp_file = map_pmem_file(thread_log, POOL_SIZE, &mapped_len, &is_pmem);
        global_log[i] = new NVMLog(tmp_file, 0, mapped_len);
    }
}

void close_global_log() {
    for (int i = 0; i < GLOBAL_LOG_NUM; i++) {
        delete global_log[i];
        global_log[i] = nullptr;
    }
}

static void BM_SingleThread(benchmark::State &state) {
    // Perform setup here

    NVMLog *plp;

    /* create the pmemlog pool or open it if it already exists */
    size_t mapped_len;
    int is_pmem;
    char* tmp_file = map_pmem_file(thread_log, POOL_SIZE, &mapped_len, &is_pmem);
    plp = new NVMLog(tmp_file, mapped_len, is_pmem);

    auto key_size = state.range(0);
    auto value_size = state.range(1);
    auto nums = state.range(2);
    for (auto _ : state) {
        single_thread_append(state, key_size, value_size, nums, plp);
    }

    state.SetBytesProcessed((key_size + value_size) * nums * state.iterations());
    delete plp;

}

static void BM_MultiThread(benchmark::State &state) {
    // Perform setup here
    static NVMLog* plp;

    if (state.thread_index == 0) {
        /* create the pmemlog pool or open it if it already exists */
        size_t mapped_len;
        int is_pmem;
        char* tmp_file = map_pmem_file(thread_log, POOL_SIZE, &mapped_len, &is_pmem);
        plp = new NVMLog(tmp_file, mapped_len, is_pmem);
    }

    auto key_size = state.range(0);
    auto value_size = state.range(1);
    auto nums = state.range(2);
    for (auto _ : state) {
        single_thread_append(state, key_size, value_size, nums / state.threads, plp);
    }
    //
    if (state.thread_index == 0) {
        // std::cout << "current offset: " << pmemlog_tell(plp) << "current threads: " << state.threads << std::endl;
        delete plp;
    }

    // data processed by each thread
    state.SetBytesProcessed((key_size + value_size) * nums / state.threads);
}


// each thread write to seperate log file
static void BM_MultiThread_Sep(benchmark::State &state) {
    // Perform setup here
    NVMLog *plp;

    std::string thread_log(path);
    thread_log += std::to_string(state.thread_index);

    size_t mapped_len;
    int is_pmem;
    char* tmp_file = map_pmem_file(thread_log, POOL_SIZE, &mapped_len, &is_pmem);

    plp = new NVMLog(tmp_file, mapped_len, is_pmem);

    auto key_size = state.range(0);
    auto value_size = state.range(1);
    auto nums = state.range(2);

    for (auto _ : state) {
        single_thread_append(state, key_size, value_size, nums / state.threads, plp);
    }

    delete plp;
    state.SetBytesProcessed((key_size + value_size) * nums * state.iterations() / state.threads);
}

static void BM_MultiThread_Limit(benchmark::State &state) {
    auto key_size = state.range(0);
    auto value_size = state.range(1);
    auto nums = state.range(2);
    srand((unsigned int) time(NULL));
    if (state.thread_index == 0) {
        init_global_log();
    }
    for (auto _ : state) {
        //single_thread_append(state, key_size, value_size, nums / state.threads, global_log[rand() % GLOBAL_LOG_NUM]);
        multi_thread_test_random(key_size, value_size, nums / state.threads);
    }
    state.SetBytesProcessed((key_size + value_size) * nums * state.iterations() / state.threads);
    if (state.thread_index == 0) {
        close_global_log();
    }
}

// Register the function as a benchmark
// BENCHMARK(BM_SingleThread)
//     ->Iterations(1)
//     ->UseRealTime()
//     ->ArgsProduct({
//         // key_size, from 8 bytes to 256 bytes
//         benchmark::CreateRange(key_begin_size, key_end_size, 2),
//         // value_size, from 8 bytes to 256 bytes
//         benchmark::CreateRange(value_begin_size, value_end_size, 2),
//         // nums always 256 bytes
//         benchmark::CreateRange(nums, nums, 1)
//     })
//     ;
/*BENCHMARK(BM_MultiThread)
    ->Iterations(1)
    // {key_size, value_size, total_nums}
    ->Args({128, 128, 1 << 20})
    ->ThreadRange(1, thread_end)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond)
    ;*/


/*BENCHMARK(BM_MultiThread_Sep)
    ->Iterations(1)
    ->Args({128, 128, (1 << 20) * 2})
    ->ThreadRange(1, thread_end)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond)
    ;*/
// Run the benchmark

BENCHMARK(BM_MultiThread_Limit)
        ->Iterations(1)
        ->Args({128, 128, (1 << 20) * 2})
        ->ThreadRange(1, thread_end)
        ->UseRealTime()
        ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();

