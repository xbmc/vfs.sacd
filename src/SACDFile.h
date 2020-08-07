/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "sacd/sacd_core.h"

#include <kodi/addon-instance/VFS.h>

struct SACDContext;

class ATTRIBUTE_HIDDEN CSACDFile : public kodi::addon::CInstanceVFS
{
public:
  CSACDFile(KODI_HANDLE instance, const std::string& version) : CInstanceVFS(instance, version) {}
  kodi::addon::VFSFileHandle Open(const kodi::addon::VFSUrl& url) override;
  ssize_t Read(kodi::addon::VFSFileHandle context, uint8_t* lpBuf, size_t uiBufSize) override;
  bool Close(kodi::addon::VFSFileHandle context) override;
  int64_t GetLength(kodi::addon::VFSFileHandle context) override;
  int64_t GetPosition(kodi::addon::VFSFileHandle context) override;
  int Stat(const kodi::addon::VFSUrl& url, kodi::vfs::FileStatus& buffer) override;
  bool IoControlGetSeekPossible(kodi::addon::VFSFileHandle context) override;
  bool ContainsFiles(const kodi::addon::VFSUrl& url,
                     std::vector<kodi::vfs::CDirEntry>& items,
                     std::string& rootPath) override;
  bool GetDirectory(const kodi::addon::VFSUrl& url,
                    std::vector<kodi::vfs::CDirEntry>& items,
                    CVFSCallbacks callbacks) override
  {
    std::string rpath;
    return ContainsFiles(url, items, rpath);
  }

  std::vector<uint8_t> id3_buffer;
  //   service_ptr_t<std_wavpack_input_t> std_wavpack_input;

  uint32_t GetSubsongCount(SACDContext& sacdContext);
  uint32_t GetSubsong(SACDContext& sacdContext, uint32_t p_index);
};
