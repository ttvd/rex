#ifndef RX_MATH_TRANSFORM_H
#define RX_MATH_TRANSFORM_H

#include <rx/math/mat3x3.h> // mat3x3, vec3f
#include <rx/math/mat4x4.h> // mat4x4

namespace rx::math {

struct transform {
  constexpr transform();
  constexpr transform(transform* parent);
  mat4x4f to_mat4() const;
  vec3f scale;
  vec3f rotate;
  vec3f translate;
  transform *parent;
};

inline constexpr transform::transform()
  : scale{1.0f, 1.0f, 1.0f}
  , rotate{}
  , translate{}
  , parent{nullptr}
{
}

inline constexpr transform::transform(transform* _parent)
  : transform{}
{
  parent = _parent;
}

inline mat4x4f transform::to_mat4() const {
  const auto local{mat4x4f::scale(scale) * mat4x4f::rotate(rotate) *
    mat4x4f::translate(translate)};
  return parent ? local * parent->to_mat4() : local;
}

} // namespace rx::math

#endif // RX_MATH_TRANSFORM_H
