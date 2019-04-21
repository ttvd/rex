#include <string.h> // memcpy

#include <rx/render/texture.h>
#include <rx/math/log2.h>

namespace rx::render {

texture::texture(frontend* _frontend, resource::type _type)
  : resource{_frontend, _type}
  , m_recorded{0}
{
}

void texture::record_format(data_format _format) {
  RX_ASSERT(!(m_recorded & k_format), "format already recorded");

  m_format = _format;
  m_recorded |= k_format;
}

void texture::record_type(type _type) {
  RX_ASSERT(!(m_recorded & k_type), "type already recorded");

  m_type = _type;
  m_recorded |= k_type;
}

void texture::record_filter(const filter_options& _options) {
  RX_ASSERT(!(m_recorded & k_filter), "filter already recorded");

  m_filter = _options;
  m_recorded |= k_filter;
}

void texture::validate() const {
  RX_ASSERT(m_recorded & k_format, "format not recorded");
  RX_ASSERT(m_recorded & k_type, "type not recorded");
  RX_ASSERT(m_recorded & k_filter, "filter not recorded");
  RX_ASSERT(m_recorded & k_wrap, "wrap not recorded");
  RX_ASSERT(m_recorded & k_dimensions, "dimensions not recorded");
}

// texture1D
texture1D::texture1D(frontend* _frontend)
  : texture{_frontend, resource::type::k_texture1D}
{
}

texture1D::~texture1D() {
}

void texture1D::write(const rx_byte* _data, rx_size _level) {
  RX_ASSERT(_data, "_data is null");
  RX_ASSERT(_level < levels(), "mipmap level out of bounds");
  validate();

  const auto& info{m_levels[_level]};
  memcpy(m_data.data() + info.offset, _data, info.size);
}

rx_byte* texture1D::map(rx_size _level) {
  RX_ASSERT(_level < levels(), "mipmap level out of bounds");
  validate();

  return m_data.data() + m_levels[_level].offset;
}

void texture1D::record_dimensions(const dimension_type& _dimensions) {
  RX_ASSERT(!(m_recorded & k_dimensions), "dimensions already recorded");

  RX_ASSERT(m_recorded & k_type, "type not recorded");
  RX_ASSERT(m_recorded & k_filter, "filter not recorded");
  RX_ASSERT(m_recorded & k_format, "format not recorded");

  m_dimensions = _dimensions;
  m_dimensions_log2 = math::log2(m_dimensions);
  m_recorded |= k_dimensions;

  if (m_type != type::k_attachment) {
    dimension_type dimensions{m_dimensions};
    rx_size offset{0};
    const auto bpp{byte_size_of_format(m_format)};
    for (rx_size i{0}; i < levels(); i++) {
      const rx_size size{dimensions * bpp};
      m_levels.push_back({offset, size, dimensions});
      offset += size;
      dimensions /= 2;
    }

    m_data.resize(offset);
    update_resource_usage(m_data.size());
  }
}

void texture1D::record_wrap(const wrap_options& _wrap) {
  RX_ASSERT(!(m_recorded & k_wrap), "wrap already recorded");

  m_wrap = _wrap;
  m_recorded |= k_wrap;
}

// texture2D
texture2D::texture2D(frontend* _frontend)
  : texture{_frontend, resource::type::k_texture2D}
{
}

texture2D::~texture2D() {
}

void texture2D::write(const rx_byte* _data, rx_size _level) {
  RX_ASSERT(_data, "_data is null");
  RX_ASSERT(_level < levels(), "mipmap level out of bounds");
  validate();

  const auto& info{m_levels[_level]};
  memcpy(m_data.data() + info.offset, _data, info.size);
}

rx_byte* texture2D::map(rx_size _level) {
  RX_ASSERT(_level < levels(), "mipmap level out of bounds");
  validate();

  return m_data.data() + m_levels[_level].offset;
}

void texture2D::record_dimensions(const math::vec2z& _dimensions) {
  RX_ASSERT(!(m_recorded & k_dimensions), "dimensions already recorded");

  RX_ASSERT(m_recorded & k_type, "type not recorded");
  RX_ASSERT(m_recorded & k_filter, "filter not recorded");
  RX_ASSERT(m_recorded & k_format, "format not recorded");

  m_dimensions = _dimensions;
  m_dimensions_log2 = m_dimensions.map(math::log2<rx_size>);
  m_recorded |= k_dimensions;

  if (m_type != type::k_attachment) {
    dimension_type dimensions{m_dimensions};
    rx_size offset{0};
    const auto bpp{byte_size_of_format(m_format)};
    for (rx_size i{0}; i < levels(); i++) {
      const rx_size size{dimensions.area() * bpp};
      m_levels.push_back({offset, size, dimensions});
      offset += size;
      dimensions /= 2;
    }

    m_data.resize(offset);
    update_resource_usage(m_data.size());
  }
}

void texture2D::record_wrap(const wrap_options& _wrap) {
  RX_ASSERT(!(m_recorded & k_wrap), "wrap already recorded");

  m_wrap = _wrap;
  m_recorded |= k_wrap;
}

// texture3D
texture3D::texture3D(frontend* _frontend)
  : texture{_frontend, resource::type::k_texture3D}
{
}

texture3D::~texture3D() {
}

void texture3D::write(const rx_byte* _data, rx_size _level) {
  RX_ASSERT(_data, "_data is null");
  RX_ASSERT(_level < levels(), "mipmap level out of bounds");
  validate();

  const auto& info{m_levels[_level]};
  memcpy(m_data.data() + info.offset, _data, info.size);
}

rx_byte* texture3D::map(rx_size _level) {
  RX_ASSERT(_level < levels(), "mipmap level out of bounds");
  validate();

  return m_data.data() + m_levels[_level].offset;
}

void texture3D::record_dimensions(const math::vec3z& _dimensions) {
  RX_ASSERT(!(m_recorded & k_dimensions), "dimensions already recorded");

  RX_ASSERT(m_recorded & k_type, "type not recorded");
  RX_ASSERT(m_recorded & k_filter, "filter not recorded");
  RX_ASSERT(m_recorded & k_format, "format not recorded");

  m_dimensions = _dimensions;
  m_dimensions_log2 = m_dimensions.map(math::log2<rx_size>);
  m_recorded |= k_dimensions;

  if (m_type != type::k_attachment) {
    dimension_type dimensions{m_dimensions};
    rx_size offset{0};
    const auto bpp{byte_size_of_format(m_format)};
    for (rx_size i{0}; i < levels(); i++) {
      const rx_size size{dimensions.area() * bpp};
      m_levels.push_back({offset, size, dimensions});
      offset += size;
      dimensions /= 2;
    }

    m_data.resize(offset);
    update_resource_usage(m_data.size());
  }
}

void texture3D::record_wrap(const wrap_options& _wrap) {
  RX_ASSERT(!(m_recorded & k_wrap), "wrap already recorded");

  m_wrap = _wrap;
  m_recorded |= k_wrap;
}

// textureCM
textureCM::textureCM(frontend* _frontend)
  : texture{_frontend, resource::type::k_textureCM}
{
}

textureCM::~textureCM() {
}

void textureCM::write(const rx_byte* _data, face _face, rx_size _level) {
  RX_ASSERT(_data, "_data is null");
  RX_ASSERT(_level < levels(), "mipmap level out of bounds");
  validate();

  const auto& info{m_levels[_level]};
  memcpy(m_data.data() + info.offset + info.dimensions.area()
    * static_cast<rx_size>(_face), _data, info.size);
}

rx_byte* textureCM::map(rx_size _level, face _face) {
  RX_ASSERT(_level < levels(), "mipmap level out of bounds");
  validate();

  const auto& info{m_levels[_level]};
  return m_data.data() + info.offset + info.dimensions.area()
    * static_cast<rx_size>(_face);
}

void textureCM::record_dimensions(const math::vec2z& _dimensions) {
  RX_ASSERT(!(m_recorded & k_dimensions), "dimensions already recorded");

  RX_ASSERT(m_recorded & k_type, "type not recorded");
  RX_ASSERT(m_recorded & k_filter, "filter not recorded");
  RX_ASSERT(m_recorded & k_format, "format not recorded");

  m_dimensions = _dimensions;
  m_dimensions_log2 = m_dimensions.map(math::log2<rx_size>);
  m_recorded |= k_dimensions;

  if (m_type != type::k_attachment) {
    dimension_type dimensions{m_dimensions};
    rx_size offset{0};
    const auto bpp{byte_size_of_format(m_format)};
    for (rx_size i{0}; i < levels(); i++) {
      const rx_size size{dimensions.area() * bpp * 6};
      m_levels.push_back({offset, size, dimensions});
      offset += size;
      dimensions /= 2;
    }

    m_data.resize(offset);
    update_resource_usage(m_data.size());
  }
}

void textureCM::record_wrap(const wrap_options& _wrap) {
  RX_ASSERT(!(m_recorded & k_wrap), "wrap already recorded");

  m_wrap = _wrap;
  m_recorded |= k_wrap;
}

} // namespace rx::render