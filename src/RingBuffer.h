/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Helpers.h"

class CRingBuffer
{
public:
  CRingBuffer() = default;
  ~CRingBuffer();

  bool Create(unsigned int size);
  void Destroy();
  void Clear();
  bool ReadData(char* buf, unsigned int size);
  bool ReadData(CRingBuffer& rBuf, unsigned int size);
  bool WriteData(const char* buf, unsigned int size);
  bool WriteData(CRingBuffer& rBuf, unsigned int size);
  bool SkipBytes(int skipSize);
  bool Append(CRingBuffer& rBuf);
  bool Copy(CRingBuffer& rBuf);
  char* getBuffer();
  unsigned int getSize();
  unsigned int getReadPtr() const;
  unsigned int getWritePtr();
  unsigned int getMaxReadSize();
  unsigned int getMaxWriteSize();

private:
  ThreadHelpers::CMutex m_critSection;

  char* m_buffer = nullptr;
  unsigned int m_size = 0;
  unsigned int m_readPtr = 0;
  unsigned int m_writePtr = 0;
  unsigned int m_fillCount = 0;
};
