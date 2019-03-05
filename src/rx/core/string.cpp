#include <string.h> // strcmp, memcpy
#include <stdarg.h> // va_{list, start, end, copy}
#include <stdio.h> // vsnprintf

#include <rx/core/algorithm.h> // swap
#include <rx/core/debug.h> // RX_MESSAGE
#include <rx/core/string.h> // string

namespace rx {

static void format_va(string& contents, const char* format, va_list va) {
  // calculate length to format
  va_list ap;
  va_copy(ap, va);
  const int length{vsnprintf(nullptr, 0, format, ap)};
  va_end(ap);

  // format into string
  contents.resize(length);
  vsnprintf(contents.data(), contents.size() + 1, format, va);
}

string string::formatter(memory::allocator* alloc, const char* format, ...) {
  va_list va;
  va_start(va, format);
  string contents{alloc};
  format_va(contents, format, va);
  va_end(va);
  return contents;
}

string::string(memory::allocator* alloc)
  : m_allocator{alloc}
  , m_data{k_small_string, reinterpret_cast<rx_byte*>(m_buffer)}
  , m_last{m_buffer}
{
  RX_ASSERT(m_allocator, "null allocator");

  resize(0);
}

string::string(memory::allocator* alloc, const string& contents)
  : string{alloc}
{
  reserve(contents.size());
  append(contents.data(), contents.size());
}

string::string(memory::allocator* alloc, const char* contents)
  : string{alloc}
{
  if (contents) {
    const rx_size size{strlen(contents)};
    reserve(size);
    append(contents, size);
  }
}

string::string(memory::allocator* alloc, const char* contents, rx_size size)
  : string{alloc}
{
  reserve(size);
  append(contents, size);
}

string::string(memory::allocator* alloc, const char* begin, const char* end)
  : string{alloc}
{
  reserve(end - begin);
  append(begin, end);
}

string::string(string&& contents)
  : m_allocator{contents.m_allocator}
{
  RX_ASSERT(m_allocator, "null allocator");

  if (contents.m_data.data() == contents.m_buffer) {
    m_data = {k_small_string, m_buffer};
    m_last = m_buffer;
    reserve(contents.size());
    append(contents.data(), contents.size());
  } else {
    m_data = utility::move(contents.m_data);
    m_last = contents.m_last;
  }

  contents.m_data = {k_small_string, contents.m_buffer};
  contents.m_last = contents.m_buffer;
  contents.resize(0);
}

string::string(const string& contents)
  : string{contents.m_allocator, contents}
{
}

string::~string() {
  if (m_data.data() != m_buffer) {
    m_allocator->deallocate(utility::move(m_data));
  }
}

string& string::operator=(const string& contents) {
  string(contents).swap(*this);
  return *this;
}

string& string::operator=(string&& contents) {
  string(utility::move(contents)).swap(*this);
  return *this;
}

void string::reserve(rx_size capacity) {
  if (m_data.data() + capacity + 1 <= m_data.end()) {
    return;
  }

  memory::block data;
  const auto size{static_cast<rx_size>(m_last - m_data.data())};
  if (m_data.data() == m_buffer) {
    data = utility::move(m_allocator->allocate(capacity + 1));
    if (size) {
      RX_MESSAGE("string \"%s\" fell through small size optimization", m_data.data());
      memcpy(data.data(), m_data.data(), size + 1);
    }
    m_data = utility::move(data);
  } else {
    m_data = utility::move(m_allocator->reallocate(m_data, capacity + 1));
  }

  m_last = m_data.data() + size;
}

void string::resize(rx_size size) {
  const auto previous_size{static_cast<rx_size>(m_last - m_data.data())};

  reserve(size);

  if (size > previous_size) {
    rx_byte* element{m_last};
    rx_byte* end{m_data.data() + size + 1};
    for (; element < end; ++element) {
      *element = 0;
    }
  } else if (m_last != m_data.data()) {
    m_data.data()[size] = 0;
  }

  m_last = m_data.data() + size;
}

string& string::append(const char* first, const char *last) {
  const auto new_size{static_cast<rx_size>(m_last - m_data.data() + last - first + 1)};
  if (m_data.data() + new_size > m_data.end()) {
    reserve((new_size * 3) / 2);
  }

  const auto length{static_cast<rx_size>(last - first)};
  memcpy(m_last, first, length);
  m_last += length;
  *m_last = 0;

  return *this;
}

string& string::append(const char* contents) {
  return append(contents, strlen(contents));
}

string string::lstrip(const char* set) const {
  const char* ch{m_data.cast<const char*>()};
  for (; strchr(set, *ch); ch++);
  return {m_allocator, ch};
}

string string::rstrip(const char* set) const {
  const char* ch{m_data.cast<const char*>() + size()};
  for (; strchr(set, *ch); ch--);
  return {m_allocator, m_data.cast<const char*>(), ch};
}

string string::substring(rx_size offset, rx_size length) const {
  const char* begin{m_data.cast<const char*>() + offset};
  RX_ASSERT(begin < data() + size(), "out of bounds");
  if (length == 0) {
    return {m_allocator, begin};
  }
  RX_ASSERT(begin + length < data() + size(), "out of bounds");
  return {m_allocator, begin, begin + length};
}

rx_size string::scan(const char* scan_format, ...) const {
  va_list va;
  va_start(va, scan_format);
  const auto result{vsscanf(data(), scan_format, va)};
  va_end(va);
  return static_cast<rx_size>(result);
}

array<string> string::split(int token, rx_size count) const {
  bool quoted{false};
  bool limit{count > 0};
  array<string> result;

  // when there is a limit we can reserve the storage upfront
  if (limit) {
    result.reserve(count);
  }

  result.push_back("");
  count--;

  for (const char* ch{m_data.cast<const char*>()}; *ch; ch++) {
    // handle escapes of quoted strings
    if (*ch == '\\' && (ch[1] == '\\' || ch[1] == '\"')) {
      if (ch[1] == '\\') {
        result.last() += "\\";
      }

      if (ch[1] == '\"') {
        result.last() += "\"";
      }

      ch += 2;
      continue;
    }

    // handle quoted strings
    if (*ch == '\"') {
      quoted = !quoted;
      continue;
    }

    if (*ch == token && !quoted && (!limit || count)) {
      result.push_back("");
      count--;
    } else {
      result.last() += *ch;
    }
  }

  return result;
}

char string::pop_back() {
  if (m_last == m_data.data()) {
    return *data();
  }
  m_last--;
  const char last{static_cast<char>(*m_last)};
  *m_last = 0;
  return last;
}

// complicated because of small string optimization
void string::swap(string& other) {
  rx::swap(m_data, other.m_data);
  rx::swap(m_last, other.m_last);

  rx_byte buffer[k_small_string];

  if (m_data.data() == other.m_buffer) {
    const rx_byte* element{other.m_buffer};
    const rx_byte* end{m_last};
    rx_byte* out{buffer};
    while (element != end) {
      *out++ = *element++;
    }
  }

  if (other.m_data.data() == m_buffer) {
    other.m_last = other.m_buffer + other.size();
    other.m_data = {k_small_string, other.m_buffer};

    rx_byte* element{other.m_data.data()};
    const rx_byte* end{other.m_last};
    const rx_byte* in{m_buffer};
    while (element != end) {
      *element++ = *in++;
    }

    *other.m_last = 0;
  }

  if (m_data.data() == other.m_buffer) {
    m_last = m_buffer + size();
    m_data = {k_small_string, m_buffer};

    rx_byte* element{m_data.data()};
    const rx_byte* end{m_last};
    const rx_byte* in{buffer};
    while (element != end) {
      *element++ = *in++;
    }

    *m_last = 0;
  }
}

bool operator==(const string& lhs, const string& rhs) {
  if (&lhs == &rhs) {
    return true;
  }

  if (lhs.size() != rhs.size()) {
    return false;
  }

  return !strcmp(lhs.data(), rhs.data());
}

bool operator!=(const string& lhs, const string& rhs) {
  if (&lhs == &rhs) {
    return false;
  }

  if (lhs.size() != rhs.size()) {
    return true;
  }

  return strcmp(lhs.data(), rhs.data());
}

bool operator<(const string& lhs, const string& rhs) {
  return &lhs == &rhs ? false : strcmp(lhs.data(), rhs.data()) < 0;
}

bool operator>(const string& lhs, const string& rhs) {
  return &lhs == &rhs ? false : strcmp(lhs.data(), rhs.data()) > 0;
}

} // namespace rx
