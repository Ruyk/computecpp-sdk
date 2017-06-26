/***************************************************************************
 *
 *  Copyright (C) 2016 Codeplay Software Limited
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  For your convenience, a copy of the License has been included in this
 *  repository.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  Codeplay's ComputeCpp SDK
 *
 *  space.cc
 *
 *  Description:
 *   Tests for the space management of mapper
 *
 **************************************************************************/

#include "gtest/gtest.h"

#include <CL/sycl.hpp>
#include <iostream>

#include "virtual_ptr.hpp"
#include "pointer_alias.hpp"

using sycl_acc_target = cl::sycl::access::target;
const sycl_acc_target sycl_acc_host = sycl_acc_target::host_buffer;
const sycl_acc_target sycl_acc_buffer = sycl_acc_target::global_buffer;

using sycl_acc_mode = cl::sycl::access::mode;
const sycl_acc_mode sycl_acc_rw = sycl_acc_mode::read_write;

using namespace codeplay;

using buffer_t = PointerMapper::buffer_t;

int n = 10000;

TEST(space, add_only) {
  //Expect: memory usage grows
  PointerMapper pMap;
  {
    float* ptrs[n];
    for(int i = 0; i < n; i++)
    {
      ptrs[i] = static_cast<float *>(SYCLmalloc(100 * sizeof(float), pMap));
    }
  }
}

TEST(space, remove_in_order) {
  //Expect: memory usage grows, then stays the same
  PointerMapper pMap;
  {
    float* ptrs[n];

    for(int i = 0; i < n; i++)
    {
      ptrs[i] = static_cast<float *>(SYCLmalloc(100 * sizeof(float), pMap));
    }

    for(int i = 0; i < n; i++)
    {
      SYCLfree(ptrs[i], pMap);
    }
  }
}

TEST(space, remove_reverse_order) {
  //Expect: memory usage grows, then goes down
  PointerMapper pMap;
  {
    float* ptrs[n];

    for(int i = 0; i < n; i++)
    {
      ptrs[i] = static_cast<float *>(SYCLmalloc(100 * sizeof(float), pMap));
    }

    for(int i = n-1; i > 1; i--)
    {
      SYCLfree(ptrs[i], pMap);
    }
  }
}

TEST(space, add_remove_same_size) {
  //Expect: memory usage stays low
  PointerMapper pMap;
  {
    for(int i = 0; i < n; i++)
    {
      auto ptr = static_cast<float *>(SYCLmalloc(100 * sizeof(float), pMap));
      SYCLfree(ptr, pMap);
    }
  }
}

TEST(space, add_remove_diff_size) {
  //Expect: memory usage grows
  PointerMapper pMap;
  {
    for(int i = 0; i < n; i++)
    {
      auto ptr = static_cast<float *>(SYCLmalloc(1* i * sizeof(float), pMap));
      SYCLfree(ptr, pMap);
    }
  }
}
