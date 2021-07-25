//
// Created by YiwenZhang on 2021/6/3.
//

#include "log.h"

NVMLog::NVMLog(const char *base, uint64_t start_offset, uint64_t size)
    :base_(base), start_(start_offset), size_(size){
    cur_ = 0;
}

NVMLog::NVMLog(std::string path, uint64_t init_size): path_(std::move(path)) {
    size_t mapped_len;
    int is_pmem;
    char* raw = (char*)pmem_map_file(path_.c_str(), init_size, PMEM_FILE_CREATE, 0, &mapped_len, &is_pmem);
    if (raw == nullptr) {
        printf("map file failed [%s]\n", strerror(errno));
        exit(-1);
    }
    base_ = raw;
    size_ = init_size;
    start_ = 0;
    cur_ = 0;
}

NVMLog::~NVMLog() {

}

AllocRes NVMLog::Alloc(uint64_t alloc_size) {
    if (cur_.load(std::memory_order_acquire) + alloc_size >= size_) {
        /*TODO:Need synchronization with threads*/
        //Expand();
        return AllocRes (FULL, 0);
    }
    unsigned int cur = 0;
    do {
        cur = cur_.load(std::memory_order_acquire);
    } while (!cur_.compare_exchange_weak(cur, cur + alloc_size,
                                         std::memory_order_release, std::memory_order_relaxed));
    return AllocRes (SUCCESS, cur);
}

void NVMLog::Expand() {
    if (path_.size() == 0) return;
    pmem_unmap(base_, size_);
    int res = truncate(path_.c_str(), size_ * 2);
    size_t mapped_len;
    int is_pmem;
    char* raw = (char*)pmem_map_file(path_.c_str(), size_ * 2, PMEM_FILE_CREATE, 0, &mapped_len, &is_pmem);
    if (raw == nullptr) {
        printf("map file failed [%s]\n", strerror(errno));
        exit(-1);
    }
    base_ = raw;
    size_ = mapped_len;
}

void NVMLog::Append(uint64_t offset, const std::string src) {
    pmem_memcpy_persist((void*)(base_ + offset), (void*)(src.c_str()), src.size());
}