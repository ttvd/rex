#ifndef RX_RENDER_PROGRAM_H
#define RX_RENDER_PROGRAM_H

#include <rx/math/vec2.h>   // vec2{i,f}
#include <rx/math/vec3.h>   // vec3{i,f}
#include <rx/math/vec4.h>   // vec4{i,f}
#include <rx/math/mat3x3.h> // mat3x3f
#include <rx/math/mat4x4.h> // mat4x4f

#include <rx/core/string.h>
#include <rx/core/array.h>
#include <rx/core/hash.h>

#include <rx/render/resource.h>

namespace rx::render
{

struct frontend;

struct uniform {
  enum class category {
    k_sampler,
    k_bool,
    k_int,
    k_float,
    k_vec2i,
    k_vec3i,
    k_vec4i,
    k_vec2f,
    k_vec3f,
    k_vec4f,
    k_mat4x4f,
    k_mat3x3f
  };

  uniform();
  uniform(memory::allocator* allocator, const string& _name, category _type);
  ~uniform();

  void record_sampler(int _sampler);
  void record_int(int _value);
  void record_vec2i(const math::vec2i& _value);
  void record_vec3i(const math::vec3i& _value);
  void record_vec4i(const math::vec4i& _value);
  void record_bool(bool _value);
  void record_float(rx_f32 _value);
  void record_vec2f(const math::vec2f& _value);
  void record_vec3f(const math::vec3f& _value);
  void record_vec4f(const math::vec4f& _value);
  void record_raw(const rx_byte* _data, rx_size _size);

  category type() const;
  const rx_byte* data() const;
  const string& name() const;
  rx_size size() const;

  void flush(rx_byte* _data);

private:
  static rx_size size_for_type(category _type);

  memory::allocator* m_allocator;
  category m_type;
  union {
    rx_byte* as_opaque;
    rx_s32* as_int;
    bool* as_boolean;
    rx_f32* as_float;
  };
  string m_name;
  bool m_dirty;
};

inline uniform::category uniform::type() const {
  return m_type;
}

inline const rx_byte* uniform::data() const {
  return as_opaque;
}

inline const string& uniform::name() const {
  return m_name;
}

inline rx_size uniform::size() const {
  return size_for_type(m_type);
}

struct program : resource {
  enum class shader {
    k_fragment,
    k_vertex
  };

  struct description {
    description() = default;
    description(const string& _name, const array<string>& _data,
      const array<string>& _layout, const array<string>& _defines);
    string name;
    array<string> data;
    array<string> layout;
    array<string> defines;
  };

  program(frontend* _frontend);

  void record_description(const description& _description);

  bool validate() const;

  void add_shader(shader _shader);
  uniform& add_uniform(const string& _name, uniform::category _type);
  uniform& operator[](rx_size _index);

  array<rx_byte> flush();

  const array<uniform>& uniforms() const &;
  const array<shader>& shaders() const &;
  const description& info() const &;

private:
  frontend* m_frontend;
  array<uniform> m_uniforms;
  array<shader> m_shaders;
  description m_description;
  rx_u64 m_dirty_bits;
  bool m_has_description;
};

inline const array<uniform>& program::uniforms() const & {
  return m_uniforms;
}

inline const program::description& program::info() const & {
  return m_description;
}

bool operator==(const program::description& _lhs, const program::description& _rhs);

} // namespace rx::render

namespace rx {
  template<>
  struct hash<render::program::description> {
    rx_size operator()(const render::program::description& _description);
  };
} // namespace rx

#endif // RX_RENDER_PROGRAM_H