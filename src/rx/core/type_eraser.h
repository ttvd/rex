#ifndef RX_CORE_TYPE_ERASER_H
#define RX_CORE_TYPE_ERASER_H

#include "rx/core/utility/forward.h"
#include "rx/core/utility/move.h"
#include "rx/core/utility/nat.h"
#include "rx/core/utility/index_sequence.h"
#include "rx/core/utility/make_index_sequence.h"
#include "rx/core/utility/construct.h"
#include "rx/core/utility/destruct.h"
#include "rx/core/utility/tuple.h" // tuple

#include "rx/core/traits/type_identity.h"

namespace rx {

// type erasing variant
// 32-bit: 16 bytes + k_memory
// 64-bit: 32 bytes + k_memory
struct type_eraser
  : concepts::no_copy
{
  static constexpr const rx_size k_alignment{16};
  static constexpr const rx_size k_memory{64};

  // initialize type eraser instance with |_data| and call constructor with |_arguments|
  template<typename T, typename... Ts>
  constexpr type_eraser(void *_data, traits::type_identity<T>, Ts&&... _arguments);

  // move a type erased instance |_eraser|
  constexpr type_eraser(type_eraser&& _eraser);

  // call the constructor on the type-erased object
  void init();

  // call the destructor on the type-erased object
  void fini();

  const void* data() const;

private:
  void* m_data;
  void (*m_construct_fn)(void*, void*);
  void (*m_destruct_fn)(void*);
  void (*m_move_tuple_fn)(void*, const void*);

  union {
    utility::nat m_nat;
    alignas(k_alignment) rx_byte m_tuple[k_memory];
  };

  template<typename T, typename... Ts, typename U, rx_size... Ns>
  static void construct_with_tuple(void* _object_data,
    [[maybe_unused]] U* _tuple_object, utility::index_sequence<Ns...>)
  {
    utility::construct<T>(_object_data,
      utility::forward<Ts>(_tuple_object->template get<Ns>())...);
  }

  template<typename T, typename... Ts>
  static void construct(void* _object_data, void* _tuple_data) {
    construct_with_tuple<T, Ts...>(_object_data,
      static_cast<utility::tuple<Ts...>*>(_tuple_data),
        utility::index_sequence_for<Ts...>{});
  }

  template<typename T>
  static void destruct(void* _object_data) {
    utility::destruct<T>(_object_data);
  }

  template<typename T, typename... Ts>
  static void move_tuple(void* _tuple_dst, const void* _tuple_src) {
    utility::construct<utility::tuple<Ts...>>(_tuple_dst,
      utility::move(*static_cast<const utility::tuple<Ts...>*>(_tuple_src)));
  }
};

template<typename T, typename... Ts>
inline constexpr type_eraser::type_eraser(void *_data, traits::type_identity<T>,
  Ts&&... args)
  : m_data{_data}
  , m_construct_fn{construct<T, Ts...>}
  , m_destruct_fn{destruct<T>}
  , m_move_tuple_fn{move_tuple<T, Ts...>}
  , m_nat{}
{
  static_assert(sizeof(utility::tuple<Ts...>) <= sizeof m_tuple,
    "too much data to type erase");

  utility::construct<utility::tuple<Ts...>>(static_cast<void*>(m_tuple),
    utility::forward<Ts>(args)...);
}

inline constexpr type_eraser::type_eraser(type_eraser&& eraser_)
  : m_data{eraser_.m_data}
  , m_construct_fn{eraser_.m_construct_fn}
  , m_destruct_fn{eraser_.m_destruct_fn}
  , m_move_tuple_fn{eraser_.m_move_tuple_fn}
  , m_nat{}
{
  eraser_.m_data = nullptr;
  eraser_.m_construct_fn = nullptr;
  eraser_.m_destruct_fn = nullptr;
  eraser_.m_move_tuple_fn = nullptr;

  m_move_tuple_fn(static_cast<void*>(m_tuple),
    static_cast<const void*>(eraser_.m_tuple));
}

inline void type_eraser::init() {
  m_construct_fn(m_data, static_cast<void*>(m_tuple));
}

inline void type_eraser::fini() {
  m_destruct_fn(m_data);
}

inline const void* type_eraser::data() const {
  return m_data;
}

} // namespace rx

#endif // RX_CORE_TYPE_ERASER_H
