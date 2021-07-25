//
// Created by YiwenZhang on 2021/6/3.
//

#ifndef NVM_LOG_LOG_H
#define NVM_LOG_LOG_H

#include <cstdio>
#include <string>
#include <atomic>
#include <libpmem.h>

enum AllocStatus {
    FAILED,
    SUCCESS,
    FULL,
};

typedef std::pair<AllocStatus, uint64_t> AllocRes;

class NVMLog {
public:
    NVMLog(char* base, uint64_t start_offset, uint64_t size);
    NVMLog(std::string path, uint64_t init_size);
    ~NVMLog();

    AllocRes Alloc(uint64_t alloc_size);
    void Append(uint64_t offset, const std::string src);
    void Expand();

private:
    std::string path_;
    char* base_;
    uint64_t start_;
    uint64_t size_;
    std::atomic<unsigned int> cur_;
};

#endif //NVM_LOG_LOG_H
