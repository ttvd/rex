#include <string.h> // strcmp

#include "rx/core/statics.h"
#include "rx/core/debug.h" // RX_MESSAGE

#include "rx/core/concurrency/spin_lock.h"
#include "rx/core/concurrency/scope_lock.h"
#include "rx/core/concurrency/scope_unlock.h"

namespace rx {

static concurrency::spin_lock g_lock;

static static_node* g_head; // protected by |g_lock|
static static_node* g_tail; // protected by |g_lock|

void static_node::link() {
  concurrency::scope_lock locked(g_lock);

  if (!g_head) {
    g_head = this;
  }

  if (g_tail) {
    m_prev = g_tail;
    m_prev->m_next = this;
  }

  g_tail = this;
}

void static_globals::init() {
  RX_MESSAGE("init static globals");
  concurrency::scope_lock locked(g_lock);
  for (auto node{g_head}; node; node = node->m_next) {
    concurrency::scope_unlock unlocked(g_lock);
    node->init_global();
  }
}

void static_globals::fini() {
  RX_MESSAGE("fini static globals");
  concurrency::scope_lock locked(g_lock);
  for (auto node{g_tail}; node; node = node->m_prev) {
    concurrency::scope_unlock unlocked(g_lock);
    node->fini_global();
  }
}

static_node* static_globals::find(const char* name) {
  concurrency::scope_lock locked(g_lock);
  for (auto node{g_head}; node; node = node->m_next) {
    if (!strcmp(node->m_name, name)) {
      return node;
    }
  }
  return nullptr;
}

#if 0
void static_globals::remove(static_node* node) {
  concurrency::scope_lock locked(g_lock);
  if (node->m_next) {
    node->m_next->m_prev = node->m_prev;
  }

  if (node->m_prev) {
    node->m_prev->m_next = node->m_next;
  }

  if (g_head == node) {
    g_head = node->m_next;
  }

  if (g_tail == node) {
    g_tail = node->m_prev;
  }
}
#endif

void static_globals::lock() {
  g_lock.lock();
}

void static_globals::unlock() {
  g_lock.unlock();
}

static_node* static_globals::head() {
  return g_head;
}

static_node* static_globals::tail() {
  return g_tail;
}

} // namespace rx
