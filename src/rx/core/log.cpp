#include <time.h> // time_t, tm, strftime
#include <string.h> // strlen

#include "rx/core/log.h" // log
#include "rx/core/string.h" // string
#include "rx/core/vector.h" // vector
#include "rx/core/debug.h" // RX_MESSAGE
#include "rx/core/algorithm/max.h"

#include "rx/core/filesystem/file.h" // file

#include "rx/core/concurrency/mutex.h" // mutex
#include "rx/core/concurrency/condition_variable.h" // condition_variable
#include "rx/core/concurrency/scope_lock.h" // scope_lock
#include "rx/core/concurrency/scope_unlock.h" // scope_unlock
#include "rx/core/concurrency/thread.h" // thread

namespace rx {

static constexpr const rx_size k_flush_threshold{1000}; // 1000 messages

static inline const char*
get_level_string(log::level lvl) {
  switch (lvl) {
  case log::level::k_warning:
    return "warning";
  case log::level::k_info:
    return "info";
  case log::level::k_verbose:
    return "verbose";
  case log::level::k_error:
    return "error";
  }
  return nullptr;
}

static inline string
time_stamp(time_t time, const char* fmt) {
  struct tm tm;
#if defined(RX_PLATFORM_WINDOWS)
  localtime_s(&tm, &time);
#else
  localtime_r(&time, &tm);
#endif
  char date[256];
  strftime(date, sizeof date, fmt, &tm);
  date[sizeof date - 1] = '\0';
  return date;
}

struct message {
  message() = default;
  message(const log* owner, string&& contents, log::level level, time_t time)
    : m_owner{owner}
    , m_contents{utility::move(contents)}
    , m_level{level}
    , m_time{time}
  {
  }
  const log* m_owner;
  string m_contents;
  log::level m_level;
  time_t m_time;
};

struct logger {
  logger();
  ~logger();

  void write(const log* owner, string&& contents, log::level level, time_t time);
  void flush(rx_size max_padding);

  void process(int thread_id); // running in |m_thread|

  enum {
    k_running = 1 << 0,
    k_ready = 1 << 1
  };

  filesystem::file m_file;
  vector<static_node*> m_logs;
  rx_size m_max_name_length;
  rx_size m_max_level_length;
  int m_status;
  concurrency::mutex m_mutex;
  vector<message> m_queue; // protected by |m_mutex|
  concurrency::condition_variable m_ready_condition;
  concurrency::condition_variable m_flush_condition;
  concurrency::thread m_thread;
};

logger::logger()
  : m_file{"log.log", "w"}
  , m_max_name_length{0}
  , m_max_level_length{0}
  , m_status{k_running}
  , m_thread{"logger", [this](int id){ process(id); }}
{
  RX_MESSAGE("starting logger... (messages will be flushed once started)");

  // initialize all loggers
  static_globals::each([this](static_node* node) {
    const char* name{node->name()};
    if (!strncmp(name, "log_", 4)) {
      node->init();

      // remember for finalization
      m_logs.push_back(node);

      // calculate maximum name length
      const auto name_length{strlen(name)};
      m_max_name_length = algorithm::max(m_max_name_length, name_length);
    }
    return true;
  });

  // calculate maximum level string length
  const auto level0{strlen(get_level_string(log::level::k_warning))};
  const auto level1{strlen(get_level_string(log::level::k_info))};
  const auto level2{strlen(get_level_string(log::level::k_verbose))};
  const auto level3{strlen(get_level_string(log::level::k_error))};

  m_max_level_length = algorithm::max(level0, level1, level2, level3);

  // signal the logging thread to begin
  {
    concurrency::scope_lock locked(m_mutex);
    m_status |= k_ready;
    m_ready_condition.signal();
  }
}

logger::~logger() {
  // signal shutdown
  {
    concurrency::scope_lock locked(m_mutex);
    m_status &= ~k_running;
    m_flush_condition.signal();
  }

  // join thread
  m_thread.join();

  // deinitialize all loggers
  m_logs.each_rev([](static_node* node) {
    node->fini();
  });
}

void logger::write(const log* owner, string&& contents, log::level level, time_t time) {
  concurrency::scope_lock locked(m_mutex);
  m_queue.emplace_back(owner, utility::move(contents), level, time);
  //if (m_queue.size() >= k_flush_threshold) {
  m_flush_condition.signal();
  //}
}

void logger::flush(rx_size max_padding) {
  m_queue.each_fwd([&](const message& _message) {
    const auto name_string{_message.m_owner->name()};
    const auto level_string{get_level_string(_message.m_level)};
    const auto padding{strlen(name_string) + strlen(level_string) + 4}; // 4 additional characters for " []/"
    m_file.print("[%s] [%s/%s]%*s | %s\n",
      time_stamp(_message.m_time, "%Y-%m-%d %H:%M:%S"),
      name_string,
      level_string,
      static_cast<int>(max_padding-padding),
      "",
      _message.m_contents);
    return true;
  });
  m_file.flush();
  m_queue.clear();
}

void logger::process(int thread_id) {
  concurrency::scope_lock locked(m_mutex);

  // wait until ready
  m_ready_condition.wait(locked, [this]{ return m_status & k_ready; });

  {
    // don't hold a lock for RX_MESSAGE since it acquires this same lock
    concurrency::scope_unlock unlocked(m_mutex);
    RX_MESSAGE("logger started on thread %d", thread_id);
    RX_MESSAGE("flushed all messages above at this time");
  }

  const auto max_padding{m_max_name_length + m_max_level_length};
  flush(max_padding);

  while (m_status & k_running) {
    flush(max_padding);

    // wait for the next flush operation
    m_flush_condition.wait(locked);
  }

  // flush any contents at thread exit
  flush(max_padding);

  RX_ASSERT(m_queue.is_empty(), "not all contents flushed");
}

static RX_GLOBAL<logger> g_logger{"log"};

void log::write(log::level level, string&& contents) {
  g_logger->write(this, utility::move(contents), level, time(nullptr));
}

} // namespace rx
