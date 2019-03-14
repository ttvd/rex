#ifndef RX_RENDER_RESOURCE_H
#define RX_RENDER_RESOURCE_H

#include <rx/core/types.h>

namespace rx::render {

struct resource {
  enum class category {
    k_buffer,
    k_texture1D,
    k_texture2D,
    k_texture3D,
    k_textureCM
  };

  resource(category _type);
  ~resource();

  void update_resource_usage(rx_size _bytes);

private:
  category m_resource_type;
  rx_size m_resource_bytes;
};

inline resource::resource(category _type)
  : m_resource_type{_type}
  , m_resource_bytes{0}
{
  // {empty}
}

inline resource::~resource() {
  update_resource_usage(0);
}

} // namespace rx::render

#endif // RX_RENDER_RESOURCE_H