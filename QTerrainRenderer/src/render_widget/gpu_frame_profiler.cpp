#include "gpu_frame_profiler.hpp"
#include "qtr/windows_patch.hpp"

#ifdef QTR_ENABLE_TRACY
#include "qtr/logger.hpp"
#include "qtr/profiling.hpp"
#include <list>
#include <mutex>
#include <string>

namespace qtr
{
namespace
{
uint64_t connection_id()
{
#ifdef TRACY_ON_DEMAND
  return tracy::GetProfiler().ConnectionId();
#else
  return 0;
#endif
}

const char *plot_name_for(const void *renderer)
{
  // Tracy retains plot-name pointers. Keep their strings alive for the process,
  // including after a widget is destroyed; one small string per context lifetime.
  static auto      *names = new std::list<std::string>;
  static std::mutex mutex;
  static uint64_t   serial = 0;
  std::lock_guard   lock(mutex);
  names->push_back("QTR GPU paintGL ms #" + std::to_string(++serial) + " renderer=" +
                   std::to_string(reinterpret_cast<std::uintptr_t>(renderer)));
  return names->back().c_str();
}
} // namespace

GpuFrameProfiler::GpuFrameProfiler(QOpenGLContext *ctx, const void *renderer)
    : context(ctx), plot_name(plot_name_for(renderer))
{
  if (QOpenGLContext::currentContext() != ctx || !initializeOpenGLFunctions())
    return;
  glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &counter_bits);
  if (counter_bits <= 0)
  {
    Logger::log()->info("{}: GPU timestamps unsupported; CPU zones remain enabled",
                        plot_name);
    return;
  }
  for (auto &slot : query_slots)
  {
    glGenQueries(1, &slot.begin);
    glGenQueries(1, &slot.end);
  }
  Logger::log()->info("{}: {}-bit timestamps, 8 pending frames maximum",
                      plot_name,
                      counter_bits);
}

GpuFrameProfiler::~GpuFrameProfiler()
{
  if (context && QOpenGLContext::currentContext() == context && counter_bits > 0)
    for (auto &slot : query_slots)
    {
      glDeleteQueries(1, &slot.begin);
      glDeleteQueries(1, &slot.end);
    }
  // If the context has already gone away, its GL objects are reclaimed by GL.
  size_t pending = 0;
  for (const auto &slot : query_slots)
    if (slot.pending)
      ++pending;
  Logger::log()->info(
      "{}: {} completed samples, {} skipped frames, {} pending at teardown",
      plot_name,
      samples,
      skipped,
      pending);
}

void GpuFrameProfiler::collect()
{
  // Visit pending frames in submission order, including after the ring wraps.
  for (size_t offset = 0; offset < query_slots.size(); ++offset)
  {
    auto &slot = query_slots[(next_slot + offset) % query_slots.size()];
    if (!slot.pending)
      continue;
    GLint available = GL_FALSE;
    glGetQueryObjectiv(slot.end, GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available)
      continue; // Never block waiting for GPU work.
    GLuint64 start = 0, finish = 0;
    glGetQueryObjectui64v(slot.begin, GL_QUERY_RESULT, &start);
    glGetQueryObjectui64v(slot.end, GL_QUERY_RESULT, &finish);
    slot.pending = false;
    if (TracyIsConnected && slot.connection == connection_id())
    {
      uint64_t elapsed = finish - start;
      if (counter_bits < 64)
        elapsed &= (uint64_t(1) << counter_bits) - 1;
      TracyPlot(plot_name, static_cast<double>(elapsed) / 1e6);
      ++samples;
    }
  }
}

void GpuFrameProfiler::begin()
{
  if (counter_bits <= 0 || !context || QOpenGLContext::currentContext() != context)
    return;
  collect();
  if (!TracyIsConnected)
    return;
  if (!query_slots[next_slot].pending)
  {
    active = static_cast<int>(next_slot);
    query_slots[next_slot].connection = connection_id();
    glQueryCounter(query_slots[next_slot].begin, GL_TIMESTAMP);
    next_slot = (next_slot + 1) % query_slots.size();
    return;
  }
  ++skipped; // A saturated pool drops a measurement rather than stalling a frame.
}

void GpuFrameProfiler::end()
{
  if (active < 0)
    return;
  if (!context || QOpenGLContext::currentContext() != context)
  {
    active = -1;
    return;
  }
  auto &slot = query_slots[active];
  glQueryCounter(slot.end, GL_TIMESTAMP);
  slot.pending = true;
  active = -1;
  collect();
}
} // namespace qtr
#endif
