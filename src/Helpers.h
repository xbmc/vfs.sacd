/*
 *      Copyright (C) 2005-2019 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <kodi/General.h>
#include <mutex>

namespace
{

inline const char* kodiTranslateLogLevel(const AddonLog logLevel)
{
  switch (logLevel)
  {
    case ADDON_LOG_DEBUG:
      return "LOG_DEBUG:   ";
    case ADDON_LOG_INFO:
      return "LOG_INFO:    ";
    case ADDON_LOG_NOTICE:
      return "LOG_NOTICE:  ";
    case ADDON_LOG_WARNING:
      return "LOG_WARNING: ";
    case ADDON_LOG_ERROR:
      return "LOG_ERROR:   ";
    case ADDON_LOG_SEVERE:
      return "LOG_SEVERE:  ";
    case ADDON_LOG_FATAL:
      return "LOG_FATAL:   ";
    default:
      break;
  }
  return "LOG_UNKNOWN: ";
}

inline void kodiLog(const AddonLog logLevel, const char* format, ...)
{
  char buffer[16384];
  va_list args;
  va_start(args, format);
  vsprintf(buffer, format, args);
  va_end(args);

  kodi::Log(logLevel, buffer);
#ifdef DEBUG
  fprintf(stderr, "%s%s\n", kodiTranslateLogLevel(logLevel), buffer);
#endif
}

} // namespace

namespace ThreadHelpers
{

class PreventCopy
{
public:
  inline PreventCopy(void) = default;
  inline ~PreventCopy(void) = default;

private:
  inline PreventCopy(const PreventCopy &c) { *this = c; }
  inline PreventCopy &operator=(const PreventCopy &c){ (void)c; return *this; }
};

class CMutex : public PreventCopy
{
public:
  inline CMutex(void) : m_iLockCount(0)
  {
  }

  inline ~CMutex(void)
  {
    Clear();
  }

  inline bool TryLock(void)
  {
    if (m_mutex.try_lock())
    {
      ++m_iLockCount;
      return true;
    }
    return false;
  }

  inline bool Lock(void)
  {
    m_mutex.lock();
    ++m_iLockCount;
    return true;
  }

  inline void Unlock(void)
  {
    if (Lock())
    {
      if (m_iLockCount >= 2)
      {
        --m_iLockCount;
        m_mutex.unlock();
      }

      --m_iLockCount;
      m_mutex.unlock();
    }
  }

  inline bool Clear(void)
  {
    bool bReturn(false);
    if (TryLock())
    {
      unsigned int iLockCount = m_iLockCount;
      for (unsigned int iPtr = 0; iPtr < iLockCount; iPtr++)
        Unlock();
      bReturn = true;
    }
    return bReturn;
  }

private:
  std::recursive_mutex m_mutex;
  volatile unsigned int m_iLockCount;
};

class CLockObject : public PreventCopy
{
public:
  inline CLockObject(CMutex &mutex, bool bClearOnExit = false) :
    m_mutex(mutex),
    m_bClearOnExit(bClearOnExit)
  {
    m_mutex.Lock();
  }

  inline ~CLockObject(void)
  {
    if (m_bClearOnExit)
      Clear();
    else
      Unlock();
  }

  inline bool TryLock(void)
  {
    return m_mutex.TryLock();
  }

  inline void Unlock(void)
  {
    m_mutex.Unlock();
  }

  inline bool Clear(void)
  {
    return m_mutex.Clear();
  }

  inline bool Lock(void)
  {
    return m_mutex.Lock();
  }

private:
  CMutex &m_mutex;
  bool m_bClearOnExit;
};

} // namespace ThreadHelpers