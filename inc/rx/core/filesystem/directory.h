#ifndef RX_CORE_FILESYSTEM_DIRECTORY_H
#define RX_CORE_FILESYSTEM_DIRECTORY_H

#include <rx/core/string.h>
#include <rx/core/function.h>
#include <rx/core/memory/system_allocator.h>

namespace rx::filesystem {

struct directory {
  directory(const char* _path);
  directory(const string& _path);
  directory(memory::allocator* _allocator, const char* _path);
  directory(memory::allocator* _allocator, const string& _path);
  ~directory();

  struct item {
    enum class type : rx_u8 {
      k_file,
      k_directory
    };

    bool is_file() const;
    bool is_directory() const;
    const string& name() const;

  private:
    friend struct directory;

    item(memory::allocator* _allocator, const char* _name, type _type);

    string m_name;
    type m_type;
  };

  operator bool() const;

  // enumerate directory with |_function| being called for each item
  // NOTE: does not consider hidden files, symbolic links, block devices, or ..
  void each(function<void(const item&)>&& _function);

private:
  memory::allocator* m_allocator;
  void* m_impl;
};

inline directory::directory(const char* _path)
  : directory{&memory::g_system_allocator, _path}
{
}

inline directory::directory(const string& _path)
  : directory{&memory::g_system_allocator, _path}
{
}

inline directory::directory(memory::allocator* _allocator, const string& _path)
  : directory{_allocator, _path.data()}
{
}

inline bool directory::item::is_file() const {
  return m_type == type::k_file;
}

inline bool directory::item::is_directory() const {
  return m_type == type::k_directory;
}

inline const string& directory::item::name() const {
  return m_name;
}

inline directory::item::item(memory::allocator* _allocator, const char* _name, type _type)
  : m_name{_allocator, _name}
  , m_type{_type}
{
  // {empty}
}

} // namespace rx::filesystem

#endif // RX_CORE_FILESYSTEM_DIRECTORY_H