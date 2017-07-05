/* A very simple result type
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (59 commits)
File Created: June 2017


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


Distributed under the Boost Software License, Version 1.0.
(See accompanying file Licence.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef OUTCOME_RESULT_HPP
#define OUTCOME_RESULT_HPP

#include "config.hpp"

#include <system_error>
#include <type_traits>  // for SFINAE
#include <utility>      // for in_place_type_t

#ifndef OUTCOME_ENABLE_POSITIVE_STATUS
//! Define to enable positive value + status returns
#define OUTCOME_ENABLE_POSITIVE_STATUS 0
#endif

OUTCOME_V2_NAMESPACE_BEGIN

#if __cplusplus >= 201700
template <class T> using in_place_type_t = std::in_place_type_t<T>;
using std::in_place_type;
#else
//! Aliases `std::in_place_type_t<T>` if on C++ 17 or later, else defined locally.
template <class T> struct in_place_type_t
{
  explicit in_place_type_t() = default;
};
//! Aliases `std::in_place_type<T>` if on C++ 17 or later, else defined locally.
template <class T> constexpr in_place_type_t<T> in_place_type{};
#endif

//! Thrown when you try to access state in a `result<R, S>` which isn't present.
class OUTCOME_SYMBOL_VISIBLE bad_result_access : public std::logic_error
{
public:
  bad_result_access(const char *what)
      : std::logic_error(what)
  {
  }
};

#if OUTCOME_ENABLE_POSITIVE_STATUS
//! Namespace for traits
namespace trait
{
  /*! Trait for whether type `S` is to be considered a negative rather than positive status type.
  \module Error code interpretation policy
  */
  template <class S> struct status_type_is_negative : std::integral_constant<bool, false>
  {
  };
  /*! Trait is enabled for `std::error_code`.
  \module Error code interpretation policy
  */
  template <> struct status_type_is_negative<std::error_code> : std::integral_constant<bool, true>
  {
  };
  /*! Trait is enabled for `std::exception_ptr`.
  \module Error code interpretation policy
  */
  template <> struct status_type_is_negative<std::exception_ptr> : std::integral_constant<bool, true>
  {
  };
  /*! Trait is enabled for `void`.
  \module Error code interpretation policy
  */
  template <> struct status_type_is_negative<void> : std::integral_constant<bool, true>
  {
  };
}
#endif

//! Placeholder type to indicate there is no value type
struct no_value_type
{
  no_value_type() = delete;
};
#if OUTCOME_ENABLE_POSITIVE_STATUS
//! Placeholder type to indicate there is no status type
struct no_status_type
{
  no_status_type() = delete;
};
#endif
//! Placeholder type to indicate there is no error type
struct no_error_type
{
  no_error_type() = delete;
};

namespace detail
{
  // Test if type is an in_place_type_t
  template <class T> struct is_in_place_type_t : std::false_type
  {
  };
  template <class U> struct is_in_place_type_t<in_place_type_t<U>> : std::true_type
  {
  };

  // True if type is the same or constructible
  template <class T, class U, class... Args> static constexpr bool is_same_or_constructible = std::is_same<T, U>::value || std::is_constructible<T, U, Args...>::value;

  // Replace void with constructible void_type
  struct empty_type
  {
  };
  struct void_type
  {
  };
  template <class T> using devoid = std::conditional_t<std::is_void<T>::value, void_type, T>;

  using status_bitfield_type = uint32_t;
  static constexpr status_bitfield_type status_have_value = (1 << 0);
  static constexpr status_bitfield_type status_have_error = (1 << 1);
  static constexpr status_bitfield_type status_have_status = (1 << 2);
  static constexpr status_bitfield_type status_have_payload = (1 << 3);
  static constexpr status_bitfield_type status_have_exception = (1 << 4);
  static constexpr status_bitfield_type status_result_last = (1 << 5);

  // Used if T is trivial
  template <class T> struct value_storage_trivial
  {
    using value_type = T;
    union {
      empty_type _empty;
      devoid<T> _value;
    };
    status_bitfield_type _status;
    constexpr value_storage_trivial() noexcept : _empty{}, _status(0) {}
    constexpr explicit value_storage_trivial(status_bitfield_type status)
        : _empty()
        , _status(status)
    {
    }
    template <class... Args>
    constexpr value_storage_trivial(in_place_type_t<value_type>, Args &&... args) noexcept(std::is_nothrow_constructible<value_type, Args...>::value)
        : _value(std::forward<Args>(args)...)
        , _status(status_have_value)
    {
    }
    template <class U, class... Args>
    constexpr value_storage_trivial(in_place_type_t<value_type>, std::initializer_list<U> il, Args &&... args) noexcept(std::is_nothrow_constructible<value_type, std::initializer_list<U>, Args...>::value)
        : _value(il, std::forward<Args>(args)...)
        , _status(status_have_value)
    {
    }
    template <class U, typename = std::enable_if_t<!std::is_same<std::decay_t<U>, value_type>::value && std::is_constructible<value_type, U>::value>>
    constexpr explicit value_storage_trivial(const value_storage_trivial<U> &o) noexcept(std::is_nothrow_constructible<value_type, U>::value)
        : value_storage_trivial(((o._status & status_have_value) != 0) ? value_storage_trivial(in_place_type<value_type>, o._value) : value_storage_trivial())
    {
      _status = o._status;
    }
    template <class U, typename = std::enable_if_t<!std::is_same<std::decay_t<U>, value_type>::value && std::is_constructible<value_type, U>::value>>
    constexpr explicit value_storage_trivial(value_storage_trivial<U> &&o) noexcept(std::is_nothrow_constructible<value_type, U>::value)
        : value_storage_trivial(((o._status & status_have_value) != 0) ? value_storage_trivial(in_place_type<value_type>, std::move(o._value)) : value_storage_trivial())
    {
      _status = o._status;
    }
  };
  // Used if T is non-trivial
  template <class T> struct value_storage_nontrivial
  {
    using value_type = T;
    union {
      empty_type _empty;
      value_type _value;
    };
    status_bitfield_type _status;
    value_storage_nontrivial() noexcept : _empty{}, _status(0) {}
    value_storage_nontrivial(value_storage_nontrivial &&o) noexcept(std::is_nothrow_move_constructible<value_type>::value)
        : value_storage_nontrivial()
    {
      if(this->_status & status_have_value)
      {
        new(&_value) value_type(std::move(o._value));
        _status = o._status;
      }
    }
    value_storage_nontrivial(const value_storage_nontrivial &o) noexcept(std::is_nothrow_copy_constructible<value_type>::value)
        : value_storage_nontrivial()
    {
      if(this->_status & status_have_value)
      {
        new(&_value) value_type(o._value);
        _status = o._status;
      }
    }
    explicit value_storage_nontrivial(status_bitfield_type status)
        : _empty()
        , _status(status)
    {
    }
    template <class... Args>
    value_storage_nontrivial(in_place_type_t<value_type>, Args &&... args) noexcept(std::is_nothrow_constructible<value_type, Args...>::value)
        : _value(std::forward<Args>(args)...)
        , _status(status_have_value)
    {
    }
    template <class U, class... Args>
    value_storage_nontrivial(in_place_type_t<value_type>, std::initializer_list<U> il, Args &&... args) noexcept(std::is_nothrow_constructible<value_type, std::initializer_list<U>, Args...>::value)
        : _value(il, std::forward<Args>(args)...)
        , _status(status_have_value)
    {
    }
    template <class U, typename = std::enable_if_t<!std::is_same<std::decay_t<U>, value_type>::value && std::is_constructible<value_type, U>::value>>
    constexpr explicit value_storage_nontrivial(const value_storage_nontrivial<U> &o) noexcept(std::is_nothrow_constructible<value_type, U>::value)
        : value_storage_nontrivial((o._status & status_have_value) != 0 ? value_storage_nontrivial(in_place_type<value_type>, o._value) : value_storage_nontrivial())
    {
      _status = o._status;
    }
    template <class U, typename = std::enable_if_t<!std::is_same<std::decay_t<U>, value_type>::value && std::is_constructible<value_type, U>::value>>
    constexpr explicit value_storage_nontrivial(value_storage_nontrivial<U> &&o) noexcept(std::is_nothrow_constructible<value_type, U>::value)
        : value_storage_nontrivial((o._status & status_have_value) != 0 ? value_storage_nontrivial(in_place_type<value_type>, std::move(o._value)) : value_storage_nontrivial())
    {
      _status = o._status;
    }
    ~value_storage_nontrivial() noexcept(std::is_nothrow_destructible<T>::value)
    {
      if(this->_status & status_have_value)
      {
        this->_value.~T();
        this->_status &= ~status_have_value;
      }
    }
  };
  template <class Base> struct value_storage_delete_copy_constructor : Base
  {
    using Base::Base;
    using value_type = typename Base::value_type;
    value_storage_delete_copy_constructor() = default;
    value_storage_delete_copy_constructor(const value_storage_delete_copy_constructor &) = delete;
    value_storage_delete_copy_constructor(value_storage_delete_copy_constructor &&) = default;
  };
  template <class Base> struct value_storage_delete_move_constructor : Base
  {
    using Base::Base;
    using value_type = typename Base::value_type;
    value_storage_delete_move_constructor() = default;
    value_storage_delete_move_constructor(const value_storage_delete_move_constructor &) = default;
    value_storage_delete_move_constructor(value_storage_delete_move_constructor &&) = delete;
  };
  template <class Base> struct value_storage_nontrivial_move_assignment : Base
  {
    using Base::Base;
    using value_type = typename Base::value_type;
    value_storage_nontrivial_move_assignment() = default;
    value_storage_nontrivial_move_assignment(const value_storage_nontrivial_move_assignment &) = default;
    value_storage_nontrivial_move_assignment(value_storage_nontrivial_move_assignment &&) = default;
    value_storage_nontrivial_move_assignment &operator=(const value_storage_nontrivial_move_assignment &o) = default;
    value_storage_nontrivial_move_assignment &operator=(value_storage_nontrivial_move_assignment &&o) noexcept(std::is_nothrow_move_assignable<value_type>::value)
    {
      if((this->_status & status_have_value) != 0 && (o._status & status_have_value) != 0)
      {
        this->_value = std::move(o._value);
      }
      else if((this->_status & status_have_value) != 0 && (o._status & status_have_value) == 0)
      {
        this->_value.~value_type();
      }
      else if((this->_status & status_have_value) == 0 && (o._status & status_have_value) != 0)
      {
        new(&this->_value) value_type(std::move(o._value));
      }
      this->_status = o._status;
      return *this;
    }
  };
  template <class Base> struct value_storage_nontrivial_copy_assignment : Base
  {
    using Base::Base;
    using value_type = typename Base::value_type;
    value_storage_nontrivial_copy_assignment() = default;
    value_storage_nontrivial_copy_assignment(const value_storage_nontrivial_copy_assignment &) = default;
    value_storage_nontrivial_copy_assignment(value_storage_nontrivial_copy_assignment &&) = default;
    value_storage_nontrivial_copy_assignment &operator=(value_storage_nontrivial_copy_assignment &&o) = default;
    value_storage_nontrivial_copy_assignment &operator=(const value_storage_nontrivial_copy_assignment &o) noexcept(std::is_nothrow_copy_assignable<value_type>::value)
    {
      if((this->_status & status_have_value) != 0 && (o._status & status_have_value) != 0)
      {
        this->_value = o._value;
      }
      else if((this->_status & status_have_value) != 0 && (o._status & status_have_value) == 0)
      {
        this->_value.~value_type();
      }
      else if((this->_status & status_have_value) == 0 && (o._status & status_have_value) != 0)
      {
        new(&this->_value) value_type(o._value);
      }
      this->_status = o._status;
      return *this;
    }
  };
  template <class T> using value_storage_select_trivality = std::conditional_t<std::is_trivial<devoid<T>>::value, value_storage_trivial<T>, value_storage_nontrivial<T>>;
  template <class T> using value_storage_select_move_constructor = std::conditional_t<std::is_move_constructible<devoid<T>>::value, value_storage_select_trivality<T>, value_storage_delete_move_constructor<value_storage_select_trivality<T>>>;
  template <class T> using value_storage_select_copy_constructor = std::conditional_t<std::is_copy_constructible<devoid<T>>::value, value_storage_select_move_constructor<T>, value_storage_delete_copy_constructor<value_storage_select_move_constructor<T>>>;
  template <class T> using value_storage_select_move_assignment = std::conditional_t<!std::is_move_assignable<devoid<T>>::value || std::is_trivially_move_assignable<devoid<T>>::value, value_storage_select_copy_constructor<T>, value_storage_nontrivial_move_assignment<value_storage_select_copy_constructor<T>>>;
  template <class T> using value_storage_select_copy_assignment = std::conditional_t<!std::is_copy_assignable<devoid<T>>::value || std::is_trivially_copy_assignable<devoid<T>>::value, value_storage_select_move_assignment<T>, value_storage_nontrivial_copy_assignment<value_storage_select_move_assignment<T>>>;
  template <class T> using value_storage_select_impl = value_storage_select_copy_assignment<T>;
}

namespace impl
{
  template <class R, class S, class NoValuePolicy> class result_final;
  //! The base implementation type of `result<R, EC, NoValuePolicy>`. Only appears separate due to standardese limitations.
  template <class R, class EC, class NoValuePolicy> class result_storage
  {
    friend NoValuePolicy;
    template <class T, class U, class V> friend class result_storage;
    template <class T, class U, class V> friend class result_final;
    static_assert(std::is_void<EC>::value || std::is_default_constructible<EC>::value, "error_type must be default constructible");

    struct disable_in_place_value_type
    {
    };
    struct disable_in_place_error_type
    {
    };

  protected:
    using _value_type = std::conditional_t<std::is_same<R, EC>::value, disable_in_place_value_type, R>;
    using _error_type = std::conditional_t<std::is_same<R, EC>::value, disable_in_place_error_type, EC>;
#if OUTCOME_ENABLE_POSITIVE_STATUS
    using _status_type = std::conditional_t<std::is_same<R, EC>::value, disable_in_place_error_type, EC>;
#endif

    detail::value_storage_select_impl<_value_type> _state;
    detail::devoid<_error_type> _error;

    result_storage() = default;

    template <class... Args>
    constexpr result_storage(in_place_type_t<_value_type> _, Args &&... args) noexcept(std::is_nothrow_constructible<_value_type, Args...>::value)
        : _state(_, std::forward<Args>(args)...)
        , _error()
    {
    }
    template <class U, class... Args>
    constexpr result_storage(in_place_type_t<_value_type> _, std::initializer_list<U> il, Args &&... args) noexcept(std::is_nothrow_constructible<_value_type, std::initializer_list<U>, Args...>::value)
        : _state(_, il, std::forward<Args>(args)...)
        , _error()
    {
    }
    template <class... Args>
    constexpr result_storage(in_place_type_t<_error_type>, Args &&... args) noexcept(std::is_nothrow_constructible<_error_type, Args...>::value)
        : _state(detail::status_have_error)
        , _error(std::forward<Args>(args)...)
    {
    }
    template <class U, class... Args>
    constexpr result_storage(in_place_type_t<_error_type>, std::initializer_list<U> il, Args &&... args) noexcept(std::is_nothrow_constructible<_error_type, std::initializer_list<U>, Args...>::value)
        : _state(detail::status_have_error)
        , _error(il, std::forward<Args>(args)...)
    {
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    struct value_status_construction_tag
    {
    };
    template <class T, class U>
    constexpr result_storage(value_status_construction_tag, T &&t, U &&u) noexcept(std::is_nothrow_constructible<_value_type, T>::value &&std::is_nothrow_constructible<_status_type, U>::value)
        : _state(in_place_type<_value_type>, std::forward<T>(t))
        , _error(std::forward<U>(u))
    {
      _state._status |= detail::status_have_status;
    }
#endif
    struct compatible_conversion_tag
    {
    };
    template <class T, class U, class V>
    constexpr result_storage(compatible_conversion_tag, const result_storage<T, U, V> &o) noexcept(std::is_nothrow_constructible<_value_type, T>::value &&std::is_nothrow_constructible<_error_type, U>::value)
        : _state(o._state)
        , _error(o._error)
    {
    }
    template <class U, class V>
    constexpr result_storage(compatible_conversion_tag, const result_storage<void, U, V> &o) noexcept(std::is_nothrow_constructible<_error_type, U>::value)
        : _state(_value_type())
        , _error(o._error)
    {
    }
    template <class T, class V>
    constexpr result_storage(compatible_conversion_tag, const result_storage<T, void, V> &o) noexcept(std::is_nothrow_constructible<_value_type, T>::value)
        : _state(o._state)
        , _error(_error_type())
    {
    }
    template <class T, class U, class V>
    constexpr result_storage(compatible_conversion_tag, result_storage<T, U, V> &&o) noexcept(std::is_nothrow_constructible<_value_type, T>::value &&std::is_nothrow_constructible<_error_type, U>::value)
        : _state(std::move(o._state))
        , _error(std::move(o._error))
    {
    }
    template <class U, class V>
    constexpr result_storage(compatible_conversion_tag, result_storage<void, U, V> &&o) noexcept(std::is_nothrow_constructible<_error_type, U>::value)
        : _state(_value_type())
        , _error(std::move(o._error))
    {
    }
    template <class T, class V>
    constexpr result_storage(compatible_conversion_tag, result_storage<T, void, V> &&o) noexcept(std::is_nothrow_constructible<_value_type, T>::value)
        : _state(std::move(o._state))
        , _error(_error_type())
    {
    }
  };
  //! The value observers implementation of `result<R, EC, NoValuePolicy>`. Only appears separate due to standardese limitations.
  template <class Base, class R, class NoValuePolicy> class result_value_observers : public Base
  {
  public:
    using value_type = R;
    using Base::Base;

    /// \output_section Narrow state observers
    /*! Access value without runtime checks.
    \preconditions The result to have a successful state, otherwise it is undefined behaviour.
    \returns Reference to the held `value_type` according to overload.
    \group assume_value
    */
    constexpr value_type &assume_value() & noexcept
    {
      NoValuePolicy::narrow_value_check(this);
      return this->_state._value;
    }
    /// \group assume_value
    constexpr const value_type &assume_value() const &noexcept
    {
      NoValuePolicy::narrow_value_check(this);
      return this->_state._value;
    }
    /// \group assume_value
    constexpr value_type &&assume_value() && noexcept
    {
      NoValuePolicy::narrow_value_check(this);
      return std::move(this->_state._value);
    }
    /// \group assume_value
    constexpr const value_type &&assume_value() const &&noexcept
    {
      NoValuePolicy::narrow_value_check(this);
      return std::move(this->_state._value);
    }

    /// \output_section Wide state observers
    /*! Access value with runtime checks.
    \returns Reference to the held `value_type` according to overload.
    \requires The result to have a successful state, else whatever `NoValuePolicy` says ought to happen.
    \group value
    */
    constexpr value_type &value() &
    {
      NoValuePolicy::wide_value_check(this);
      return this->_state._value;
    }
    /// \group value
    constexpr const value_type &value() const &
    {
      NoValuePolicy::wide_value_check(this);
      return this->_state._value;
    }
    /// \group value
    constexpr value_type &&value() &&
    {
      NoValuePolicy::wide_value_check(this);
      return std::move(this->_state._value);
    }
    /// \group value
    constexpr const value_type &&value() const &&
    {
      NoValuePolicy::wide_value_check(this);
      return std::move(this->_state._value);
    }
  };
  template <class Base, class NoValuePolicy> class result_value_observers<Base, void, NoValuePolicy> : public Base
  {
  public:
    using Base::Base;

    /// \output_section Narrow state observers
    /*! Access value without runtime checks.
    \returns Nothing.
    */
    constexpr void assume_value() const noexcept { NoValuePolicy::narrow_value_check(this); }
    /// \output_section Wide state observers
    /*! Access value with runtime checks.
    \returns Nothing.
    \requires The result to have a successful state, else whatever `NoValuePolicy` says ought to happen.
    */
    constexpr void value() const { NoValuePolicy::wide_value_check(this); }
  };

  //! The error observers implementation of `result<R, EC, NoValuePolicy>`. Only appears separate due to standardese limitations.
  template <class Base, class EC, class NoValuePolicy> class result_error_observers : public Base
  {
  public:
    using error_type = EC;
    using Base::Base;

    /// \output_section Narrow state observers
    /*! Access error without runtime checks.
    \preconditions The result to have a failed state, otherwise it is undefined behaviour.
    \returns Reference to the held `error_type` according to overload.
    \group assume_error
    */
    constexpr error_type &assume_error() & noexcept
    {
      NoValuePolicy::narrow_error_check(this);
      return this->_error;
    }
    /// \group assume_error
    constexpr const error_type &assume_error() const &noexcept
    {
      NoValuePolicy::narrow_error_check(this);
      return this->_error;
    }
    /// \group assume_error
    constexpr error_type &&assume_error() && noexcept
    {
      NoValuePolicy::narrow_error_check(this);
      return std::move(this->_error);
    }
    /// \group assume_error
    constexpr const error_type &&assume_error() const &&noexcept
    {
      NoValuePolicy::narrow_error_check(this);
      return std::move(this->_error);
    }

    /// \output_section Wide state observers
    /*! Access error with runtime checks.
    \returns Reference to the held `error_type` according to overload.
    \requires The result to have a failed state, else whatever `NoValuePolicy` says ought to happen.
    \group error
    */
    constexpr error_type &error() &
    {
      NoValuePolicy::wide_error_check(this);
      return this->_error;
    }
    /// \group error
    constexpr const error_type &error() const &
    {
      NoValuePolicy::wide_error_check(this);
      return this->_error;
    }
    /// \group error
    constexpr error_type &&error() &&
    {
      NoValuePolicy::wide_error_check(this);
      return std::move(this->_error);
    }
    /// \group error
    constexpr const error_type &&error() const &&
    {
      NoValuePolicy::wide_error_check(this);
      return std::move(this->_error);
    }
  };
  template <class Base, class NoValuePolicy> class result_error_observers<Base, void, NoValuePolicy> : public Base
  {
  public:
    using Base::Base;
    /// \output_section Narrow state observers
    /*! Access error without runtime checks.
    \returns Nothing.
    */
    constexpr void assume_error() const noexcept { NoValuePolicy::narrow_error_check(this); }
    /// \output_section Wide state observers
    /*! Access error with runtime checks.
    \returns Nothing.
    \requires The result to have a failed state, else whatever `NoValuePolicy` says ought to happen.
    */
    constexpr void error() const { NoValuePolicy::wide_error_check(this); }
  };

#if OUTCOME_ENABLE_POSITIVE_STATUS
  //! The status observers implementation of `result<R, EC, NoValuePolicy>`. Only appears separate due to standardese limitations.
  template <class Base, class EC, class NoValuePolicy> class result_status_observers : public Base
  {
  public:
    using status_type = EC;
    using Base::Base;

    /// \output_section Narrow state observers
    /*! Access status without runtime checks.
    \preconditions The result to have a status state, otherwise it is undefined behaviour.
    \returns Reference to the held `status_type` according to overload.
    \group assume_status
    */
    constexpr status_type &assume_status() & noexcept
    {
      NoValuePolicy::narrow_status_check(this);
      return this->_error;
    }
    /// \group assume_status
    constexpr const status_type &assume_status() const &noexcept
    {
      NoValuePolicy::narrow_status_check(this);
      return this->_error;
    }
    /// \group assume_status
    constexpr status_type &&assume_status() && noexcept
    {
      NoValuePolicy::narrow_status_check(this);
      return std::move(this->_error);
    }
    /// \group assume_status
    constexpr const status_type &&assume_status() const &&noexcept
    {
      NoValuePolicy::narrow_status_check(this);
      return std::move(this->_error);
    }

    /// \output_section Wide state observers
    /*! Access status with runtime checks.
    \returns Reference to the held `status_type` according to overload.
    \requires The result to have a success + status state, else whatever `NoValuePolicy` says ought to happen.
    \group status
    */
    constexpr status_type &status() &
    {
      NoValuePolicy::wide_status_check(this);
      return this->_error;
    }
    /// \group status
    constexpr const status_type &status() const &
    {
      NoValuePolicy::wide_status_check(this);
      return this->_error;
    }
    /// \group status
    constexpr status_type &&status() &&
    {
      NoValuePolicy::wide_status_check(this);
      return std::move(this->_error);
    }
    /// \group status
    constexpr const status_type &&status() const &&
    {
      NoValuePolicy::wide_status_check(this);
      return std::move(this->_error);
    }
  };
  template <class Base, class NoValuePolicy> class result_status_observers<Base, void, NoValuePolicy> : public Base
  {
  public:
    using Base::Base;
    /// \output_section Narrow state observers
    /*! Access status without runtime checks.
    \returns Nothing.
    */
    constexpr void assume_status() const noexcept { NoValuePolicy::narrow_status_check(this); }

    /// \output_section Wide state observers
    /*! Access status with runtime checks.
    \returns Nothing.
    \requires The result to have a success + status state, else whatever `NoValuePolicy` says ought to happen.
    */
    constexpr void status() const { NoValuePolicy::wide_status_check(this); }
  };
  template <class Base, class EC, class NoValuePolicy> using select_result_observers_error_or_status = std::conditional_t<trait::status_type_is_negative<std::decay_t<EC>>::value, result_error_observers<Base, EC, NoValuePolicy>, result_status_observers<Base, EC, NoValuePolicy>>;
  template <class R, class EC, class NoValuePolicy> using select_result_impl = select_result_observers_error_or_status<result_value_observers<result_storage<R, EC, NoValuePolicy>, R, NoValuePolicy>, EC, NoValuePolicy>;
#else
  template <class R, class EC, class NoValuePolicy> using select_result_impl = result_error_observers<result_value_observers<result_storage<R, EC, NoValuePolicy>, R, NoValuePolicy>, EC, NoValuePolicy>;
#endif

  //! The assembled implementation type of `result<R, EC, NoValuePolicy>`. Only appears separate due to standardese limitations.
  template <class R, class S, class NoValuePolicy> class result_final : public select_result_impl<R, S, NoValuePolicy>
  {
    using base = select_result_impl<R, S, NoValuePolicy>;

  public:
    using base::base;

    /// \output_section State check observers
    /*! Checks if has value.
    \returns True if has value.
    */
    constexpr explicit operator bool() const noexcept { return (this->_state._status & detail::status_have_value) != 0; }
    /*! Checks if has value.
    \returns True if has value.
    */
    constexpr bool has_value() const noexcept { return (this->_state._status & detail::status_have_value) != 0; }
    /*! Checks if has error.
    \returns True if has error.
    */
    constexpr bool has_error() const noexcept { return (this->_state._status & detail::status_have_error) != 0; }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Checks if has status information.
\returns True if has status.
*/
    constexpr bool has_status() const noexcept { return (this->_state._status & detail::status_have_status) != 0; }
#endif
    /*! Checks if has payload.
    \returns True if has payload.
    */
    constexpr bool has_payload() const noexcept { return (this->_state._status & detail::status_have_payload) != 0; }
    /*! Checks if has exception.
    \returns True if has exception.
    */
    constexpr bool has_exception() const noexcept { return (this->_state._status & detail::status_have_exception) != 0; }

    /// \output_section Comparison operators
    /*! True if equal to the other result.
    \tparam 4
    \exclude
    \tparam 5
    \exclude
    \param o The other result to compare to.

    \effects Calls the `operator==` operation on each of the two stored items until one returns false.
    \requires That the expression of calling `operator==` on each of the two stored items is a valid expression.
    \throws Any exception the individual `operator==` operations might throw.
    */
    template <class T, class U, class V,                                    //
              typename = decltype(std::declval<R>() == std::declval<T>()),  //
              typename = decltype(std::declval<S>() == std::declval<U>())   //
              >
    constexpr bool operator==(const result_final<T, U, V> &o) const noexcept(  //
    noexcept(std::declval<R>() == std::declval<T>())                           //
    && noexcept(std::declval<S>() == std::declval<U>()))
    {
      if(this->_state._status == o._state._status)
      {
        if(this->_state._status & detail::status_have_value)
        {
          return this->_state._value == o._state._value && this->_error == o._error;
        }
        return this->_error == o._error;
      }
      return false;
    }
    /*! True if not equal to the other result.
    \tparam 4
    \exclude
    \tparam 5
    \exclude
    \param o The other result to compare to.

    \effects Calls the `operator!=` operation on each of the two stored items until one returns true.
    \requires That the expression of calling `operator!=` on each of the two stored items is a valid expression.
    \throws Any exception the individual `operator!=` operations might throw.
    */
    template <class T, class U, class V,                                    //
              typename = decltype(std::declval<R>() != std::declval<T>()),  //
              typename = decltype(std::declval<S>() != std::declval<U>())   //
              >
    constexpr bool operator!=(const result_final<T, U, V> &o) const noexcept(  //
    noexcept(std::declval<R>() != std::declval<T>())                           //
    && noexcept(std::declval<S>() != std::declval<U>()))
    {
      if(this->_state._status != o._state._status)
      {
        return true;
      }
      if(this->_state._status & detail::status_have_value)
      {
        if(this->_state._value != o._state._value)
          return true;
      }
      return this->_error != o._error;
    }
  };
}

//! Namespace for policies
namespace policy
{
#ifdef __cpp_exceptions
  /*! Policy interpreting EC as a type to be thrown directly during wide checks.
  \module Error code interpretation policy
  */
  template <class EC> struct throw_directly
  {
    /*! Performs a narrow check of state, used in the assume_value() functions.
    \effects None.
    */
    template <class Impl> static constexpr void narrow_value_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_value) == 0)
        __builtin_unreachable();
#endif
    }
    /*! Performs a narrow check of state, used in the assume_error() functions
    \effects None.
    */
    template <class Impl> static constexpr void narrow_error_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_error) == 0)
        __builtin_unreachable();
#endif
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Performs a narrow check of state, used in the assume_status() functions
\effects None.
*/
    template <class Impl> static constexpr void narrow_status_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_status) == 0)
        __builtin_unreachable();
#endif
    }
#endif
    /*! Performs a wide check of state, used in the value() functions.
    \effects If result does not have a value, if it has an error it throws that `error()`, else it throws `bad_result_access`.
    */
    template <class Impl> static constexpr void wide_value_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_value) == 0)
      {
        if((self->_state._status & detail::status_have_error) != 0)
        {
          throw self->_error;
        }
        throw bad_result_access("no value");
      }
    }
    /*! Performs a wide check of state, used in the error() functions
    \effects If result does not have an error, it throws `bad_result_access`.
    */
    template <class Impl> static constexpr void wide_error_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_error) == 0)
      {
        throw bad_result_access("no error");
      }
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Performs a wide check of state, used in the status() functions
\effects If result does not have an status, it throws `bad_result_access`.
*/
    template <class Impl> static constexpr void wide_status_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_status) == 0)
      {
        throw bad_result_access("no status");
      }
    }
#endif
  };
  /*! Policy interpreting EC as a type implementing the `std::error_code` contract
  and any wide attempt to access the successful state throws the `error_code` wrapped into
  a `std::system_error`
  \module Error code interpretation policy
  */
  template <class EC> struct error_code_throw_as_system_error
  {
    static_assert(std::is_convertible<std::error_code, EC>::value, "error_type must be convertible into a std::error_code to be used with this policy");
    /*! Performs a narrow check of state, used in the assume_value() functions.
    \effects None.
    */
    template <class Impl> static constexpr void narrow_value_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_value) == 0)
        __builtin_unreachable();
#endif
    }
    /*! Performs a narrow check of state, used in the assume_error() functions.
    \effects None.
    */
    template <class Impl> static constexpr void narrow_error_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_error) == 0)
        __builtin_unreachable();
#endif
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Performs a narrow check of state, used in the assume_status() functions
\effects None.
*/
    template <class Impl> static constexpr void narrow_status_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_status) == 0)
        __builtin_unreachable();
#endif
    }
#endif
    /*! Performs a wide check of state, used in the value() functions.
    \effects If result does not have a value, if it has an error it throws a `std::system_error(error())`, else it throws `bad_result_access`.
    */
    template <class Impl> static constexpr void wide_value_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_value) == 0)
      {
        if((self->_state._status & detail::status_have_error) != 0)
        {
          throw std::system_error(self->_error);
        }
        throw bad_result_access("no value");
      }
    }
    /*! Performs a wide check of state, used in the error() functions
    \effects If result does not have an error, it throws `bad_result_access`.
    */
    template <class Impl> static constexpr void wide_error_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_error) == 0)
      {
        throw bad_result_access("no error");
      }
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Performs a wide check of state, used in the status() functions
\effects If result does not have an status, it throws `bad_result_access`.
*/
    template <class Impl> static constexpr void wide_status_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_status) == 0)
      {
        throw bad_result_access("no status");
      }
    }
#endif
  };
  /*! Policy interpreting EC as a type implementing the `std::exception_ptr` contract
  and any wide attempt to access the successful state calls `std::rethrow_exception()`.
  \module Error code interpretation policy
  */
  template <class EC> struct exception_ptr_rethrow
  {
    static_assert(std::is_convertible<std::exception_ptr, EC>::value, "error_type must be convertible into a std::exception_ptr to be used with this policy");
    /*! Performs a narrow check of state, used in the assume_value() functions.
    \effects None.
    */
    template <class Impl> static constexpr void narrow_value_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_value) == 0)
        __builtin_unreachable();
#endif
    }
    /*! Performs a narrow check of state, used in the assume_error() functions
    \effects None.
    */
    template <class Impl> static constexpr void narrow_error_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_error) == 0)
        __builtin_unreachable();
#endif
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Performs a narrow check of state, used in the assume_status() functions
\effects None.
*/
    template <class Impl> static constexpr void narrow_status_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_status) == 0)
        __builtin_unreachable();
#endif
    }
#endif
    /*! Performs a wide check of state, used in the value() functions
    \effects If result does not have a value, if it has an error it rethrows that error via `std::rethrow_exception()`, else it throws `bad_result_access`.
    */
    template <class Impl> static constexpr void wide_value_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_value) == 0)
      {
        if((self->_state._status & detail::status_have_error) != 0)
        {
          std::rethrow_exception(self->_error);
        }
        throw bad_result_access("no value");
      }
    }
    /*! Performs a wide check of state, used in the value() functions
    \effects If result does not have a value, if it has an error it throws that error, else it throws `bad_result_access`.
    */
    template <class Impl> static constexpr void wide_error_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_error) == 0)
      {
        throw bad_result_access("no error");
      }
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Performs a wide check of state, used in the status() functions
\effects If result does not have an status, it throws `bad_result_access`.
*/
    template <class Impl> static constexpr void wide_status_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_status) == 0)
      {
        throw bad_result_access("no status");
      }
    }
#endif
  };
#endif
  /*! Policy implementing any wide attempt to access the successful state as calling `std::terminate`
  \module Error code interpretation policy
  */
  struct terminate
  {
    /*! Performs a narrow check of state, used in the assume_value() functions.
    \effects None.
    */
    template <class Impl> static constexpr void narrow_value_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_value) == 0)
        __builtin_unreachable();
#endif
    }
    /*! Performs a narrow check of state, used in the assume_error() functions
    \effects None.
    */
    template <class Impl> static constexpr void narrow_error_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_error) == 0)
        __builtin_unreachable();
#endif
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Performs a narrow check of state, used in the assume_status() functions
\effects None.
*/
    template <class Impl> static constexpr void narrow_status_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_status) == 0)
        __builtin_unreachable();
#endif
    }
#endif
    /*! Performs a narrow check of state, used in the assume_payload() functions
    \effects None.
    */
    template <class Impl> static constexpr void narrow_payload_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_payload) == 0)
        __builtin_unreachable();
#endif
    }
    /*! Performs a narrow check of state, used in the assume_exception() functions
    \effects None.
    */
    template <class Impl> static constexpr void narrow_exception_check(Impl *self) noexcept
    {
      (void) self;
#if defined(__GNUC__) || defined(__clang__)
      if((self->_state._status & detail::status_have_exception) == 0)
        __builtin_unreachable();
#endif
    }
    /*! Performs a wide check of state, used in the value() functions.
    \effects If result does not have a value, calls `std::terminate()`.
    */
    template <class Impl> static constexpr void wide_value_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_value) == 0)
      {
        std::terminate();
      }
    }
    /*! Performs a wide check of state, used in the error() functions
    \effects If result does not have an error, calls `std::terminate()`.
    */
    template <class Impl> static constexpr void wide_error_check(Impl *self) noexcept
    {
      if((self->_state._status & detail::status_have_error) == 0)
      {
        std::terminate();
      }
    }
#if OUTCOME_ENABLE_POSITIVE_STATUS
    /*! Performs a wide check of state, used in the status() functions
\effects If result does not have an status, calls `std::terminate()`.
*/
    template <class Impl> static constexpr void wide_status_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_status) == 0)
      {
        std::terminate();
      }
    }
#endif
    /*! Performs a wide check of state, used in the payload() functions
    \effects If outcome does not have an exception, calls `std::terminate()`.
    */
    template <class Impl> static constexpr void wide_payload_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_payload) == 0)
      {
        std::terminate();
      }
    }
    /*! Performs a wide check of state, used in the exception() functions
    \effects If outcome does not have an exception, calls `std::terminate()`.
    */
    template <class Impl> static constexpr void wide_exception_check(Impl *self)
    {
      if((self->_state._status & detail::status_have_exception) == 0)
      {
        std::terminate();
      }
    }
  };

#ifdef __cpp_exceptions
  /*! Default `result<R, S>` policy selector.
  \module Error code interpretation policy
  */
  template <class EC>
  using default_result_policy = std::conditional_t<                                             //
  std::is_void<EC>::value,                                                                      //
  terminate,                                                                                    //
  std::conditional_t<                                                                           //
  detail::is_same_or_constructible<std::error_code, EC>, error_code_throw_as_system_error<EC>,  //
  std::conditional_t<                                                                           //
  detail::is_same_or_constructible<std::exception_ptr, EC>, exception_ptr_rethrow<EC>,          //
  throw_directly<EC>                                                                            //
  >>>;
#else
  template <class EC> using default_result_policy = terminate;
#endif
}

/*! Used to return from functions (i) a value (ii) a value and a positive status or (iii) no value and a negative status. `constexpr` capable.
\module result<R, S> implementation
\tparam R The optional type of the successful result (use `void` to disable).
\tparam S The optional type of the status result (use `void` to disable). Must be either `void` or DefaultConstructible.
\tparam NoValuePolicy Policy on how to interpret type `S` when a wide observation of a not present value occurs.

This is a vocabulary type implementing [P0262R0 A Class for Status and Optional Value](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0262r0.html),
albeit with types `Status` and `Value` reversed in lexical order. By default `S` is considered to be a *positive* status type used to supply
additional information about the successful return of a type `R`. If however the trait `trait::status_type_is_negative<S>`
has been specialised to be true, then `S` is considered to be a *negative* status type used to supply information about
the cause of failure to return a type `R`.

`trait::status_type_is_negative<S>` is already set to true for these types (you can specialise in your own types easily):
- `std::error_code`
- `std::exception_ptr`
- `void`

When `trait::status_type_is_negative<S>` is false, the default for `NoValuePolicy` is:
  1. If `.status()` called when there is no `status_type`:
    - `throw bad_result_access()` if C++ exceptions are enabled, else call `std::terminate()`.

When `trait::status_type_is_negative<S>` is true, the default for `NoValuePolicy` is:
  1. If `.value()` called when there is no `value_type` but there is an `error_type`:
    - If `S` convertible to a `std::error_code`, then `throw std::system_error(error())` [`policy::error_code_throw_as_system_error<S>`]
    if C++ exceptions are enabled, else call `std::terminate()`.
    - If `S` convertible to a `std::exception_ptr`, then `std::rethrow_exception(error())` [`policy::exception_ptr_rethrow<S>`]
    if C++ exceptions are enabled, else call `std::terminate()`.
    - If `S` is `void`, call `std::terminate()` [`policy::terminate<S>`]
    - If `S` is none of the above, then someone has enabled the negative status type trait but did not specify a custom policy.
    We therefore simply `throw error()` [`policy::throw_directly<S>`] if C++ exceptions are enabled, else call `std::terminate`.
  2. If `.error()` called when there is no `error_type`:
    - `throw bad_result_access()` if C++ exceptions are enabled, else call `std::terminate()`.

*/
template <class R,                                                //
          class S = std::error_code,                              //
          class NoValuePolicy = policy::default_result_policy<S>  //
          >
class OUTCOME_NODISCARD result : public impl::result_final<R, S, NoValuePolicy>
{
  using base = impl::result_final<R, S, NoValuePolicy>;

  struct value_converting_constructor_tag
  {
  };
  struct error_converting_constructor_tag
  {
  };
  struct error_condition_converting_constructor_tag
  {
  };
  struct value_status_converting_constructor_tag
  {
  };

public:
  /// \output_section Member types
  //! The success type.
  using value_type = R;
  //! The S type configured
  using status_error_type = S;
#if OUTCOME_ENABLE_POSITIVE_STATUS
  //! The status type, always `no_status_type` if `trait::status_type_is_negative<S>` is true.
  using status_type = typename base::status_type;
  //! The failure type, always `no_error_type` if `trait::status_type_is_negative<S>` is false.
  using error_type = typename base::error_type;
#else
  //! The failure type.
  using error_type = S;
#endif

  //! Used to disable in place type construction when `value_type` and `error_type` are ambiguous.
  using value_type_if_enabled = typename base::_value_type;
  //! Used to disable in place type construction when `value_type` and `error_type` are ambiguous.
  using error_type_if_enabled = typename base::_error_type;

  /// \output_section Default, copy/move constructors and assignment
  //! Default construction is not permitted.
  result() = delete;
  //! Move construction available if `value_type` and `status_error_type` implement it.
  result(result && /*unused*/) = default;
  //! Copy construction available if `value_type` and `status_error_type` implement it.
  result(const result & /*unused*/) = default;
  //! Move assignment available if `value_type` and `status_error_type` implement it.
  result &operator=(result && /*unused*/) = default;
  //! Copy assignment available if `value_type` and `status_error_type` implement it.
  result &operator=(const result & /*unused*/) = default;

  /// \output_section Converting constructors
  /*! Converting constructor to a successful result.
  \tparam enable_value_converting_constructor
  \exclude
  \param 1
  \exclude
  \param t The value from which to initialise the `value_type`.

  \effects Initialises the result with a `value_type`.
  \requires Type T is constructible to `value_type`, is not constructible to `status_error_type`, and is not `result<R, S>` and not `in_place_type<>`.
  \throws Any exception the construction of `value_type(T)` might throw.
  */
  template <class T, typename enable_value_converting_constructor = std::enable_if_t<  //
                     !std::is_same<std::decay_t<T>, result>::value                     // not my type
                     && !detail::is_in_place_type_t<std::decay_t<T>>::value            // not in place construction
                     && detail::is_same_or_constructible<value_type, T> && !std::is_constructible<status_error_type, T>::value>>
  constexpr result(T &&t, value_converting_constructor_tag = value_converting_constructor_tag()) noexcept(std::is_nothrow_constructible<value_type, T>::value)
      : base(in_place_type<typename base::value_type>, std::forward<T>(t))
  {
  }
#if OUTCOME_ENABLE_POSITIVE_STATUS
  /*! Converting constructor to a successful result + status.
\tparam enable_value_status_converting_constructor
\exclude
\param 2
\exclude
\param t The value from which to initialise the `value_type`.
\param u The value from which to initialise the `status_type`.

\effects Initialises the result with a `value_type` and an additional `status_type`.
\requires `trait::status_type_is_negative<EC>` must be false; Type `T` is constructible to `value_type`, is not constructible to `status_type`, and is not `result<R, S>` and not `in_place_type<>`;
Type `U` is constructible to `status_type`, is not constructible to `value_type`.
\throws Any exception the construction of `value_type(T)` and `status_type(U)` might throw.
*/
  template <class T, class U, typename enable_value_status_converting_constructor = std::enable_if_t<  //
                              !std::is_same<std::decay_t<T>, result>::value                            // not my type
                              && !detail::is_in_place_type_t<std::decay_t<T>>::value                   // not in place construction
                              && detail::is_same_or_constructible<value_type, T> && !std::is_constructible<status_type, T>::value && detail::is_same_or_constructible<status_type, U> && !std::is_constructible<value_type, U>::value>>
  constexpr result(T &&t, U &&u, value_status_converting_constructor_tag = value_status_converting_constructor_tag()) noexcept(std::is_nothrow_constructible<value_type, T>::value &&std::is_nothrow_constructible<status_type, U>::value)
      : base(typename base::value_status_construction_tag(), std::forward<T>(t), std::forward<U>(u))
  {
  }
#endif
  /*! Converting constructor to a failure result.
  \tparam enable_error_converting_constructor
  \exclude
  \param 1
  \exclude
  \param t The value from which to initialise the `error_type`.

  \effects Initialises the result with a `error_type`.
  \requires `trait::status_type_is_negative<EC>` must be true; Type T is constructible to `error_type`,
  is not constructible to `value_type`, and is not `result<R, S>` and not `in_place_type<>`.
  \throws Any exception the construction of `error_type(T)` might throw.
  */
  template <class T, typename enable_error_converting_constructor = std::enable_if_t<  //
                     !std::is_same<std::decay_t<T>, result>::value                     // not my type
                     && !detail::is_in_place_type_t<std::decay_t<T>>::value            // not in place construction
                     && !std::is_constructible<value_type, T>::value && detail::is_same_or_constructible<error_type, T>>>
  constexpr result(T &&t, error_converting_constructor_tag = error_converting_constructor_tag()) noexcept(std::is_nothrow_constructible<error_type, T>::value)
      : base(in_place_type<typename base::error_type>, std::forward<T>(t))
  {
  }
  /*! Special error condition converting constructor to a failure result.
  \tparam enable_error_condition_converting_constructor
  \exclude
  \param 1
  \exclude
  \param t The error condition from which to initialise the `error_type`.

  \effects Initialises the result with a `error_type` constructed via `make_error_code(t)`.
  \requires `trait::status_type_is_negative<EC>` must be true; `std::is_error_condition_enum<ErrorCondEnum>` must be true,
  `ErrorCondEnum` is not constructible to `value_type` nor `error_type`, and is not `result<R, S>` and not `in_place_type<>`;
  Finally, the expression `error_type(make_error_code(ErrorCondEnum()))` must be valid.
  \throws Any exception the construction of `error_type(make_error_code(t))` might throw.
  */
  template <class ErrorCondEnum, typename enable_error_condition_converting_constructor = std::enable_if_t<                                               //
                                 !std::is_same<std::decay_t<ErrorCondEnum>, result>::value                                                                // not my type
                                 && !detail::is_in_place_type_t<std::decay_t<ErrorCondEnum>>::value                                                       // not in place construction
                                 && std::is_error_condition_enum<ErrorCondEnum>::value                                                                    // is an error condition enum
                                 && !std::is_constructible<value_type, ErrorCondEnum>::value && !std::is_constructible<error_type, ErrorCondEnum>::value  // not constructible via any other means
                                 >,
            typename = decltype(error_type(make_error_code(ErrorCondEnum())))  // is a valid expression
            >
  constexpr result(ErrorCondEnum &&t, error_condition_converting_constructor_tag = error_condition_converting_constructor_tag()) noexcept(noexcept(error_type(make_error_code(std::forward<ErrorCondEnum>(t)))))
      : base(in_place_type<typename base::error_type>, make_error_code(t))
  {
  }

  /*! Converting copy constructor from a compatible result type.
  \tparam enable_compatible_conversion
  \exclude
  \param o The compatible result.

  \effects Initialises the result with a copy of the compatible result.
  \requires Both result's `value_type`, `error_type` and `status_type` need to be constructible, or the source `void`.
  \throws Any exception the construction of `value_type(T)` and `status_error_type(U)` might throw.
  */
  template <class T, class U, class V, typename enable_compatible_conversion = std::enable_if_t<                                                          //
                                       !std::is_same<result<T, U, V>, result>::value                                                                      // not my type
                                       && (std::is_void<T>::value || detail::is_same_or_constructible<value_type, typename result<T, U, V>::value_type>)  // if our value types are constructible
                                       &&(std::is_void<U>::value || detail::is_same_or_constructible<error_type, typename result<T, U, V>::error_type>)   // if our error types are constructible
#if OUTCOME_ENABLE_POSITIVE_STATUS
                                       &&(std::is_void<U>::value || detail::is_same_or_constructible<status_type, typename result<T, U, V>::status_type>)  // if our status types are constructible
#endif
                                       >>
  constexpr explicit result(const result<T, U, V> &o) noexcept(std::is_nothrow_constructible<value_type, T>::value &&std::is_nothrow_constructible<status_error_type, U>::value)
      : base(typename base::compatible_conversion_tag(), o)
  {
  }
  /*! Converting move constructor from a compatible result type.
  \tparam enable_compatible_conversion
  \exclude
  \param o The compatible result.

  \effects Initialises the result with a move of the compatible result.
  \requires Both result's `value_type`, `error_type` and `status_type` need to be constructible, or the source `void`.
  \throws Any exception the construction of `value_type(T)` and `status_error_type(U)` might throw.
  */
  template <class T, class U, class V, typename enable_compatible_conversion = std::enable_if_t<                                                          //
                                       !std::is_same<result<T, U, V>, result>::value                                                                      // not my type
                                       && (std::is_void<T>::value || detail::is_same_or_constructible<value_type, typename result<T, U, V>::value_type>)  // if our value types are constructible
                                       &&(std::is_void<U>::value || detail::is_same_or_constructible<error_type, typename result<T, U, V>::error_type>)   // if our error types are constructible
#if OUTCOME_ENABLE_POSITIVE_STATUS
                                       &&(std::is_void<U>::value || detail::is_same_or_constructible<status_type, typename result<T, U, V>::status_type>)  // if our status types are constructible
#endif
                                       >>
  constexpr explicit result(result<T, U, V> &&o) noexcept(std::is_nothrow_constructible<value_type, T>::value &&std::is_nothrow_constructible<status_error_type, U>::value)
      : base(typename base::compatible_conversion_tag(), std::move(o))
  {
  }

  /// \output_section In place constructors
  /*! Inplace constructor to a successful result.
  \tparam 1
  \exclude
  \param _ Tag type to indicate we are doing in place construction of `value_type`.
  \param args Arguments with which to in place construct.

  \effects Initialises the result with a `value_type`.
  \requires `value_type` is void or `Args...` are constructible to `value_type`.
  \throws Any exception the construction of `value_type(Args...)` might throw.
  */
  template <class... Args, typename = std::enable_if_t<std::is_void<value_type>::value || std::is_constructible<value_type, Args...>::value>>
  constexpr explicit result(in_place_type_t<value_type_if_enabled>, Args &&... args) noexcept(std::is_nothrow_constructible<value_type, Args...>::value)
      : base(in_place_type<typename base::_value_type>, std::forward<Args>(args)...)
  {
  }
  /*! Inplace constructor to a successful result.
  \tparam 2
  \exclude
  \param _ Tag type to indicate we are doing in place construction of `value_type`.
  \param il An initializer list with which to in place construct.
  \param args Arguments with which to in place construct.

  \effects Initialises the result with a `value_type`.
  \requires The initializer list + `Args...` are constructible to `value_type`.
  \throws Any exception the construction of `value_type(il, Args...)` might throw.
  */
  template <class U, class... Args, typename = std::enable_if_t<std::is_constructible<value_type, std::initializer_list<U>, Args...>::value>>
  constexpr explicit result(in_place_type_t<value_type_if_enabled>, std::initializer_list<U> il, Args &&... args) noexcept(std::is_nothrow_constructible<value_type, std::initializer_list<U>, Args...>::value)
      : base(in_place_type<typename base::_value_type>, il, std::forward<Args>(args)...)
  {
  }
  /*! Inplace constructor to a failure result.
  \tparam 1
  \exclude
  \param _ Tag type to indicate we are doing in place construction of `error_type`.
  \param args Arguments with which to in place construct.

  \effects Initialises the result with a `error_type`.
  \requires `trait::status_type_is_negative<EC>` must be true; `error_type` is void or `Args...` are constructible to `error_type`.
  \throws Any exception the construction of `error_type(Args...)` might throw.
  */
  template <class... Args, typename = std::enable_if_t<std::is_void<error_type>::value || std::is_constructible<error_type, Args...>::value>>
  constexpr explicit result(in_place_type_t<error_type_if_enabled>, Args &&... args) noexcept(std::is_nothrow_constructible<error_type, Args...>::value)
      : base(in_place_type<typename base::_error_type>, std::forward<Args>(args)...)
  {
  }
  /*! Inplace constructor to a failure result.
  \tparam 2
  \exclude
  \param _ Tag type to indicate we are doing in place construction of `error_type`.
  \param il An initializer list with which to in place construct.
  \param args Arguments with which to in place construct.

  \effects Initialises the result with a `error_type`.
  \requires `trait::status_type_is_negative<EC>` must be true; The initializer list + `Args...` are constructible to `error_type`.
  \throws Any exception the construction of `error_type(il, Args...)` might throw.
  */
  template <class U, class... Args, typename = std::enable_if_t<std::is_constructible<error_type, std::initializer_list<U>, Args...>::value>>
  constexpr explicit result(in_place_type_t<error_type_if_enabled>, std::initializer_list<U> il, Args &&... args) noexcept(std::is_nothrow_constructible<error_type, std::initializer_list<U>, Args...>::value)
      : base(in_place_type<typename base::_error_type>, il, std::forward<Args>(args)...)
  {
  }
};

OUTCOME_V2_NAMESPACE_END

#endif
