/*
 *  Copyright (C) 2020 Team Kodi <https://kodi.tv>
 *  Copyright (c) 2011-2019 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

/*
 * Code taken SACD Decoder plugin (from foo_input_sacd) by Team Kodi.
 * Original author Maxim V.Anisiutkin.
 */

#include "sacd_media.h"

#include "scarletbook.h"

sacd_media_disc_t::~sacd_media_disc_t()
{
  close();
}

bool sacd_media_disc_t::open(const std::string& path, bool write)
{
  //   open_reason = reason;
  //   TCHAR device[8];
  //   _tcscpy_s(device, 8, _T("\\\\.\\?:"));
  //   device[4] = path[7];
  //   media_disc = ::CreateFile(device, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
  //   if (media_disc == INVALID_HANDLE_VALUE) {
  //     media_disc = ::CreateFile(device, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
  //   }
  //   if (media_disc != INVALID_HANDLE_VALUE) {
  //     file_position = 0;
  //     return true;
  //   }
  return false;
}

bool sacd_media_disc_t::close()
{
  //   if (media_disc == INVALID_HANDLE_VALUE) {
  return true;
  //   }
  //   BOOL hr = CloseHandle(media_disc);
  //   media_disc    = INVALID_HANDLE_VALUE;
  //   file_position = -1;
  //   return hr == TRUE;
}

bool sacd_media_disc_t::can_seek()
{
  return true;
}

bool sacd_media_disc_t::seek(int64_t position, int mode)
{
  switch (mode)
  {
    case SEEK_SET:
      file_position = position;
      break;
    case SEEK_CUR:
      file_position += position;
      break;
    case SEEK_END:
      file_position = get_size() - position;
      break;
  }
  return true;
}

std::shared_ptr<kodi::vfs::CFile> sacd_media_disc_t::get_handle()
{
  return std::shared_ptr<kodi::vfs::CFile>();
}

int64_t sacd_media_disc_t::get_position()
{
  return file_position;
}

int64_t sacd_media_disc_t::get_size()
{
  //   LARGE_INTEGER liFileSize;
  //   liFileSize.QuadPart = 0;
  //   ::GetFileSizeEx(media_disc, &liFileSize);
  //   return liFileSize.QuadPart;
  return -1;
}

kodi::vfs::FileStatus sacd_media_disc_t::get_stats()
{
  kodi::vfs::FileStatus filestats;
  //   ::GetFileTime(media_disc, NULL, NULL, (LPFILETIME)&filestats.m_timestamp);
  //   filestats.m_size = get_size();
  return filestats;
}

size_t sacd_media_disc_t::read(void* data, size_t size)
{
  //   int64_t read_start = file_position & (int64_t)(~(SACD_LSN_SIZE - 1));
  //   size_t read_length =
  //       (size_t)((file_position + size - read_start + SACD_LSN_SIZE - 1) / SACD_LSN_SIZE) *
  //       SACD_LSN_SIZE;
  //   size_t start_offset = file_position & (SACD_LSN_SIZE - 1);
  //   size_t end_offset = (file_position + size) & (SACD_LSN_SIZE - 1);
  //   LARGE_INTEGER liPosition;
  //   liPosition.QuadPart = read_start;
  //   if (::SetFilePointerEx(media_disc, liPosition, &liPosition, FILE_BEGIN) == FALSE)
  //   {
  //     return 0;
  //   }
  //   DWORD cbRead;
  bool bReadOk = true;
  //   if (start_offset == 0 && end_offset == 0)
  //   {
  //     file_position += size;
  //     if (::ReadFile(media_disc, data, (DWORD)size, &cbRead, NULL) == FALSE)
  //     {
  //       bReadOk = false;
  //     }
  //     return bReadOk ? size : 0;
  //   }
  //   uint8_t sacd_block[SACD_LSN_SIZE];
  //   size_t bytes_to_read;
  //   size_t copy_bytes;
  //   bytes_to_read = size;
  //   copy_bytes = min(SACD_LSN_SIZE - start_offset, bytes_to_read);
  //   if (::ReadFile(media_disc, sacd_block, SACD_LSN_SIZE, &cbRead, NULL) == FALSE)
  //   {
  //     bReadOk = false;
  //   }
  //   memcpy(data, sacd_block + start_offset, copy_bytes);
  //   data = (uint8_t*)data + copy_bytes;
  //   file_position += (int64_t)copy_bytes;
  //   bytes_to_read -= copy_bytes;
  //   if (bytes_to_read <= 0)
  //   {
  //     return bReadOk ? size : 0;
  //   }
  //   size_t full_blocks_to_read = (bytes_to_read - end_offset) / SACD_LSN_SIZE;
  //   if (full_blocks_to_read > 0)
  //   {
  //     if (::ReadFile(media_disc, data, full_blocks_to_read * SACD_LSN_SIZE, &cbRead, NULL) == FALSE)
  //     {
  //       bReadOk = false;
  //     }
  //     data = (uint8_t*)data + full_blocks_to_read * SACD_LSN_SIZE;
  //     file_position += (int64_t)(full_blocks_to_read * SACD_LSN_SIZE);
  //     bytes_to_read -= full_blocks_to_read * SACD_LSN_SIZE;
  //   }
  //   if (bytes_to_read <= 0)
  //   {
  //   }
  //   if (::ReadFile(media_disc, sacd_block, SACD_LSN_SIZE, &cbRead, NULL) == FALSE)
  //   {
  //     bReadOk = false;
  //   }
  //   memcpy(data, sacd_block, end_offset);
  //   data = (uint8_t*)data + cbRead;
  //   file_position += (int64_t)end_offset;
  return bReadOk ? size : 0;
}

size_t sacd_media_disc_t::write(const void* data, size_t size)
{
  return 0;
}

int64_t sacd_media_disc_t::skip(int64_t bytes)
{
  file_position += bytes;
  return file_position;
}

void sacd_media_disc_t::truncate(int64_t position)
{
}

void sacd_media_disc_t::on_idle()
{
}


sacd_media_file_t::sacd_media_file_t()
{
}

sacd_media_file_t::~sacd_media_file_t()
{
  close();
}

bool sacd_media_file_t::open(const std::string& path, bool write)
{
  m_path = path;
  std::shared_ptr<kodi::vfs::CFile> file = std::make_shared<kodi::vfs::CFile>();
  if (!write)
  {
    if (!file->OpenFile(path))
      return false;
  }
  else
  {
    if (!file->OpenFileForWrite(path))
      return false;
  }

  media_file = file;
  return true;
}

bool sacd_media_file_t::close()
{
  media_file.reset();
  return media_file.use_count() == 0;
}

bool sacd_media_file_t::can_seek()
{
  return media_file->IoControlGetSeekPossible();
}

bool sacd_media_file_t::seek(int64_t position, int mode)
{
  media_file->Seek(position, mode);
  return true;
}

std::shared_ptr<kodi::vfs::CFile> sacd_media_file_t::get_handle()
{
  return media_file;
}

int64_t sacd_media_file_t::get_position()
{
  return media_file->GetPosition();
}

int64_t sacd_media_file_t::get_size()
{
  return media_file->GetLength();
}

kodi::vfs::FileStatus sacd_media_file_t::get_stats()
{
  kodi::vfs::FileStatus stats;
  kodi::vfs::StatFile(m_path, stats);
  return stats;
}

size_t sacd_media_file_t::read(void* data, size_t size)
{
  return media_file->Read(data, size);
}

size_t sacd_media_file_t::write(const void* data, size_t size)
{
  media_file->Write(data, size);
  return size;
}

int64_t sacd_media_file_t::skip(int64_t bytes)
{
  return media_file->Seek(bytes, SEEK_CUR);
}

void sacd_media_file_t::truncate(int64_t position)
{
  media_file->Truncate(position);
}

void sacd_media_file_t::on_idle()
{
}
