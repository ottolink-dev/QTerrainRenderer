#pragma once

#ifdef QTR_ENABLE_TRACY
#include <string_view>
#include <tracy/Tracy.hpp>
#ifndef TRACY_ENABLE
#error QTR_ENABLE_TRACY requires a Tracy client built with TRACY_ENABLE
#endif

#define QTR_PROFILE_ZONE(name) ZoneScopedN(name)
// Evaluate text only during capture, and keep temporaries alive while Tracy copies it.
#define QTR_PROFILE_TEXT(text)                                                           \
  do                                                                                     \
  {                                                                                      \
    if (TracyIsConnected)                                                                \
    {                                                                                    \
      const auto            &qtr_profile_text = (text);                                  \
      const std::string_view qtr_view(qtr_profile_text);                                 \
      ZoneText(qtr_view.data(), qtr_view.size());                                        \
    }                                                                                    \
  } while (false)
#else
#define QTR_PROFILE_ZONE(name)
#define QTR_PROFILE_TEXT(text)                                                           \
  do                                                                                     \
  {                                                                                      \
  } while (false)
#endif
