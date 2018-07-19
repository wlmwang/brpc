// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Derived from google3/util/gtl/stl_util.h

#ifndef BUTIL_STL_UTIL_H_
#define BUTIL_STL_UTIL_H_

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "butil/logging.h"

// @tips
// STL clear()/reserve(0) 并不总是释放内部分配的内存。
// 

// Clears internal memory of an STL object.
// STL clear()/reserve(0) does not always free internal memory allocated
// This function uses swap/destructor to ensure the internal memory is freed.
// 
// 清除 STL 对象的内部存储器。此函数使用 swap()+"析构函数"来确保内部内存被释放。
template<class T>
void STLClearObject(T* obj) {
  T tmp;
  tmp.swap(*obj);
  // Sometimes "T tmp" allocates objects with memory (arena implementation?).
  // Hence using additional reserve(0) even if it doesn't always work.
  // 
  // 有时 "T tmp" 会触发分配内存的对象（arena 实现？）。因此使用额外的 reserve(0) 即使
  // 它没有效。
  obj->reserve(0);
}

// For a range within a container of pointers, calls delete (non-array version)
// on these pointers.
// NOTE: for these three functions, we could just implement a DeleteObject
// functor and then call for_each() on the range and functor, but this
// requires us to pull in all of algorithm.h, which seems expensive.
// For hash_[multi]set, it is important that this deletes behind the iterator
// because the hash_set may call the hash function on the iterator when it is
// advanced, which could result in the hash function trying to deference a
// stale pointer.
// 
// 对于范围内的指针容器迭代器，循环调用 delete（非数组版本）。
template <class ForwardIterator>
void STLDeleteContainerPointers(ForwardIterator begin, ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

// For a range within a container of pairs, calls delete (non-array version) on
// BOTH items in the pairs.
// NOTE: Like STLDeleteContainerPointers, it is important that this deletes
// behind the iterator because if both the key and value are deleted, the
// container may call the hash function on the iterator when it is advanced,
// which could result in the hash function trying to dereference a stale
// pointer.
// 
// 对于范围内的 std::pair 容器迭代器，循环调用 delete first 以及 delete second（非数组
// 版本）。
template <class ForwardIterator>
void STLDeleteContainerPairPointers(ForwardIterator begin,
                                    ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
    delete temp->second;
  }
}

// For a range within a container of pairs, calls delete (non-array version) on
// the FIRST item in the pairs.
// NOTE: Like STLDeleteContainerPointers, deleting behind the iterator.
// 
// 对于范围内的 std::pair 容器迭代器，循环调用 delete first（非数组版本）
template <class ForwardIterator>
void STLDeleteContainerPairFirstPointers(ForwardIterator begin,
                                         ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
  }
}

// For a range within a container of pairs, calls delete.
// NOTE: Like STLDeleteContainerPointers, deleting behind the iterator.
// Deleting the value does not always invalidate the iterator, but it may
// do so if the key is a pointer into the value object.
// 
// 对于范围内的 std::pair 容器迭代器，循环调用 delete second（非数组版本）
template <class ForwardIterator>
void STLDeleteContainerPairSecondPointers(ForwardIterator begin,
                                          ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->second;
  }
}

// To treat a possibly-empty vector as an array, use these functions.
// If you know the array will never be empty, you can use &*v.begin()
// directly, but that is undefined behaviour if |v| is empty.
// 
// 将可能为空的 std::vector<> 转换为原生数组时，请使用这些函数。当 std::vector<> 
// 永远不会是空时，可直接使用 &*v->begin() ，但是如果 |v| 为空时，将是未定义的。
template<typename T>
inline T* vector_as_array(std::vector<T>* v) {
  return v->empty() ? NULL : &*v->begin();
}

template<typename T>
inline const T* vector_as_array(const std::vector<T>* v) {
  return v->empty() ? NULL : &*v->begin();
}

// Return a mutable char* pointing to a string's internal buffer,
// which may not be null-terminated. Writing through this pointer will
// modify the string.
//
// string_as_array(&str)[i] is valid for 0 <= i < str.size() until the
// next call to a string method that invalidates iterators.
//
// As of 2006-04, there is no standard-blessed way of getting a
// mutable reference to a string's internal buffer. However, issue 530
// (http://www.open-std.org/JTC1/SC22/WG21/docs/lwg-active.html#530)
// proposes this as the method. According to Matt Austern, this should
// already work on all current implementations.
// 
// 返回一个指向 std::string 内部缓冲区的可变 char* 指针，该缓冲区可能不是以 null 
// 结尾的。通过此指针可以修改字符串。
// 
// 截至 2006-04 ，没有标准的方式来获取字符串内部缓冲区的可变引用。
// 
// string_as_array(&str)[i] 对 0 <= i < str.size() 有效，直到下一次调用使迭
// 代器无效的 std::string 方法为止。
inline char* string_as_array(std::string* str) {
  // DO NOT USE const_cast<char*>(str->data())
  // 
  // 不要使用 const_cast<char*>(str->data())
  return str->empty() ? NULL : &*str->begin();
}

// The following functions are useful for cleaning up STL containers whose
// elements point to allocated memory.
// 
// 以下函数对于清理其元素是指向已分配内存的 STL 容器非常有用。

// STLDeleteElements() deletes all the elements in an STL container and clears
// the container.  This function is suitable for use with a vector, set,
// hash_set, or any other STL container which defines sensible begin(), end(),
// and clear() methods.
//
// If container is NULL, this function is a no-op.
//
// As an alternative to calling STLDeleteElements() directly, consider
// STLElementDeleter (defined below), which ensures that your container's
// elements are deleted when the STLElementDeleter goes out of scope.
// 
// STLDeleteElements() 删除 STL 容器中的所有"指针元素"指向的已分配实际内存并清除容器（所
// 有元素本身）。此函数适用于 vector,set,hash_set 或任何其他定义合理的 begin(), end() 
// 和 clear() 方法的 STL 容器。
// 如果 container 为 NULL ，则此函数 no-op 。
// 
// 作为直接调用 STLDeleteElements() 的替代方法，请考虑 STLElementDeleter （在下面定义），
// 它确保在 STLElementDeleter 超出作用域范围时删除容器的元素。
template <class T>
void STLDeleteElements(T* container) {
  if (!container)
    return;
  STLDeleteContainerPointers(container->begin(), container->end());
  container->clear();
}

// Given an STL container consisting of (key, value) pairs, STLDeleteValues
// deletes all the "value" components and clears the container.  Does nothing
// in the case it's given a NULL pointer.
// 
// 给定由 (key, value) 对组成 std::pair 的 STL 容器，STLDeleteValues 删除所有 "value" 
// 组件并清除容器。如果 container 为 NULL ，则此函数 no-op 。
template <class T>
void STLDeleteValues(T* container) {
  if (!container)
    return;
  for (typename T::iterator i(container->begin()); i != container->end(); ++i)
    delete i->second;
  container->clear();
}


// The following classes provide a convenient way to delete all elements or
// values from STL containers when they goes out of scope.  This greatly
// simplifies code that creates temporary objects and has multiple return
// statements.  Example:
//
// vector<MyProto *> tmp_proto;
// STLElementDeleter<vector<MyProto *> > d(&tmp_proto);
// if (...) return false;
// ...
// return success;
// 
// 
// STLElementDeleter 提供了一种方便的方法，可以在 STL 容器超出作用域范围时从 STL 容器中
// 删除所有元素或值。这极大地简化了创建临时对象并具有多个 return 语句的代码。
// 
// Use like:
// vector<MyProto *> tmp_proto;
// STLElementDeleter<vector<MyProto *> > d(&tmp_proto);
// if (...) return false;
// ...
// return success;

// Given a pointer to an STL container this class will delete all the element
// pointers when it goes out of scope.
// 
// 给定一个指向 STL 容器的指针，该类将在超出范围时删除所有元素指针。
template<class T>
class STLElementDeleter {
 public:
  STLElementDeleter<T>(T* container) : container_(container) {}
  ~STLElementDeleter<T>() { STLDeleteElements(container_); }

 private:
  T* container_;
};

// Given a pointer to an STL container this class will delete all the value
// pointers when it goes out of scope.
// 
// 给定一个指向 STL 容器的指针，该类将在超出作用域范围时删除所有值("std::pair::second") 
// 的指针。
template<class T>
class STLValueDeleter {
 public:
  STLValueDeleter<T>(T* container) : container_(container) {}
  ~STLValueDeleter<T>() { STLDeleteValues(container_); }

 private:
  T* container_;
};

// Test to see if a set, map, hash_set or hash_map contains a particular key.
// Returns true if the key is in the collection.
// 
// 测试 set, map, hash_set 或 hash_map 是否包含特定键。如果键位于集合中，则返回 true 。
// 
// @tips
// vector,deque,list 或者原生数组，请使用 std::find()/std::find_if() 函数。对于自定
// 义结构，默认使用 operator==() 。
template <typename Collection, typename Key>
bool ContainsKey(const Collection& collection, const Key& key) {
  return collection.find(key) != collection.end();
}

namespace butil {

// Returns true if the container is sorted.
// 
// 如果容器已排序，则返回 true 。
template <typename Container>
bool STLIsSorted(const Container& cont) {
  // Note: Use reverse iterator on container to ensure we only require
  // value_type to implement operator<.
  // 
  // @tips
  // std::adjacent_find() 算法用于查找 [first,last) 区间中相等或满足条件的邻近元素
  // 对（两个连续的元素）。返回元素对中第一个元素的迭代器位置，未找到则返回 last 。
  // 
  // 
  // @todo
  // 注意：在容器上使用反向迭代器以确保我们只需要 value_type 实现 operator< 。
  return std::adjacent_find(cont.rbegin(), cont.rend(),
                            std::less<typename Container::value_type>())
      == cont.rend();
}

// Returns a new ResultType containing the difference of two sorted containers.
// 
// @tips
// std::set_difference() 可以用来求两个集合的差集。要求两个区间必须是有序的（从小到大排列）。
// 算法 std::set_difference() 可构造区间 S1,S2 的差集（出现于 S1 但不出现于 S2 的元素）。
// 由于区间 S1,S2 内的每个元素都不需唯一，因此，如果某个值在 S1 中出现 m 次，在 S2 中出现 n 
// 次，那么该值在输出区间中出现的次数为 std::max(m-n,0) ，且全部来自 S1 。
// 
// Use like:
// std::vector<int> v1 {1, 2, 5, 5, 5, 9};
// std::vector<int> v2 {2, 5, 7};
// std::vector<int> diff;
// 
// std::sort(v1.begin(), v1.end());
// std::sort(v2.begin(), v2.end());
// 
// std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(), 
//    std::inserter(diff, diff.begin()));
// 
// std::cout << "diff: ";
// for (auto i : diff) std::cout << i << ' ';
// 
// Output:
// diff: 1 5 5 9
// 
// 
// 一定谨记：两个区间 S1,S2 必须是有序区间（从小到大）。
// 
// 返回包含两个已排序容器的差异的新 ResultType 。
template <typename ResultType, typename Arg1, typename Arg2>
ResultType STLSetDifference(const Arg1& a1, const Arg2& a2) {
  // 一定谨记：两个区间必须是有序区间（从小到大）。
  DCHECK(STLIsSorted(a1));
  DCHECK(STLIsSorted(a2));
  ResultType difference;
  std::set_difference(a1.begin(), a1.end(),
                      a2.begin(), a2.end(),
                      std::inserter(difference, difference.end()));
  return difference;
}

// Returns a new ResultType containing the union of two sorted containers.
// 
// @tips
// std::set_union() 可以用来求两个集合的并集。要求两个区间必须是有序的（从小到大排列）。
// 算法 std::set_union() 可构造区间 S1,S2 的并集（出现于 S1 和出现于 S2 的元素）。
// 由于区间 S1,S2 内的每个元素都不需唯一，因此，如果某个值在区间 S1 中出现 m 次，在区间 
// S2 中出现 n 次，那么该值在输出区间中会出现 std::min(m,n) 次，且全部来自 S1 。
// 
// Use like:
// std::vector<int> v1 {1, 2, 3, 3, 3, 4, 5};
// std::vector<int> v2 {            3, 4, 5, 6, 7};
// std::vector<int> dest1;
// 
// std::sort(v1.begin(), v1.end());
// std::sort(v2.begin(), v2.end());
// 
// std::set_union(v1.begin(), v1.end(), v2.begin(), v2.end(), 
//    std::back_inserter(dest1));
// 
// std::cout << "union: ";
// for (auto i : dest1) std::cout << i << ' ';
// 
// Output:
// union: 1 2 3 3 3 4 5 6 7
// 
// 
// 一定谨记：两个区间 S1,S2 必须是有序区间（从小到大）。
// 
// 返回包含两个已排序容器的并集的新 ResultType 。
template <typename ResultType, typename Arg1, typename Arg2>
ResultType STLSetUnion(const Arg1& a1, const Arg2& a2) {
  // 一定谨记：两个区间必须是有序区间（从小到大）。
  DCHECK(STLIsSorted(a1));
  DCHECK(STLIsSorted(a2));
  ResultType result;
  std::set_union(a1.begin(), a1.end(),
                 a2.begin(), a2.end(),
                 std::inserter(result, result.end()));
  return result;
}

// Returns a new ResultType containing the intersection of two sorted
// containers.
// 
// @tips
// std::set_intersection() 可以用来求两个集合的交集。要求两个区间必须是有序的（从小到大排列）。
// 算法 std::set_intersection() 可构造区间 S1,S2 的交集（出现于 S1 且出现于 S2 的元素）。
// 由于区间 S1,S2 内的每个元素都不需唯一，因此，如果某个值在区间 S1 中出现 m 次，在区间 
// S2 中出现 n 次，那么该值在输出区间中会出现 std::min(m,n) 次，且全部来自 S1 。
// 
// Use like:
// std::vector<int> v1{1,2,3,4,5,6,7,8};
// std::vector<int> v2{        5,  7,  9,10};
// std::vector<int> v_intersection;
// 
// std::sort(v1.begin(), v1.end());
// std::sort(v2.begin(), v2.end());
// 
// std::set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(), 
//    std::back_inserter(v_intersection));
// 
// std::cout << "intersection: ";
// for (auto i : v_intersection) std::cout << i << ' ';
// 
// Output:
// intersection: 5 7
// 
// 
// 一定谨记：两个区间 S1,S2 必须是有序区间（从小到大）。
// 
// 返回包含两个已排序容器的交集的新 ResultType 。
template <typename ResultType, typename Arg1, typename Arg2>
ResultType STLSetIntersection(const Arg1& a1, const Arg2& a2) {
  // 一定谨记：两个区间必须是有序区间（从小到大）。
  DCHECK(STLIsSorted(a1));
  DCHECK(STLIsSorted(a2));
  ResultType result;
  std::set_intersection(a1.begin(), a1.end(),
                        a2.begin(), a2.end(),
                        std::inserter(result, result.end()));
  return result;
}

// Returns true if the sorted container |a1| contains all elements of the sorted
// container |a2|.
// 
// @tips
// std::includes() 用于判断序列 S2 是否包含于序列 S1 ，前提是序列 S1,S2 必须为有序序列（若
// 为无序序列，首先应该通过 std::sort 使其变为有序序列）， false（不包含）, true（包含）。
// 但是该功能要求序列 S2 中的元素在序列 S1 中仍然连续出现，只是用来判断序列 S1 是否包含 S2 中
// 的所有元素（不要求连续），并不类似于字符串判断中的子序列函数 strstr() 。
// 
// std::vector<char> v1 {'a', 'b', 'c', 'f', 'h', 'x'};
// std::vector<char> v2 {'a', 'b', 'c'};
// 
// std::cout << "includes: " << std::includes(v1.begin(), v1.end(), 
//    v2.begin(), v2.end()) << '\n';
// 
// Output:
// includes: true
// 
// 
// 一定谨记：两个区间 S1,S2 必须是有序区间（从小到大）。
// 
// 如果已排序的容器 |a1| ，包含已排序容器的 |a2| 的所有元素，返回 true 。
template <typename Arg1, typename Arg2>
bool STLIncludes(const Arg1& a1, const Arg2& a2) {
  // 一定谨记：两个区间 S1,S2 必须是有序区间（从小到大）。
  DCHECK(STLIsSorted(a1));
  DCHECK(STLIsSorted(a2));
  return std::includes(a1.begin(), a1.end(),
                       a2.begin(), a2.end());
}

}  // namespace butil

#endif  // BUTIL_STL_UTIL_H_
