// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_OBSERVER_LIST_H__
#define BUTIL_OBSERVER_LIST_H__

#include <algorithm>
#include <limits>
#include <vector>

#include "butil/basictypes.h"
#include "butil/logging.h"
#include "butil/memory/weak_ptr.h"

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A container for a list of observers.  Unlike a normal STL vector or list,
//   this container can be modified during iteration without invalidating the
//   iterator.  So, it safely handles the case of an observer removing itself
//   or other observers from the list while observers are being notified.
//
// TYPICAL USAGE:
//
//   class MyWidget {
//    public:
//     ...
//
//     class Observer {
//      public:
//       virtual void OnFoo(MyWidget* w) = 0;
//       virtual void OnBar(MyWidget* w, int x, int y) = 0;
//     };
//
//     void AddObserver(Observer* obs) {
//       observer_list_.AddObserver(obs);
//     }
//
//     void RemoveObserver(Observer* obs) {
//       observer_list_.RemoveObserver(obs);
//     }
//
//     void NotifyFoo() {
//       FOR_EACH_OBSERVER(Observer, observer_list_, OnFoo(this));
//     }
//
//     void NotifyBar(int x, int y) {
//       FOR_EACH_OBSERVER(Observer, observer_list_, OnBar(this, x, y));
//     }
//
//    private:
//     ObserverList<Observer> observer_list_;
//   };
//
//
///////////////////////////////////////////////////////////////////////////////
//
//
// 一个观察者的列表容器。与普通的 STL 数组或列表不同，这个容器可以在迭代期间修改而不会使
// 迭代器失效。所以，我们可以安全的处理一个被通知观察者，即使该观察者已经从列表中移除或者
// 被移除的情况下（原理：在迭代过程中，所有删除操作仅仅将指针置为空）。
//
//   class MyWidget {   // 被观察者
//    public:
//     ...
//
//     class Observer { // 观察者
//      public:
//       virtual void OnFoo(MyWidget* w) = 0;
//       virtual void OnBar(MyWidget* w, int x, int y) = 0;
//     };
//
//     void AddObserver(Observer* obs) {
//       // 添加一个观察者 obs
//       observer_list_.AddObserver(obs);
//     }
//
//     void RemoveObserver(Observer* obs) {
//       // 移除一个观察者 obs
//       observer_list_.RemoveObserver(obs);
//     }
//
//     void NotifyFoo() {
//       // 将 "Foo" 事件通知给所有观察者
//       FOR_EACH_OBSERVER(Observer, observer_list_, OnFoo(this));
//     }
//
//     void NotifyBar(int x, int y) {
//       // 将 "Bar" 事件通知给所有观察者
//       FOR_EACH_OBSERVER(Observer, observer_list_, OnBar(this, x, y));
//     }
//
//    private:
//     ObserverList<Observer> observer_list_; // 观察者列表
//   };
//

// 线程安全的观察者列表容器
template <typename ObserverType>
class ObserverListThreadSafe;

// 观察者列表容器基类
template <class ObserverType>
class ObserverListBase
    : public butil::SupportsWeakPtr<ObserverListBase<ObserverType> > {
 public:
  // Enumeration of which observers are notified.
  // 
  // 通知类型（是否在通知观察者期间，通知新添加的观察者）
  enum NotificationType {
    // Specifies that any observers added during notification are notified.
    // This is the default type if non type is provided to the constructor.
    // 
    // 指定在发送通知期间，添加的新观察者可以被通知。如果向构造函数提供非类型，则这是
    // 默认类型。
    NOTIFY_ALL,

    // Specifies that observers added while sending out notification are not
    // notified.
    // 
    // 指定在发送通知期间，添加的新观察者不被通知。
    NOTIFY_EXISTING_ONLY
  };

  // An iterator class that can be used to access the list of observers.  See
  // also the FOR_EACH_OBSERVER macro defined below.
  // 
  // 观察者列表容器的迭代器
  class Iterator {
   public:
    Iterator(ObserverListBase<ObserverType>& list)
        : list_(list.AsWeakPtr()),
          index_(0),
          max_index_(list.type_ == NOTIFY_ALL ?
                     std::numeric_limits<size_t>::max() :
                     list.observers_.size()) {
      // 观察者被迭代器对象访问
      ++list_->notify_depth_;
    }

    ~Iterator() {
      // 递减观察者容器被迭代器对象引用次数，如为 0 时，删除所有"空观察者(被逻辑删除)"
      // 元素指针。
      if (list_.get() && --list_->notify_depth_ == 0)
        list_->Compact();
    }

    ObserverType* GetNext() {
      if (!list_.get())
        return NULL;
      ListType& observers = list_->observers_;
      // Advance if the current element is null
      size_t max_index = std::min(max_index_, observers.size());
      // 获取不为空的观察者（指针）
      while (index_ < max_index && !observers[index_])
        ++index_;
      return index_ < max_index ? observers[index_++] : NULL;
    }

   private:
    butil::WeakPtr<ObserverListBase<ObserverType> > list_;
    
    // 观察者列表当前被迭代的索引值
    size_t index_;

    // 观察者列表在迭代通知时最大索引值（当 typ_== NOTIFY_ALL 时，每次迭代重新获取所有
    // 观察者列表最大索引，从而让观察者列表在迭代期间添加的新观察者可以被索引到，否则，最
    // 大索引在迭代开始，即，迭代器构造时就被确定，迭代期间添加的新观察者也就不会被通知）。
    size_t max_index_;
  };

  ObserverListBase() : notify_depth_(0), type_(NOTIFY_ALL) {}
  explicit ObserverListBase(NotificationType type)
      : notify_depth_(0), type_(type) {}

  // Add an observer to the list.  An observer should not be added to
  // the same list more than once.
  // 
  // 将观察者添加到列表中。不应将观察者多次添加到同一列表中。
  void AddObserver(ObserverType* obs) {
    // 不能重复添加
    if (std::find(observers_.begin(), observers_.end(), obs)
        != observers_.end()) {
      NOTREACHED() << "Observers can only be added once!";
      return;
    }
    observers_.push_back(obs);
  }

  // Remove an observer from the list if it is in the list.
  // 
  // 从列表中移除一个观察者（如果它在列表中）
  void RemoveObserver(ObserverType* obs) {
    typename ListType::iterator it =
      std::find(observers_.begin(), observers_.end(), obs);
    if (it != observers_.end()) {
      if (notify_depth_) {
        // 观察者列表在迭代期间，不真正删除容器元素
        *it = 0;
      } else {
        // 真正的删除容器中元素（观察者指针）
        observers_.erase(it);
      }
    }
  }

  // 是否已有某"观察者"对象
  bool HasObserver(ObserverType* observer) const {
    for (size_t i = 0; i < observers_.size(); ++i) {
      if (observers_[i] == observer)
        return true;
    }
    return false;
  }

  void Clear() {
    if (notify_depth_) {
      // 观察者列表在迭代期间，不真正删除容器元素
      for (typename ListType::iterator it = observers_.begin();
           it != observers_.end(); ++it) {
        *it = 0;
      }
    } else {
      // 删除所有容器中元素（观察者指针）
      observers_.clear();
    }
  }

 protected:
  size_t size() const { return observers_.size(); }

  void Compact() {
    // @tips
    // std::remove 函数是通过覆盖来"移去"元素的。返回一个迭代器，该迭代器指向未移去
    // 的最后一个元素的下一个位置，它与 begin() 组成的区间表示"移除"后的元素集合。
    // 
    // 
    // 移除所有观察者列表中所有空指针元素（迭代期间，被"逻辑"删除的观察者）。
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(),
                    static_cast<ObserverType*>(NULL)), observers_.end());
  }

 private:
  friend class ObserverListThreadSafe<ObserverType>;

  // 观察者列表内部以 std::vector 存储
  typedef std::vector<ObserverType*> ListType;

  // 观察者列表
  ListType observers_;

  // 观察者列表容器正在被迭代器访问个数
  int notify_depth_;
  NotificationType type_;

  friend class ObserverListBase::Iterator;

  DISALLOW_COPY_AND_ASSIGN(ObserverListBase);
};

// 观察者列表容器
template <class ObserverType, bool check_empty = false>
class ObserverList : public ObserverListBase<ObserverType> {
 public:
  // 通知类型（是否在通知观察者期间，通知新添加的观察者）
  typedef typename ObserverListBase<ObserverType>::NotificationType
      NotificationType;

  ObserverList() {}
  explicit ObserverList(NotificationType type)
      : ObserverListBase<ObserverType>(type) {}

  ~ObserverList() {
    // When check_empty is true, assert that the list is empty on destruction.
    if (check_empty) {
      ObserverListBase<ObserverType>::Compact();
      DCHECK_EQ(ObserverListBase<ObserverType>::size(), 0U);
    }
  }

  bool might_have_observers() const {
    return ObserverListBase<ObserverType>::size() != 0;
  }
};

// 迭代所有观察者列表容器
#define FOR_EACH_OBSERVER(ObserverType, observer_list, func)               \
  do {                                                                     \
    if ((observer_list).might_have_observers()) {                          \
      ObserverListBase<ObserverType>::Iterator                             \
          it_inside_observer_macro(observer_list);                         \
      ObserverType* obs;                                                   \
      while ((obs = it_inside_observer_macro.GetNext()) != NULL)           \
        obs->func;                                                         \
    }                                                                      \
  } while (0)

#endif  // BUTIL_OBSERVER_LIST_H__
