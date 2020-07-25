/*
 *  Copyright (C) 2014 Arne Morten Kvarving
 *  Copyright (C) 2014-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <kodi/Filesystem.h>
#include <kodi/addon-instance/VFS.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include "RingBuffer.h"

extern "C"
{

#include "dsf.h"
#include "logging.h"
#include "sacd_reader.h"
#include "scarletbook.h"
#include "scarletbook_id3.h"
#include "scarletbook_read.h"
#include "scarletbook_output.h"
#include "scarletbook_print.h"

struct sacd_input_s
{
  void* fd;
  uint8_t* input_buffer;
  ssize_t total_sectors;
};


int sacd_vfs_input_authenticate(sacd_input_t dev)
{
  return 0;
}

int sacd_vfs_input_decrypt(sacd_input_t dev, uint8_t *buffer, int blocks)
{
  return 0;
}

/**
 * initialize and open a SACD device or file.
 */
sacd_input_t sacd_vfs_input_open(const char *target)
{
  sacd_input_t dev;

  /* Allocate the library structure */
  dev = static_cast<sacd_input_t>(calloc(sizeof(*dev), 1));
  if (dev == nullptr)
  {
    kodiLog(ADDON_LOG_ERROR, "%s: Could not allocate memory", __func__);
    return nullptr;
  }

  /* Open the device */
  kodi::vfs::FileStatus status;
  kodi::vfs::StatFile(target, status);
  dev->total_sectors = status.GetSize() / SACD_LSN_SIZE;
  kodi::vfs::CFile* file = new kodi::vfs::CFile;
  dev->fd = file;
  bool result = file->OpenFile(target, 0);
  if (!result)
  {
    goto error;
  }

  return dev;

error:

  delete file;
  free(dev);

  return 0;
}

/**
 * return the last error message
 */
char *sacd_vfs_input_error(sacd_input_t dev)
{
  return const_cast<char*>("unknown error");
}

/**
 * read data from the device.
 */
ssize_t sacd_vfs_input_read(sacd_input_t dev, int pos, int blocks, void *buffer)
{
  kodi::vfs::CFile* file = static_cast<kodi::vfs::CFile*>(dev->fd);
  file->Seek(pos*SACD_LSN_SIZE, SEEK_SET);
  return file->Read(buffer, blocks*SACD_LSN_SIZE)/SACD_LSN_SIZE;
}

/**
 * close the SACD device and clean up.
 */
int sacd_vfs_input_close(sacd_input_t dev)
{
  kodi::vfs::CFile* file = static_cast<kodi::vfs::CFile*>(dev->fd);
  delete file;

  free(dev);

  return 0;
}

uint32_t sacd_vfs_input_total_sectors(sacd_input_t dev)
{
  if (!dev)
    return 0;

  return dev->total_sectors;
}

static std::string URLDecode(const std::string& strURLData)
//modified to be more accomodating - if a non hex value follows a % take the characters directly and don't raise an error.
// However % characters should really be escaped like any other non safe character (www.rfc-editor.org/rfc/rfc1738.txt)
{
  std::string strResult;

  /* result will always be less than source */
  strResult.reserve( strURLData.length() );

  for (unsigned int i = 0; i < strURLData.size(); ++i)
  {
    int kar = (unsigned char)strURLData[i];
    if (kar == '+') strResult += ' ';
    else if (kar == '%')
    {
      if (i < strURLData.size() - 2)
      {
        std::string strTmp;
        strTmp.assign(strURLData.substr(i + 1, 2));
        int dec_num=-1;
        sscanf(strTmp.c_str(), "%x", reinterpret_cast<unsigned int*>(&dec_num));
        if (dec_num<0 || dec_num>255)
          strResult += kar;
        else
        {
          strResult += (char)dec_num;
          i += 2;
        }
      }
      else
        strResult += kar;
    }
    else strResult += kar;
  }

  return strResult;
}

static std::string URLEncode(const std::string& strURLData)
{
  std::string strResult;

  /* wonder what a good value is here is, depends on how often it occurs */
  strResult.reserve( strURLData.length() * 2 );

  for (size_t i = 0; i < strURLData.size(); ++i)
  {
    const char kar = strURLData[i];

    // Don't URL encode "-_.!()" according to RFC1738
    //! @todo Update it to "-_.~" after Gotham according to RFC3986
    if (std::isalnum(kar) || kar == '-' || kar == '.' || kar == '_' || kar == '!' || kar == '(' || kar == ')')
      strResult.push_back(kar);
    else
    {
      char temp[128];
      sprintf(temp,"%%%2.2X", (unsigned int)((unsigned char)kar));
      strResult += temp;
    }
  }

  return strResult;
}

struct SACDContext
{
  sacd_reader_t* reader = nullptr;
  scarletbook_handle_t* handle = nullptr;
  scarletbook_output_t* output = nullptr;
  scarletbook_output_format_t* ft = nullptr;
  uint32_t block_size = 0;
  uint32_t end_lsn = 0;
  uint32_t encrypted_start_1 = 0;
  uint32_t encrypted_start_2 = 0;
  uint32_t encrypted_end_1 = 0;
  uint32_t encrypted_end_2 = 0;
  uint32_t checked_for_non_encrypted_disc = 0;
  uint32_t non_encrypted_disc = 0;
  int encrypted = 0;
  uint8_t* frame_buffer = nullptr;
  CRingBuffer decode_buffer;
  int64_t pos = 0;
};

static void frame_read_callback(scarletbook_handle_t* handle, uint8_t* frame_data,
                                size_t frame_size, void* userdata)
{
  SACDContext* ctx = static_cast<SACDContext*>(userdata);

  size_t actual = (*ctx->ft->handler.write)(ctx->ft, frame_data, frame_size);
  ctx->decode_buffer.WriteData(reinterpret_cast<char*>(ctx->frame_buffer), actual);
  ctx->ft->write_length += actual;
}

static void frame_decoded_callback(uint8_t* frame_data, size_t frame_size, void* userdata)
{
  SACDContext* ctx = static_cast<SACDContext*>(userdata);
  dsf_handle_t* handle = static_cast<dsf_handle_t*>(ctx->ft->priv);

  size_t actual = (*ctx->ft->handler.write)(ctx->ft, frame_data, frame_size);
  ctx->decode_buffer.WriteData(reinterpret_cast<char*>(ctx->frame_buffer), actual);
  ctx->ft->write_length += actual;
}

static void frame_error_callback(int frame_count, int frame_error_code,
                                 const char *frame_error_message, void *userdata)
{
  kodiLog(ADDON_LOG_ERROR, "%s: ERROR decoding DST frame", __func__);
}

}

class ATTRIBUTE_HIDDEN CSACDFile : public kodi::addon::CInstanceVFS
{
public:
  CSACDFile(KODI_HANDLE instance, const std::string& version) : CInstanceVFS(instance, version) { }
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
  { std::string rpath; return ContainsFiles(url, items, rpath); }

  std::vector<uint8_t> id3_buffer;
};

kodi::addon::VFSFileHandle CSACDFile::Open(const kodi::addon::VFSUrl& url)
{
  std::string file(url.GetFilename());
  int track = strtol(file.substr(0,file.size()-4).c_str(), 0, 10);
  SACDContext* result = new SACDContext;
  result->reader = sacd_open(URLDecode(url.GetHostname()).c_str());
  if (!result->reader)
  {
    delete result;
    return nullptr;
  }
  result->handle = scarletbook_open(result->reader, 0);
  if (!result->handle)
  {
    sacd_close(result->reader);
    delete result;
    return nullptr;
  }

  std::string url2 = url.GetURL();
  result->output = scarletbook_output_create(result->handle, 0, 0, 0);
  scarletbook_output_enqueue_track(result->output, result->handle->twoch_area_idx,
                                   track-1, const_cast<char*>(url2.c_str()),
                                   const_cast<char*>("dsf"), 0);

  scarletbook_frame_init(result->handle);

  result->frame_buffer = new uint8_t[128*1024];
  result->decode_buffer.Create(1024*1024*10);

  id3_buffer.resize(128*1024);
  int len = scarletbook_id3_tag_render(result->handle, id3_buffer.data(),
                                       0, track-1);
  id3_buffer.resize(len);

  struct list_head* node_ptr = result->output->ripping_queue.next;
  result->ft = list_entry(node_ptr, scarletbook_output_format_t, siblings);
  result->ft->priv = calloc(1, result->ft->handler.priv_size);
  result->ft->fd = 0;

  // what blocks do we need to process?
  result->ft->current_lsn = result->ft->start_lsn;
  result->end_lsn = result->ft->start_lsn + result->ft->length_lsn;

  dsf_handle_t* handle = static_cast<dsf_handle_t*>(result->ft->priv);
  handle->header_size = (result->end_lsn-result->ft->start_lsn) * SACD_LSN_SIZE; // store approximate length here for header injection
  (*result->ft->handler.startwrite)(result->ft);

  // set the encryption range
  if (result->handle->area[0].area_toc != 0)
  {
    result->encrypted_start_1 = result->handle->area[0].area_toc->track_start;
    result->encrypted_end_1 = result->handle->area[0].area_toc->track_end;
  }
  if (result->handle->area[1].area_toc != 0)
  {
    result->encrypted_start_2 = result->handle->area[1].area_toc->track_start;
    result->encrypted_end_2 = result->handle->area[1].area_toc->track_end;
  }

  return result;
}

ssize_t CSACDFile::Read(kodi::addon::VFSFileHandle context, uint8_t* lpBuf, size_t uiBufSize)
{
  SACDContext* ctx = static_cast<SACDContext*>(context);

  // prepend header
  dsf_handle_t* handle = static_cast<dsf_handle_t*>(ctx->ft->priv);
  handle->data = ctx->frame_buffer;

  if (handle && ctx->pos < id3_buffer.size())
  {
    size_t tocopy = std::min(uiBufSize, static_cast<size_t>(id3_buffer.size()-ctx->pos));
    memcpy(lpBuf, id3_buffer.data()+ctx->pos, tocopy);
    ctx->pos += tocopy;
    return tocopy;
  }

  int header_pos = ctx->pos-id3_buffer.size();
  if (handle && header_pos < handle->header_size)
  {
    size_t tocopy = std::min(uiBufSize, static_cast<size_t>(handle->header_size-header_pos));
    memcpy(lpBuf, handle->header+header_pos, tocopy);
    ctx->pos += tocopy;
    return tocopy;
  }

  while (ctx->decode_buffer.getMaxReadSize() < 32*1024)
  {
    // decode some more data
    if (ctx->ft->current_lsn < ctx->end_lsn)
    {
      // check what block ranges are encrypted..
      if (ctx->ft->current_lsn < ctx->encrypted_start_1)
      {
        ctx->block_size = std::min(ctx->encrypted_start_1 - ctx->ft->current_lsn,
                                   MAX_PROCESSING_BLOCK_SIZE);
        ctx->encrypted = 0;
      }
      else if (ctx->ft->current_lsn >= ctx->encrypted_start_1 &&
               ctx->ft->current_lsn <= ctx->encrypted_end_1)
      {
        ctx->block_size = std::min(ctx->encrypted_end_1 + 1 - ctx->ft->current_lsn,
                                   MAX_PROCESSING_BLOCK_SIZE);
        ctx->encrypted = 1;
      }
      else if (ctx->ft->current_lsn > ctx->encrypted_end_1 &&
               ctx->ft->current_lsn < ctx->encrypted_start_2)
      {
        ctx->block_size = std::min(ctx->encrypted_start_2 - ctx->ft->current_lsn,
                                   MAX_PROCESSING_BLOCK_SIZE);
        ctx->encrypted = 0;
      }
      else if (ctx->ft->current_lsn >= ctx->encrypted_start_2 &&
               ctx->ft->current_lsn <= ctx->encrypted_end_2)
      {
        ctx->block_size = std::min(ctx->encrypted_end_2 + 1 - ctx->ft->current_lsn,
                                   MAX_PROCESSING_BLOCK_SIZE);
        ctx->encrypted = 1;
      }
      else
      {
        ctx->block_size = MAX_PROCESSING_BLOCK_SIZE;
        ctx->encrypted = 0;
      }
      ctx->block_size = std::min(ctx->end_lsn - ctx->ft->current_lsn, ctx->block_size);

      // read some blocks
      ctx->block_size = (uint32_t) sacd_read_block_raw(static_cast<sacd_reader_t*>(ctx->ft->sb_handle->sacd),
                                                       ctx->ft->current_lsn,
                                                       ctx->block_size,
                                                       ctx->output->read_buffer);
      if (ctx->block_size == 0)
        return -1;

      ctx->ft->current_lsn += ctx->block_size;
      ctx->output->stats_total_sectors_processed += ctx->block_size;
      ctx->output->stats_current_file_sectors_processed += ctx->block_size;

      // the ATAPI call which returns the flag if the disc is encrypted or not is unknown at this point.
      // user reports tell me that the only non-encrypted discs out there are DSD 3 14/16 discs.
      // this is a quick hack/fix for these discs.
      if (ctx->encrypted && ctx->checked_for_non_encrypted_disc == 0)
      {
        switch (ctx->handle->area[ctx->ft->area].area_toc->frame_format)
        {
          case FRAME_FORMAT_DSD_3_IN_14:
          case FRAME_FORMAT_DSD_3_IN_16:
            ctx->non_encrypted_disc = *(uint64_t *)(ctx->output->read_buffer + 16) == 0;
            break;
        }

        ctx->checked_for_non_encrypted_disc = 1;
      }

      // encrypted blocks need to be decrypted first
      if (ctx->encrypted && ctx->non_encrypted_disc == 0)
        sacd_decrypt(static_cast<sacd_reader_t*>(ctx->ft->sb_handle->sacd),
                     ctx->output->read_buffer, ctx->block_size);

      scarletbook_process_frames(ctx->ft->sb_handle, ctx->output->read_buffer,
                                 ctx->block_size, ctx->ft->current_lsn == ctx->end_lsn,
                                 frame_read_callback, ctx);
    }
    else
      return 0;
  }

  size_t tocopy = std::min(uiBufSize, (size_t)ctx->decode_buffer.getMaxReadSize());
  ctx->decode_buffer.ReadData(reinterpret_cast<char*>(lpBuf), tocopy);
  ctx->pos += tocopy;
  return tocopy;
}

bool CSACDFile::Close(kodi::addon::VFSFileHandle context)
{
  SACDContext* ctx = static_cast<SACDContext*>(context);
  free(ctx->output->read_buffer);
  free(ctx->output);
  scarletbook_close(ctx->handle);
  sacd_close(ctx->reader);
  delete ctx;
  return true;
}

int64_t CSACDFile::GetLength(kodi::addon::VFSFileHandle context)
{
  SACDContext* ctx = static_cast<SACDContext*>(context);
  dsf_handle_t* handle = static_cast<dsf_handle_t*>(ctx->ft->priv);
  return (ctx->end_lsn - ctx->ft->start_lsn) * SACD_LSN_SIZE + handle->header_size + id3_buffer.size();
}

int64_t CSACDFile::GetPosition(kodi::addon::VFSFileHandle context)
{
  SACDContext* ctx = static_cast<SACDContext*>(context);
  return ctx->pos;
}

int CSACDFile::Stat(const kodi::addon::VFSUrl& url, kodi::vfs::FileStatus& buffer)
{
  return -1;
}

bool CSACDFile::IoControlGetSeekPossible(kodi::addon::VFSFileHandle context)
{
  return false;
}

bool CSACDFile::ContainsFiles(const kodi::addon::VFSUrl& url, std::vector<kodi::vfs::CDirEntry>& items, std::string& rootPath)
{
  sacd_reader_t* reader;
  std::string encoded;
  if (strncmp(url.GetURL().c_str(), "sacd://" ,7) == 0 && !url.GetHostname().empty())
  {
    encoded = URLEncode(url.GetHostname());
    reader = sacd_open(url.GetHostname().c_str());
  }
  else
  {
    encoded = URLEncode(url.GetURL());
    reader = sacd_open(url.GetURL().c_str());
  }
  if (reader)
  {
    scarletbook_handle_t* handle = scarletbook_open(reader, 0);
    if (handle)
    {
      scarletbook_area_t* area = &handle->area[0];
      kodi::vfs::CDirEntry item;
      for (size_t i=0;i<area->area_toc->track_count;++i)
      {
        area_track_text_t* track_text = &area->area_track_text[i];
        item.SetLabel(track_text->track_type_title);
        item.SetTitle(track_text->track_type_title);
        std::stringstream str;
        str << "sacd://" << encoded << '/' << i+1 << ".dsf";
        item.SetPath(str.str());
        items.push_back(item);
      }
      scarletbook_close(handle);
      sacd_close(reader);

      std::stringstream str;
      str << "sacd://" << encoded << '/';
      rootPath = str.str();

      return !items.empty();
    }
  }
  return false;
}


class ATTRIBUTE_HIDDEN CMyAddon : public kodi::addon::CAddonBase
{
public:
  CMyAddon() = default;
  ADDON_STATUS CreateInstance(int instanceType, const std::string& instanceID, KODI_HANDLE instance, const std::string& version, KODI_HANDLE& addonInstance) override
  {
    init_logging();
    addonInstance = new CSACDFile(instance, version);
    return ADDON_STATUS_OK;
  }
  ~CMyAddon() override
  {
    destroy_logging();
  }
};

ADDONCREATOR(CMyAddon);
