/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * Memory Allocator
 **/

#pragma once

#include "fastertransformer/common.h"
#include "fastertransformer/utils.h"
#include <cuda_runtime.h>

#ifdef GOOGLE_CUDA
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_types.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/lib/core/errors.h"
#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#endif

namespace fastertransformer{


class IAllocator{
 public:
  virtual void* malloc(size_t size) = 0;
  virtual void free(void* ptr) = 0;
};

template<AllocatorType AllocType_>
class Allocator;

template<>
class Allocator<AllocatorType::CUDA> : public IAllocator{
  const int device_id_;
 public:
  Allocator(int device_id): device_id_(device_id){}

  void* malloc(size_t size) {
    void* ptr = nullptr;
    int o_device = 0;
    check_cuda_error(get_set_device(device_id_, &o_device));
    check_cuda_error(cudaMalloc(&ptr, size));
    check_cuda_error(get_set_device(o_device));
    return ptr;
  }
  
  void free(void* ptr) {
    int o_device = 0;
    check_cuda_error(get_set_device(device_id_, &o_device));
    check_cuda_error(cudaFree(ptr));
    check_cuda_error(get_set_device(o_device));
    return;
  }
};


//TODO: allocator of TensorFlow
//      You can add context to constructor
#ifdef GOOGLE_CUDA
using namespace tensorflow;
template<>
class Allocator<AllocatorType::TF> : public IAllocator{
  OpKernelContext *context_;
  std::vector<Tensor> allocated_tensors_;
 public:
  Allocator(OpKernelContext *context): context_(context){}

  void* malloc(size_t size) {
    long long int buf_size = (long long int)size;
    Tensor temporary_memory;
    tensorflow::Status status = context_->allocate_temp(DT_UINT8, TensorShape{buf_size}, &temporary_memory);
    if(status != tensorflow::Status::OK())
      throw std::runtime_error("[@BertTransformer::allocate], allocate memory failed");

    // Hold the reference of the allocated tensors until the end of the
    // allocator, tensorflow will deallocate tensors until that point.
    allocated_tensors_.push_back(temporary_memory);

    auto flat = temporary_memory.flat<uint8>();
    void* ptr = (void*)flat.data();
    return ptr;
  }
  
  void free(void* ptr) {
      VLOG(2) << "[@BertTransformer::free] deallocating " << ptr << "\n";
      // do nothing here, tensorflow will deallocate for us.
      return;
  }
};
#endif
}//namespace fastertransformer
