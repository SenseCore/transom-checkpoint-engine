/**
 * @file transom_memcpy.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-09-08
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <assert.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
struct Tensor {
    char *data_ptr;
    size_t nBytes;
    size_t size;
};

// Define this to turn on error checking
#define CUDA_ERROR_CHECK

#define CudaSafeCall(err) __cudaSafeCall(err, __FILE__, __LINE__)
#define CudaCheckError() __cudaCheckError(__FILE__, __LINE__)

inline void
__cudaSafeCall(cudaError err, const char *file, const int line) {
#ifdef CUDA_ERROR_CHECK
    if (cudaSuccess != err) {
        fprintf(stderr, "cudaSafeCall() failed at %s:%i : %s\n",
                file, line, cudaGetErrorString(err));
        exit(-1);
    }
#endif
    return;
}

inline void __cudaCheckError(const char *file, const int line) {
#ifdef CUDA_ERROR_CHECK
    cudaError err = cudaGetLastError();
    if (cudaSuccess != err) {
        fprintf(stderr, "cudaCheckError() failed at %s:%i : %s\n",
                file, line, cudaGetErrorString(err));
        exit(-1);
    }
    // More careful checking. However, this will affect performance.
    // Comment away if needed.
    err = cudaDeviceSynchronize();
    if (cudaSuccess != err) {
        fprintf(stderr, "cudaCheckError() with sync failed at %s:%i : %s\n",
                file, line, cudaGetErrorString(err));
        exit(-1);
    }
#endif
    return;
}

#define BUFFER_SIZE (1024 * 512)
#define BUFFER_NUMS 2

bool transom_memcpy(char *Pyshared_mem_name, char *Pymetadata_ptr, size_t Pymetadata_size,
                    Tensor *Pytensors, size_t Pytensor_numbers, size_t Pyshm_size, int Pypid, int Pymemfd) {
    // std::cout << "write shm name: " << shared_mem_name << std::endl;
    // avoid variable drift
    const char *shared_mem_name = Pyshared_mem_name;
    const char *metadata_ptr = Pymetadata_ptr;
    const size_t metadata_size = Pymetadata_size;
    const Tensor *tensors = Pytensors;
    const size_t tensor_numbers = Pytensor_numbers;
    const size_t shm_size = Pyshm_size;
    const int pid = Pypid;
    const int memfd = Pymemfd;

    auto start_time = std::chrono::high_resolution_clock::now();
    const std::string proc_file = "/proc/" + std::to_string(pid) + "/fd/" + std::to_string(memfd);
    const int fd = open(proc_file.c_str(), O_RDWR);
    if (fd < 0) {
        perror("error: Failed to open:");
        std::cerr << "error: Failed to open" << shared_mem_name << "\n";
        return false;
    }
    // check file size
    struct stat sb;
    fstat(fd, &sb);
    size_t file_size = sb.st_size;
    // printf("C++> proc_file:%s size %ld shm_size %ld\n", proc_file.c_str(), file_size, shm_size);
    if (file_size != shm_size) {
        printf("C++> error: %s size != shm_size %ld != %ld\n", proc_file.c_str(), file_size, shm_size);
        return false;
    }
    char *shm_addr = reinterpret_cast<char *>(mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (shm_addr == reinterpret_cast<char *>(MAP_FAILED)) {
        perror("error: Failed to mmap");
        return false;
    }
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);
    printf("C++> remove-create-set shm: %s elapsed time: %ld ms, shm_size: %ld tensor_addr: 0x%lx\n",
           shared_mem_name, diff.count(), shm_size, reinterpret_cast<size_t>(tensors));

    // check whether Tensor is on the GPU
    bool USE_CUDA = false;
    if (tensor_numbers > 0) {
        struct cudaPointerAttributes attributes;
        cudaPointerGetAttributes(&attributes, tensors[tensor_numbers - 1].data_ptr);
        if (attributes.type == cudaMemoryTypeDevice) {
            USE_CUDA = true;
        }
        printf("C++> USE_CUDA: %d tensor_numbers: %ld\n", USE_CUDA, tensor_numbers);
    }
    char *pin_buffer[tensor_numbers * BUFFER_NUMS + 1];
    if (USE_CUDA) {
        // auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < tensor_numbers * BUFFER_NUMS + 1; i++) {
            CudaSafeCall(cudaHostAlloc(reinterpret_cast<void **>(&pin_buffer[i]), BUFFER_SIZE, cudaHostAllocDefault));
        }
        // auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        //      std::chrono::high_resolution_clock::now() - start);
        // printf("C++> cudaHostRegister elapsed time: %d ms tensor_addr:%p\n", diff.count(), (void *)tensors);
    }

    // memcpy: write metadata to shm
    start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threadList;
    // memcpy(shm_addr, metadata_ptr, metadata_size);
    threadList.emplace_back(std::thread(&memcpy, shm_addr, metadata_ptr, metadata_size));
    // printf("C++> metadata_size: %d\n", metadata_size);
    // printf("C++> tensor_numbers: %d\n", tensor_numbers);
    size_t offset = 0;
    char *data_start_pos = shm_addr + metadata_size;

    for (size_t i = 0; i < tensor_numbers; i++) {
        // memcpy(data_start_pos + offset, &(tensors[i].size), sizeof(size_t));
        threadList.emplace_back(std::thread(&memcpy, data_start_pos + offset, &(tensors[i].size), sizeof(size_t)));
        offset += sizeof(size_t);
        if (USE_CUDA) {
            const size_t shared_nums = std::floor(tensors[i].nBytes / BUFFER_SIZE);
            const size_t shared_remainder = tensors[i].nBytes % BUFFER_SIZE;
            auto t0 = std::thread([shared_nums](size_t i, char *pin_buffer, char *data_ptr, char *dest) {
                // printf("C++> i=%ld, offset: %ld shared_nums:%ld shared_remainder:%ld data_start_pos: %ld\n",
                //        i, offset, shared_nums, shared_remainder, data_start_pos + offset);
                for (size_t j = 0; j < shared_nums / BUFFER_NUMS; j++) {
                    // printf("C++> j=%ld, Dsrc=%ld, data_start_pos + shared_offset: %ld \n",
                    //        j, BUFFER_SIZE * j, data_start_pos + shared_offset);
                    CudaSafeCall(cudaMemcpy(pin_buffer, data_ptr + BUFFER_SIZE * j,
                                            BUFFER_SIZE, cudaMemcpyDeviceToHost));
                    memcpy(dest + BUFFER_SIZE * j, pin_buffer, BUFFER_SIZE);
                }
            },
                                  i, pin_buffer[i], tensors[i].data_ptr, data_start_pos + offset);

            auto t1 = std::thread([shared_nums](size_t i, char *pin_buffer, char *data_ptr, char *dest) {
                // printf("C++> i=%ld, offset: %ld shared_nums:%ld shared_remainder:%ld data_start_pos: %ld\n",
                //        i, offset, shared_nums, shared_remainder, data_start_pos + offset);
                for (size_t j = 0; j < shared_nums / BUFFER_NUMS; j++) {
                    // printf("C++> j=%ld, Dsrc=%ld, data_start_pos + shared_offset: %ld \n",
                    //        j, BUFFER_SIZE * j, data_start_pos + shared_offset);
                    CudaSafeCall(cudaMemcpy(pin_buffer, data_ptr + BUFFER_SIZE * j,
                                            BUFFER_SIZE, cudaMemcpyDeviceToHost));
                    memcpy(dest + BUFFER_SIZE * j, pin_buffer, BUFFER_SIZE);
                }
            },
                                  i, pin_buffer[i + tensor_numbers], tensors[i].data_ptr + shared_nums / BUFFER_NUMS * BUFFER_SIZE, data_start_pos + offset + shared_nums / BUFFER_NUMS * BUFFER_SIZE);
            CudaSafeCall(cudaMemcpy(pin_buffer[tensor_numbers * BUFFER_NUMS],
                                    tensors[i].data_ptr + shared_nums * BUFFER_SIZE,
                                    shared_remainder, cudaMemcpyDeviceToHost));
            memcpy(data_start_pos + offset + shared_nums * BUFFER_SIZE,
                   pin_buffer[tensor_numbers * BUFFER_NUMS],
                   shared_remainder);
            threadList.emplace_back(std::move(t0));
            threadList.emplace_back(std::move(t1));
        } else {
            const size_t num_threads = 4; // TODO(guoyongqiang) need a user to define?
            const size_t tensor_per_size = std::floor(tensors[i].nBytes / num_threads);
            const size_t tensor_remainder = tensors[i].nBytes % num_threads;
            for (int j = 0; j < num_threads; ++j) {
                size_t tensor_offset = tensor_per_size * j;
                threadList.emplace_back(std::thread(&memcpy,
                                                    data_start_pos + offset + tensor_offset,
                                                    tensors[i].data_ptr + tensor_offset,
                                                    tensor_per_size));
            }
            threadList.emplace_back(std::thread(&memcpy,
                                                data_start_pos + offset + tensor_per_size * num_threads,
                                                tensors[i].data_ptr + tensor_per_size * num_threads,
                                                tensor_remainder));
        }
        offset += tensors[i].nBytes;
        // printf("C++> tensor_addr: %lld tensors[%ld].nBytes: %ld\n",
        //        (size_t)tensors[i].data_ptr, i, tensors[i].nBytes);
    }
    for (auto &t : threadList) {
        t.join();
    }
    threadList.clear();
    diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);
    printf("C++> memcpy elapsed time: %ld ms, offset: %ld tensor_addr: 0x%lx\n",
           diff.count(), offset, reinterpret_cast<size_t>(tensors));
    // startTime = std::chrono::high_resolution_clock::now();
    if (USE_CUDA) {
        for (size_t i = 0; i < tensor_numbers * BUFFER_NUMS + 1; i++) {
            CudaSafeCall(cudaFreeHost(pin_buffer[i]));
        }
    }
    std::thread([shm_addr, shm_size]() {
        if (munmap(shm_addr, shm_size) != 0) {
            perror("error: munmap failed");
        }
    }).detach();
    if (close(fd) != 0) {
        perror("error: close fd failed");
    }
    offset += metadata_size;
    if (offset != shm_size) {
        std::cerr << "offset != shm_size: \n";
        printf("offset:%ld shm_size:%ld\n", offset, shm_size);
        return false;
    }

    // diff = std::chrono::duration_cast<std::chrono::milliseconds>(
    //     std::chrono::high_resolution_clock::now() - start_time);
    // printf("C++> GC elapsed time: %ld ms, offset: %ld tensor_addr: 0x%lx\n",
    //        diff.count(), offset, (size_t)tensors);

    return true;
}

#ifdef __cplusplus
}
#endif