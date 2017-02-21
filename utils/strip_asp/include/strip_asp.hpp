/***************************************************************************
 *
 *  Copyright (C) 2017 Codeplay Software Limited
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
 *  strip_asp.hpp
 *
 *  Description:
 *    Utility header that helps integrating SYCL types with 
 *    standard template library functions.
 *
 **************************************************************************/
#ifndef COMPUTECPP_STRIP_ASP
#define COMPUTECPP_STRIP_ASP

/* strip_asp.
 * When using ComputeCpp CE, the Device Compiler uses Address Spaces
 * to deal with the different global memories.
 * However, this causes problem with std type traits, which see the
 * types with address space qualifiers as different from the C++ 
 * standard types.
 *
 * This is strip_asp function servers as a workaround that removes
 * the address space for various types.
 */
template<typename TypeWithAddressSpace>
struct strip_asp {
  typedef TypeWithAddressSpace type;
};

#if defined(__SYCL_DEVICE_ONLY__) && defined(__COMPUTECPP__)

#define COMPUTECPP_ASP(NUM) __attribute((address_space(NUM)))

#define GENERATE_STRIP_ASP(ENTRY_TYPE)\
template<>\
struct strip_asp<COMPUTECPP_ASP(1) ENTRY_TYPE>  {\
  typedef ENTRY_TYPE type;\
};\
\
template<>\
struct strip_asp<COMPUTECPP_ASP(2) ENTRY_TYPE>  {\
  typedef ENTRY_TYPE type;\
};\
\
template<>\
struct strip_asp<COMPUTECPP_ASP(3) ENTRY_TYPE>  {\
  typedef ENTRY_TYPE type;\
};

GENERATE_STRIP_ASP(double)
GENERATE_STRIP_ASP(float)
#endif  // __SYCL_DEVICE_ONLY__  && __COMPUTECPP__

#endif  // COMPUTECPP_STRIP_ASP
