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

int n = 20;
int startCount = 5;

TEST(space, add_only) {
  //Expect: memory usage grows
  PointerMapper pMap;
  {
    float* ptrs[n];
    for(int i = 0; i < n; i++)
    {
      ptrs[i] = static_cast<float *>(SYCLmalloc(100 * sizeof(float), pMap));
      ASSERT_EQ(pMap.count(), i+1);
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
      ASSERT_EQ(pMap.count(), i+1);
    }

    for(int i = 0; i < n; i++)
    {
      SYCLfree(ptrs[i], pMap);
      ASSERT_EQ(pMap.count(), n-1-i);
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
      ASSERT_EQ(pMap.count(), i+1);
    }

    for(int i = n-1; i >= 0; i--)
    {
      SYCLfree(ptrs[i], pMap);
      ASSERT_EQ(pMap.count(), i);
    }

    ASSERT_EQ(pMap.count(), 0);
  }
}

TEST(space, add_remove_same_size) {
  //Expect: memory usage stays low
  PointerMapper pMap;
  {
    float* ptrs[n];

    for(int i = 0; i < startCount; i++)
    {
      ptrs[i] = static_cast<float *>(SYCLmalloc(n * sizeof(float), pMap));
      ASSERT_EQ(pMap.count(), i+1);
    }

    for(int i = startCount; i < n; i++)
    {
      SYCLfree(ptrs[i-startCount], pMap);
      ptrs[i] = static_cast<float *>(SYCLmalloc(n * sizeof(float), pMap));
      ASSERT_EQ(pMap.count(), startCount);
    }
  }
}

TEST(space, add_remove_diff_size) {
  //Expect: memory usage grows
  PointerMapper pMap;
  {
    float* ptrs[n];

    for(int i = 0; i < startCount; i++)
    {
      ptrs[i] = static_cast<float *>(SYCLmalloc(n * sizeof(float), pMap));
      ASSERT_EQ(pMap.count(), i+1);
    }

    for(int i = startCount; i < n; i++)
    {
      SYCLfree(ptrs[i-startCount], pMap);
      ptrs[i] = static_cast<float *>(SYCLmalloc(1* (n-i) * sizeof(float), pMap));
      ASSERT_EQ(pMap.count(), startCount);
    }
  }
}

TEST(space, fragmentation) {
  PointerMapper pMap;
  {
    auto length1 = 100;
    auto length2 = 50;
    auto length3 = 50;
    auto length4 = 100;

    auto ptr1 = static_cast<float *>(SYCLmalloc(length1*sizeof(float), pMap));
    auto ptr2 = static_cast<float *>(SYCLmalloc(length2*sizeof(float), pMap));
    auto ptr3 = static_cast<float *>(SYCLmalloc(length3*sizeof(float), pMap));
    auto ptr4 = static_cast<float *>(SYCLmalloc(length4*sizeof(float), pMap));

    // Remove the second pointer
    SYCLfree(ptr2, pMap);
    // The pointer is freed
    ASSERT_TRUE(pMap.get_node(ptr2)->second._free); 

    // Add a new pointer, half the size of the removed pointer
    auto newSize = length2*sizeof(float)/2;
    auto ptr5 = static_cast<float *>(SYCLmalloc(newSize, pMap));
    /*
    // New pointer reuses the space of the removed pointer
    ASSERT_EQ(ptr2, ptr5); 
    // The remaining space is freed and of correct size
    auto ptrFree = ptr5+length2/2;
    auto freeSize = length2*sizeof(float) - newSize;
    ASSERT_TRUE(pMap.get_node(ptrFree)->second._free);
    ASSERT_EQ(freeSize, pMap.get_node(ptrFree)->second._size);
    */

    // Free the node after the new free space
    // They are two separate nodes
    /* TODO (Vanya)
    ASSERT_NE(ptr3, ptrFree);
    SYCLfree(ptr3, pMap);
    // The two freed spaces are now fused
    ASSERT_EQ(freeSize+length3*sizeof(float), pMap.get_node(ptrFree)->second._size);
    ASSERT_EQ(pMap.get_node(ptr3), pMap.get_node(ptrFree));
    */
  }
}
