/***************************************************************************
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
 *  virtual_ptr.hpp
 *
 *  Description:
 *    Interface for SYCL buffers to behave as a non-deferrenciable pointer
 *
 * Authors:
 *
 *    Ruyman Reyes   Codeplay Software Ltd.
 *    Mehdi Goli     Codeplay Software Ltd.
 *
 **************************************************************************/

#include <CL/sycl.hpp>
#include <iostream>

#include <queue>
#include <unordered_map>

#ifndef VIRTUAL_PTR_VERBOSE
// Show extra information when allocating and de-allocating
// #define VIRTUAL_PTR_VERBOSE 1
#define VIRTUAL_PTR_VERBOSE 0
#endif  // VIRTUAL_PTR_VERBOSE

namespace codeplay {

using buffer_data_type = uint8_t;
using sycl_acc_target = cl::sycl::access::target;
using sycl_acc_mode = cl::sycl::access::mode;

/**
 * PointerMapper
 *  Associates fake pointers with buffers.
 *
 */
class PointerMapper {
 public:
  /* pointer information definitions
   */
  static const unsigned long ADDRESS_BITS = sizeof(void *) * 8;

  using base_ptr_t = uintptr_t;

  /* Structure of a virtual pointer
   *
   * |================================================|
   * |               POINTER ADDRESS                  |
   * |================================================|
   */
  struct virtual_pointer_t {
    /* Type for the pointers
    */
    base_ptr_t m_contents;

    /** Conversions from virtual_pointer_t to
     * the void * should just reinterpret_cast the integer
     * number
     */
    operator void *() const { return reinterpret_cast<void *>(m_contents); }

    /**
     * Convert back to the integer number.
     */
    operator base_ptr_t() const { return m_contents; }

    /**
     * Add a certain value to the pointer to create a
     * new pointer to that offset
     */
    virtual_pointer_t operator+(size_t off) { return m_contents + off; }

    /**
     * Numerical order for sorting pointers in containers.
     */
    inline bool operator<(virtual_pointer_t rhs) const {
      return (reinterpret_cast<base_ptr_t>(m_contents) <
              reinterpret_cast<base_ptr_t>(rhs.m_contents));
    }

    /**
     * Numerical order for sorting pointers in containers
     */
    inline bool operator==(virtual_pointer_t rhs) const {
      return (reinterpret_cast<base_ptr_t>(m_contents) ==
              reinterpret_cast<base_ptr_t>(rhs.m_contents));
    }

    /**
     * Simple forward to the equality overload.
     */
    inline bool operator!=(virtual_pointer_t rhs) const {
      return !(this->operator==(rhs));
    }

    /**
     * Converts a void * into a virtual pointer structure.
     * Note that this will only work if the void * was
     * already a virtual_pointer_t, but we have no way of
     * checking
     */
    virtual_pointer_t(const void *ptr)
        : m_contents(reinterpret_cast<base_ptr_t>(ptr)){};

    /**
     * Creates a virtual_pointer_t from the given integer
     * number
     */
    virtual_pointer_t(base_ptr_t u) : m_contents(u){};
  };

  /* Definition of a null pointer
   */
  const virtual_pointer_t null_virtual_ptr = nullptr;

  /**
   * Whether if a pointer is null or not.
   * A pointer is nullptr if the value is of null_virtual_ptr
   */
  static inline bool is_nullptr(virtual_pointer_t ptr) {
    return (static_cast<void *>(ptr) == nullptr);
  }

  /* basic type for all buffers
   */
  using buffer_t = cl::sycl::buffer_mem;

  /**
   * Node that stores information about a device allocation.
   * Nodes are sorted by size to organise a free list of nodes
   * that can be recovered.
   */
  struct pMapNode_t {
    buffer_t _b;
    size_t _size;
    bool _free;

    pMapNode_t(buffer_t b, size_t size, bool f) : _b{b}, _size{size}, _free{f} {
      _b.set_final_data(nullptr);
    }

    bool operator<(const pMapNode_t &rhs) { return (_size < rhs._size); }
  };

  /** Storage of the pointer / buffer tree
   */
  using pointerMap_t = std::map<virtual_pointer_t, pMapNode_t>;

  /**
   * Obtain the insertion point in the pointer map for
   * a pointer of the given size.
   * \param requiredSize Size attemted to reclaim
   */
  typename pointerMap_t::iterator get_insertion_point(size_t requiredSize) {
    typename pointerMap_t::iterator retVal;
    bool reuse = false;
    if (!m_freeList.empty()) {
      // lets try to re-use an existing block
      for (auto freeElem : m_freeList) {
        if (freeElem->second._size == requiredSize) {
          retVal = freeElem;
          reuse = true;
          // Element is not going to be free anymore
          m_freeList.erase(freeElem);
          break;
        }
      }
    }
    if (!reuse) {
      retVal = (--m_pointerMap.end());
    }
    return retVal;
  }

  /**
   * Returns an iterator to the node that stores the information
   * of the given virtual pointer from the given pointer map
   * structure.
   * If pointer asked is not found, throws std::out_of_range.
   * If the pointer map structure is empty, throws std::out_of_range
   *
   * \param pMap the pointerMap_t structure storing all the pointers
   * \param virtual_pointer_ptr The virtual pointer to obtain the node of
   * \throws std::out:of_range if the pointer is not found or pMap is empty
   */
  typename pointerMap_t::iterator get_node(const virtual_pointer_t ptr) {
    if (this->count() == 0) {
      throw std::out_of_range("There are no pointers allocated");
    }
#if VIRTUAL_PTR_VERBOSE
    std::cout << "Searching for: " << static_cast<long>(ptr) << std::endl;
    for (auto &n : m_pointerMap) {
      std::cout << static_cast<long>(n.first) << " { "
                << n.second._b.get_impl().get() << ", " << n.second._free
                << " }" << std::endl;
    }
#endif  // VIRTUAL_PTR_VERBOSE
    // The previous element to the lower bound is the node that
    // holds this memory address
    auto node = m_pointerMap.lower_bound(ptr);
    // If the value of the pointer is not the one of the node
    // then we return the previous one
    if (node->first != ptr) {
      if (node == std::begin(m_pointerMap)) {
        throw std::out_of_range("The pointer is not registered in the map");
      }
      --node;
    }
    return node;
  }

  /* get_buffer.
   * Returns a buffer from the map using the pointer address
   */
  cl::sycl::buffer<buffer_data_type, 1, cl::sycl::detail::base_allocator>
  get_buffer(const virtual_pointer_t ptr) {
    using buffer_t =
        cl::sycl::buffer<buffer_data_type, 1, cl::sycl::detail::base_allocator>;

    // get_node() returns a `buffer_mem`, so we need to cast it to a `buffer<>`.
    // We can do this without the `buffer_mem` being a pointer, as we
    // only declare member variables in the base class (`buffer_mem`) and not in
    // the child class (`buffer<>).
    buffer_t buf(*(static_cast<buffer_t *>(&get_node(ptr)->second._b)));
    return buf;
  }

  /**
   * TODO(Vanya): Doxygen
   */
  template <sycl_acc_mode access_mode,
            sycl_acc_target access_target = sycl_acc_target::global_buffer>
  cl::sycl::accessor<buffer_data_type, 1, access_mode, access_target>
  get_accessor(const virtual_pointer_t ptr) {
    return get_buffer(ptr).get_access<access_mode, access_target>();
  }

  /**
   * TODO(Vanya): Doxygen
   */
  template <sycl_acc_mode access_mode,
            sycl_acc_target access_target = sycl_acc_target::global_buffer>
  cl::sycl::accessor<buffer_data_type, 1, access_mode, access_target>
  get_accessor(const virtual_pointer_t ptr, cl::sycl::handler &cgh) {
    return get_buffer(ptr).get_access<access_mode, access_target>(cgh);
  }

  /*
   * Returns the offset from the base address of this pointer.
   */
  inline off_t get_offset(const virtual_pointer_t ptr) {
    // The previous element to the lower bound is the node that
    // holds this memory address
    return (ptr - get_node(ptr)->first);
  }

  /**
   * Constructs the PointerMapper structure.
   */
  PointerMapper() : m_pointerMap{}, m_freeList{} {};

  /**
   * PointerMapper cannot be copied or moved
   */
  PointerMapper(const PointerMapper &) = delete;

  /**
  *	empty the pointer list
  */
  inline void clear() {
    m_freeList.clear();
    m_pointerMap.clear();
  }

  /* add_pointer.
   * Adds a pointer to the map and returns the virtual pointer id.
   * Note: Currently we don't re-use existing spaces.
   */
  virtual_pointer_t add_pointer(buffer_t &&b) {
    virtual_pointer_t retVal = nullptr;
    size_t bufSize = b.get_count();
    pMapNode_t p{b, bufSize, false};
    // If this is the first pointer:
    if (m_pointerMap.empty()) {
      virtual_pointer_t initialVal{1};
#if VIRTUAL_PTR_VERBOSE
      std::cout << "Adding pointer " << static_cast<long>(initialVal)
                << " COUNT " << p._b.get_count() << " Size: " << p._b.get_size()
                << " Buffer impl: " << p._b.get_impl().get() << std::endl;
#endif  // VIRTUAL_PTR_VERBOSE
      m_pointerMap.emplace(initialVal, p);
      return initialVal;
    }
    auto lastElemIter = get_insertion_point(bufSize);
    // If we are recovering an existing node,
    // since we only recover nodes of the same size, we simply
    // replace the buffer
    if (lastElemIter->second._free) {
      lastElemIter->second._b = b;
      // Note: We set the node as not freed here, once the
      // new buffer is assigned.
      // However, we remove it from the free list in the
      // get_insertion_point function.
      // If an exception happens in between the two, the
      // node will be not freed, but will be unreachable from
      // the free list.
      lastElemIter->second._free = false;
      retVal = lastElemIter->first;
    } else {
      size_t lastSize = lastElemIter->second._size;
      retVal = lastElemIter->first + lastSize;
      m_pointerMap.emplace(retVal, p);
    }
#if VIRTUAL_PTR_VERBOSE
    std::cout << "Adding pointer " << std::hex << static_cast<long>(retVal)
              << std::dec << " Size: " << bufSize << " Buffer impl: "
              << m_pointerMap.rbegin()->second._b.get_impl().get() << std::endl;
#endif  // VIRTUAL_PTR_VERBOSE
    return retVal;
  }

  /* remove_pointer.
   * Removes the given pointer from the map.
   * Currently we dont re-cover the gaps in the virtual address
   * space that we have freed.
   */
  void remove_pointer(const virtual_pointer_t ptr) {
    auto node = this->get_node(ptr);

    // If node is the last one, nothing to do,
    // simply remove and try to recover space
    // if there are various consecutive free blocks
    // at the end.
    if (node->first == m_pointerMap.rbegin()->first) {
      do {
        // Delete the entry on the free list
        auto freeListPtr = m_freeList.find(node);
        if (freeListPtr != std::end(m_freeList)) {
          m_freeList.erase(freeListPtr);
        }
        m_pointerMap.erase(--(m_pointerMap.end()));
        if (m_pointerMap.size() == 0) {
          // The map is empty
          break;
        }
      } while (m_pointerMap.rbegin()->second._free);
    } else {
      node->second._free = true;
      m_freeList.emplace(node);
    }
#if VIRTUAL_PTR_VERBOSE
    std::cout << "New list after removing: " << static_cast<long>(ptr)
              << std::endl;
    for (auto &n : m_pointerMap) {
      std::cout << static_cast<long>(n.first) << " { "
                << n.second._b.get_impl().get() << ", "
                << ((n.second._free) ? "Freed" : "Usable") << ", "
                << n.second._b.get_count() << " }" << std::endl;
    }
#endif  // VIRTUAL_PTR_VERBOSE
  }

  /* count.
   * Return the number of active pointers (i.e, pointers that
   * have been malloc but not freed).
   */
  size_t count() const {
    if (!m_pointerMap.empty()) {
#if VIRTUAL_PTR_VERBOSE
      std::cout << " Map size " << m_pointerMap.size() << " "
                << m_freeList.size() << std::endl;
#endif  // VERBOSE
      return (m_pointerMap.size() - m_freeList.size());
    } else {
      return 0;
    }
  }

 private:
  /* Maps the pointer addresses to buffer and size pairs.
    */
  pointerMap_t m_pointerMap;

  /**
   * Compare two iterators to pointer map entries according to
   * the size of the allocation on the device.
   */
  struct SortBySize {
    bool operator()(typename pointerMap_t::iterator a,
                    typename pointerMap_t::iterator b) {
      return (a->second < b->second);
    }
  };

  /* List of free nodes available for re-using
   */
  std::set<typename pointerMap_t::iterator, SortBySize> m_freeList;
};

/**
 * Malloc-like interface to the pointer-mapper.
 * Given a size, creates a byte-typed buffer and returns a
 * fake pointer to keep track of it.
 * \param size Size in bytes of the desired allocation
 * \throw cl::sycl::exception if error while creating the buffer
 */
template <
    typename buffer_allocator = cl::sycl::default_allocator<buffer_data_type> >
inline void *SYCLmalloc(size_t size, PointerMapper &pMap) {
  // Create a generic buffer of the given size
  using buffer_t = cl::sycl::buffer<buffer_data_type, 1, buffer_allocator>;
  auto thePointer = pMap.add_pointer(buffer_t(cl::sycl::range<1>{size}));
  // Store the buffer on the global list
  return static_cast<void *>(thePointer);
}

/**
 * Free-like interface to the pointer mapper.
 * Given a fake-pointer created with the virtual-pointer malloc,
 * destroys the buffer and remove it from the list.
 */
template <typename PointerMapper>
inline void SYCLfree(void *ptr, PointerMapper &pMap) {
  pMap.remove_pointer(ptr);
}

/**
 * Frees all interface to the pointer mapper.
 * clear all the memory allocared to the SYCL.
 */
template <typename PointerMapper>
inline void SYCLfreeAll(PointerMapper &pMap) {
  pMap.clear();
}

}  // codeplay
