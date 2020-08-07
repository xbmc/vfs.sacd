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

#include "sacd_wavpack.h"

#include <kodi/Filesystem.h>
#include <kodi/General.h>

static int32_t wavpack_read_bytes(void* id, void* data, int32_t bcount)
{
  return (int32_t) reinterpret_cast<sacd_media_t*>(id)->read(data, bcount);
}

static int64_t wavpack_get_pos(void* id)
{
  return reinterpret_cast<sacd_media_t*>(id)->get_position();
}

static int wavpack_set_pos_abs(void* id, int64_t pos)
{
  return reinterpret_cast<sacd_media_t*>(id)->seek(pos, SEEK_SET) ? 0 : -1;
}

static int wavpack_set_pos_rel(void* id, int64_t delta, int mode)
{
  return reinterpret_cast<sacd_media_t*>(id)->seek(delta, mode) ? 0 : -1;
}

static int wavpack_push_back_byte(void* id, int c)
{
  return reinterpret_cast<sacd_media_t*>(id)->seek(-1, SEEK_CUR) ? c : EOF;
}

static int64_t wavpack_get_length(void* id)
{
  return reinterpret_cast<sacd_media_t*>(id)->get_size();
}

static int wavpack_can_seek(void* id)
{
  return reinterpret_cast<sacd_media_t*>(id)->can_seek() ? 1 : 0;
}

static int32_t wavpack_write_bytes(void* id, void* data, int32_t bcount)
{
  return (int32_t) reinterpret_cast<sacd_media_t*>(id)->write(data, (size_t)bcount);
}

static int wavpack_truncate_here(void* id)
{
  reinterpret_cast<sacd_media_t*>(id)->truncate(
      reinterpret_cast<sacd_media_t*>(id)->get_position());
  return 0;
}

static int wavpack_close_stream(void* id)
{
  return 0;
}

static WavpackStreamReader64 wavpack_reader = {
    wavpack_read_bytes,    wavpack_write_bytes,    wavpack_get_pos,    wavpack_set_pos_abs,
    wavpack_set_pos_rel,   wavpack_push_back_byte, wavpack_get_length, wavpack_can_seek,
    wavpack_truncate_here, wavpack_close_stream};

#define BITSTREAM_SHORTS

static void block_update_checksum(unsigned char* buffer_start)
{
  WavpackHeader* wphdr = (WavpackHeader*)buffer_start;
  unsigned char *dp, meta_id, c1, c2;
  uint32_t bcount, meta_bc;

  if (!(wphdr->flags & HAS_CHECKSUM))
    return;

  bcount = wphdr->ckSize - sizeof(WavpackHeader) + 8;
  dp = (unsigned char*)(wphdr + 1);

  while (bcount >= 2)
  {
    meta_id = *dp++;
    c1 = *dp++;

    meta_bc = c1 << 1;
    bcount -= 2;

    if (meta_id & ID_LARGE)
    {
      if (bcount < 2)
        return;

      c1 = *dp++;
      c2 = *dp++;
      meta_bc += ((uint32_t)c1 << 9) + ((uint32_t)c2 << 17);
      bcount -= 2;
    }

    if (bcount < meta_bc)
      return;

    if ((meta_id & ID_UNIQUE) == ID_BLOCK_CHECKSUM)
    {
#ifdef BITSTREAM_SHORTS
      uint16_t* csptr = (uint16_t*)buffer_start;
#else
      unsigned char* csptr = buffer_start;
#endif
      int wcount = (int)(dp - 2 - buffer_start) >> 1;
      uint32_t csum = (uint32_t)-1;

      if ((meta_id & ID_ODD_SIZE) || meta_bc < 2 || meta_bc > 4)
        return;

#ifdef BITSTREAM_SHORTS
      while (wcount--)
        csum = (csum * 3) + *csptr++;
#else
      WavpackNativeToLittleEndian((WavpackHeader*)buffer_start, WavpackHeaderFormat);

      while (wcount--)
      {
        csum = (csum * 3) + csptr[0] + (csptr[1] << 8);
        csptr += 2;
      }

      WavpackLittleEndianToNative((WavpackHeader*)buffer_start, WavpackHeaderFormat);
#endif

      if (meta_bc == 4)
      {
        *dp++ = csum;
        *dp++ = csum >> 8;
        *dp++ = csum >> 16;
        *dp++ = csum >> 24;
        return;
      }
      else
      {
        csum ^= csum >> 16;
        *dp++ = csum;
        *dp++ = csum >> 8;
        return;
      }
    }

    bcount -= meta_bc;
    dp += meta_bc;
  }
}

bool g_dsd_in_wavpack(const std::string& file)
{
  auto is_dsd = false;
  std::string wavpack_file = kodi::vfs::TranslateSpecialProtocol(file);
  char error[80];
  int flags = OPEN_WVC | OPEN_FILE_UTF8 | OPEN_DSD_NATIVE | OPEN_ALT_TYPES;
  auto wpc = WavpackOpenFileInput(wavpack_file.c_str(), error, flags, 0);
  if (wpc)
  {
    is_dsd = (WavpackGetQualifyMode(wpc) & QMODE_DSD_AUDIO) ? true : false;
    WavpackCloseFile(wpc);
  }
  return is_dsd;
}

sacd_wavpack_t::sacd_wavpack_t()
{
  m_file = nullptr;
  m_mode = ACCESS_MODE_NULL;
  m_wpc = nullptr;
  m_version = 0;
  m_samplerate = 0;
  m_channel_count = 0;
  m_channel_mask = 0;
  m_loudspeaker_config = 0;
  m_framerate = 0;
  m_samples = 0;
  m_duration = 0.0;
  m_track_number = 0;
  m_has_id3 = false;
}

sacd_wavpack_t::~sacd_wavpack_t()
{
  close();
}

uint32_t sacd_wavpack_t::get_track_count(uint32_t mode)
{
  uint32_t track_mode = mode ? mode : m_mode;
  uint32_t track_count = 0;
  if (track_mode & ACCESS_MODE_TWOCH)
  {
    if (m_channel_count <= 2)
    {
      track_count += m_tracklist.size();
    }
  }
  if (track_mode & ACCESS_MODE_MULCH)
  {
    if (m_channel_count > 2)
    {
      track_count += m_tracklist.size();
    }
  }
  if (track_mode & ACCESS_MODE_SINGLE_TRACK)
  {
    track_count = std::min(track_count, 1u);
  }
  return track_count;
}

uint32_t sacd_wavpack_t::get_track_number(uint32_t track_index)
{
  return track_index + 1;
}

int sacd_wavpack_t::get_channels(uint32_t track_number)
{
  return m_channel_count;
}

int sacd_wavpack_t::get_loudspeaker_config(uint32_t track_number)
{
  return m_loudspeaker_config;
}

int sacd_wavpack_t::get_samplerate(uint32_t track_number)
{
  return m_samplerate;
}

int sacd_wavpack_t::get_framerate(uint32_t track_number)
{
  return m_framerate;
}

double sacd_wavpack_t::get_duration(uint32_t track_number)
{
  if (track_number == TRACK_SELECTED)
  {
    track_number = m_track_number;
  }
  double duration = (double)WavpackGetNumSamples64(m_wpc) / (double)WavpackGetSampleRate(m_wpc);
  uint32_t track_index = track_number - 1;
  if (track_index != -1)
  {
    if (track_index < m_tracklist.size())
    {
      duration = m_tracklist[track_index].stop_time - m_tracklist[track_index].start_time;
    }
  }
  return duration;
}

void sacd_wavpack_t::set_mode(uint32_t mode)
{
  m_mode = mode;
}

bool sacd_wavpack_t::open(sacd_media_t* p_file)
{
  m_file = p_file;
  m_tracklist.resize(0);
  m_id3_tagger.remove_all();
  m_has_id3 = false;
  char error[80];
  int flags =
      OPEN_WVC | OPEN_TAGS | OPEN_WRAPPER | OPEN_FILE_UTF8 | OPEN_DSD_NATIVE | OPEN_ALT_TYPES;
  //   if (m_file->get_reason() == input_open_info_write)
  //   {
  //     flags |= OPEN_EDIT_TAGS;
  //   }
  m_wpc = WavpackOpenFileInputEx64(&wavpack_reader, m_file, nullptr, error, flags, 0);
  if (!m_wpc)
  {
    kodi::Log(ADDON_LOG_ERROR, "Error: sacd_wavpack_t::open() => %s", error);
    return false;
  }
  auto qmode = WavpackGetQualifyMode(m_wpc);
  if (!(qmode & QMODE_DSD_AUDIO))
  {
    kodi::Log(ADDON_LOG_ERROR, "unsupported format");
    return false;
  }
  m_version = WavpackGetVersion(m_wpc);
  m_samplerate = WavpackGetNativeSampleRate(m_wpc);
  m_channel_count = WavpackGetNumChannels(m_wpc);
  m_channel_mask = WavpackGetChannelMask(m_wpc);
  if (m_channel_mask == 0x00 || m_channel_mask == 0xff)
  {
    switch (m_channel_count)
    {
      case 1:
        m_loudspeaker_config = 5;
        break;
      case 2:
        m_loudspeaker_config = 0;
        break;
      case 3:
        m_loudspeaker_config = 6;
        break;
      case 4:
        m_loudspeaker_config = 1;
        break;
      case 5:
        m_loudspeaker_config = 3;
        break;
      case 6:
        m_loudspeaker_config = 4;
        break;
      default:
        m_loudspeaker_config = 65535;
        break;
    }
  }
  else
  {
    m_loudspeaker_config = m_channel_mask | 0x10000000;
  }
  m_framerate = 75;
  m_samples = WavpackGetNumSamples64(m_wpc);
  m_duration = (double)m_samples / (double)WavpackGetSampleRate(m_wpc);
  import_metainfo();
  import_cuesheet();
  m_id3_tagger.set_single_track(m_tracklist.size() == 1);
  return true;
}

bool sacd_wavpack_t::close()
{
  m_track_number = 0;
  m_tracklist.resize(0);
  m_id3_tagger.remove_all();
  if (m_wpc)
  {
    WavpackCloseFile(m_wpc);
    m_wpc = nullptr;
  }
  return true;
}

bool sacd_wavpack_t::select_track(uint32_t track_number, uint32_t offset)
{
  m_track_number = track_number;
  auto [start_time, stop_time] = get_track_times(track_number);
  m_track_start = (uint64_t)(m_samples * start_time / m_duration);
  m_track_end = (uint64_t)(m_samples * stop_time / m_duration);
  auto samples_in_frame = m_samplerate / 8 / m_framerate;
  m_track_position = (m_track_start / samples_in_frame) * samples_in_frame;
  return WavpackSeekSample64(m_wpc, m_track_position) == 1;
}

bool sacd_wavpack_t::read_frame(uint8_t* frame_data, size_t* frame_size, frame_type_e* frame_type)
{
  if (m_track_position < m_track_end)
  {
    m_data.resize(*frame_size);
    size_t samples = *frame_size / m_channel_count;
    samples = WavpackUnpackSamples(m_wpc, m_data.data(), samples);
    for (size_t sample = 0; sample < m_data.size(); sample++)
    {
      frame_data[sample] = m_data[sample];
    }
    *frame_type = frame_type_e::DSD;
    *frame_size = samples * m_channel_count;
    m_track_position += samples;
    return samples > 0;
  }
  else
  {
    return false;
  }
}

bool sacd_wavpack_t::seek(double seconds)
{
  auto track_offset =
      std::min((uint64_t)((m_track_end - m_track_start) * seconds / get_duration(m_track_number)),
               m_track_end - m_track_start);
  auto samples_in_frame = m_samplerate / 8 / m_framerate;
  m_track_position = m_track_start + (track_offset / samples_in_frame) * samples_in_frame;
  return WavpackSeekSample64(m_wpc, m_track_position) == 1;
}

void sacd_wavpack_t::get_info(uint32_t track_number, kodi::addon::AudioDecoderInfoTag& info)
{
  kodi::addon::AudioDecoderInfoTag id3_info;
  m_id3_tagger.get_info(track_number, id3_info);
  //   if (m_cuesheet.have_cuesheet())
  //   {
  //     try
  //     {
  //       m_cuesheet.get_track_info(track_number, info);
  //       //info.merge_fallback(id3_info);
  //     }
  //     catch (...)
  //     {
  //     }
  //     return;
  //   }
  if (m_id3_tagger.get_count() <= 1)
  {
    auto pos = m_file->get_position();
    try
    {
      //       tag_processor::read_trailing(m_file->get_handle(), info, abort_callback_impl());
      //info.merge_fallback(id3_info);
    }
    catch (...)
    {
    }
    m_file->seek(pos, SEEK_SET);
    return;
  }
  info = id3_info;
}

void sacd_wavpack_t::set_info(uint32_t track_number, const kodi::addon::AudioDecoderInfoTag& info)
{
  m_id3_tagger.set_info(track_number, info);
  //   if (m_cuesheet.have_cuesheet())
  //   {
  //     try
  //     {
  //       m_cuesheet.set_track_info(track_number, info);
  //     }
  //     catch (...)
  //     {
  //     }
  //   }
}

void sacd_wavpack_t::commit()
{
  /*
	m_wv.attach(m_file);
	m_wv.load_alt_header_and_trailer();
	int64_t old_size = m_wv.get_alt_trailer().size();
	update_tags(m_wv.get_alt_trailer());
	int64_t new_size = m_wv.get_alt_trailer().size();
	update_size(m_wv.get_alt_header(), new_size - old_size);
	m_wv.save_alt_header_and_trailer();
	m_wv.detach();
	*/
  if (m_id3_tagger.is_single_track())
  {
    try
    {
      kodi::addon::AudioDecoderInfoTag info;
      if (m_id3_tagger.get_info(0, info))
      {
        //         tag_processor::write_apev2(m_file->get_handle(), info, abort_callback_impl());
      }
    }
    catch (...)
    {
    }
  }
  //   if (m_cuesheet.have_cuesheet())
  //   {
  //     kodi::addon::AudioDecoderInfoTag info;
  //     m_cuesheet.get_tag(info);
  //     std::string tag_name = "cuesheet";
  //     std::string tag_value = info.meta_get(tag_name, 0);
  //     WavpackAppendTagItem(m_wpc, tag_name.c_str(), tag_value.c_str(), tag_value.length());
  //     WavpackWriteTag(m_wpc);
  //   }
}

std::tuple<double, double> sacd_wavpack_t::get_track_times(uint32_t track_number)
{
  double start_time = 0.0;
  double stop_time = m_duration;
  uint32_t track_index = track_number - 1;
  if (track_index < m_tracklist.size())
  {
    if (m_mode & ACCESS_MODE_FULL_PLAYBACK)
    {
      if (track_index > 0)
      {
        start_time = m_tracklist[track_index].start_time;
      }
      if (track_index + 1 < m_tracklist.size())
      {
        stop_time = m_tracklist[track_index + 1].start_time;
      }
    }
    else
    {
      start_time = m_tracklist[track_index].start_time;
      stop_time = m_tracklist[track_index].stop_time;
    }
  }
  return std::make_tuple(start_time, stop_time);
}

void sacd_wavpack_t::import_metainfo()
{
  Chunk ck;
  ID id;
  WavpackSeekTrailingWrapper(m_wpc);
  auto data = WavpackGetWrapperData(m_wpc);
  auto size = WavpackGetWrapperBytes(m_wpc);
  if (data == nullptr || size < sizeof(ck))
  {
    track_time_t t;
    t.start_time = 0.0;
    t.stop_time = m_duration;
    m_tracklist.push_back(t);
    return;
  }
  auto offset = 0u;
  switch (WavpackGetFileFormat(m_wpc))
  {
    case WP_FORMAT_DFF:
      memcpy(&ck, data + offset, sizeof(ck));
      if (ck != "FRM8")
      {
        break;
      }
      offset += sizeof(ck);
      memcpy(&id, data + offset, sizeof(id));
      if (id != "DSD ")
      {
        break;
      }
      offset += sizeof(id);
      while (offset + sizeof(ck) <= size)
      {
        memcpy(&ck, data + offset, sizeof(ck));
        offset += sizeof(ck);
        if (ck == "DSD ")
        {
          track_time_t t;
          t.start_time = 0.0;
          t.stop_time = m_duration;
          m_tracklist.push_back(t);
        }
        else if (ck == "DIIN")
        {
          auto id_diin_end = data + offset + ck.get_size();
          auto start_mark_count = 0;
          while (data + offset < id_diin_end)
          {
            memcpy(&ck, data + offset, sizeof(ck));
            offset += sizeof(ck);
            if (ck == "MARK")
            {
              if (ck.get_size() >= sizeof(Marker))
              {
                Marker m;
                memcpy(&m, data + offset, sizeof(m));
                m.hours = hton16(m.hours);
                m.samples = hton32(m.samples);
                m.offset = hton32(m.offset);
                m.markType = hton16(m.markType);
                m.markChannel = hton16(m.markChannel);
                m.TrackFlags = hton16(m.TrackFlags);
                m.count = hton32(m.count);
                switch (m.markType)
                {
                  case TrackStart:
                    if (start_mark_count > 0)
                    {
                      track_time_t t;
                      m_tracklist.push_back(t);
                    }
                    start_mark_count++;
                    if (m_tracklist.size() > 0)
                    {
                      m_tracklist[m_tracklist.size() - 1].start_time =
                          get_marker_time(m, m_samplerate);
                      m_tracklist[m_tracklist.size() - 1].stop_time = m_duration;
                      if (m_tracklist.size() - 1 > 0)
                      {
                        if (m_tracklist[m_tracklist.size() - 2].stop_time >
                            m_tracklist[m_tracklist.size() - 1].start_time)
                        {
                          m_tracklist[m_tracklist.size() - 2].stop_time =
                              m_tracklist[m_tracklist.size() - 1].start_time;
                        }
                      }
                    }
                    break;
                  case TrackStop:
                    if (m_tracklist.size() > 0)
                    {
                      m_tracklist[m_tracklist.size() - 1].stop_time =
                          get_marker_time(m, m_samplerate);
                    }
                    break;
                }
              }
            }
            offset += (int)ck.get_size();
            offset += offset & 1;
          }
        }
        else if (ck == "ID3 ")
        {
          if (ck.get_size())
          {
            id3_tags_t id3_tags;
            id3_tags.value.resize((size_t)ck.get_size());
            memcpy(id3_tags.value.data(), data + offset, (size_t)ck.get_size());
            m_id3_tagger.append(id3_tags);
            offset += (int)ck.get_size();
            m_has_id3 = true;
          }
        }
        else
        {
          offset += (int)ck.get_size();
        }
        offset += offset & 1;
      }
      break;
    case WP_FORMAT_DSF:
      memcpy(&ck, data + offset, sizeof(ck));
      if (ck != "DSD ")
      {
        break;
      }
      offset += sizeof(ck);
      offset += 8;
      offset += 8;
      while (offset + sizeof(ck) <= size)
      {
        memcpy(&ck, data + offset, sizeof(ck));
        if (ck == "data")
        {
          offset += sizeof(ck);
          track_time_t t;
          t.start_time = 0.0;
          t.stop_time = m_duration;
          m_tracklist.push_back(t);
          auto id3_size = size - offset;
          if (id3_size)
          {
            id3_tags_t id3_tags;
            id3_tags.value.resize(size - offset);
            memcpy(id3_tags.value.data(), data + offset, size - offset);
            m_id3_tagger.append(id3_tags);
            m_has_id3 = true;
            offset = size;
          }
        }
        else
        {
          offset += (int)ck.ckDataSize;
        }
      }
      break;
  }
  WavpackFreeWrapper(m_wpc);
}

void sacd_wavpack_t::import_cuesheet()
{
  kodi::addon::AudioDecoderInfoTag info;
  info.SetDuration(m_duration);
  auto tag_name = "cuesheet";
  auto tag_size = WavpackGetTagItem(m_wpc, tag_name, nullptr, 0);
  if (tag_size > 0)
  {
    std::vector<char> tag_value;
    tag_size = 2 * (tag_size + 1);
    tag_value.resize(tag_size);
    WavpackGetTagItem(m_wpc, tag_name, tag_value.data(), tag_size);
    //     info.meta_set(tag_name, tag_value.data());
  }
  //   m_cuesheet.set_tag(info);
  //   if (m_cuesheet.have_cuesheet())
  //   {
  //     if (m_cuesheet.get_cue_track_count() > 0)
  //     {
  //       m_tracklist.resize(0);
  //       for (size_t track_index = 0; track_index < m_cuesheet.get_cue_track_count(); track_index++)
  //       {
  //         track_time_t s;
  //         auto track = m_cuesheet.remap_trackno(track_index);
  //         double begin, length;
  //         m_cuesheet.query_track_offsets(track, begin, length);
  //         s.start_time = begin;
  //         s.stop_time = begin + length;
  //         m_tracklist.push_back(s);
  //       }
  //     }
  //   }
}

void sacd_wavpack_t::update_tags(std::vector<uint8_t>& metadata)
{
  Chunk ck;
  ID id;
  auto offset = 0;
  auto id3_start = -1;
  auto id3_end = -1;
  switch (WavpackGetFileFormat(m_wpc))
  {
    case WP_FORMAT_DFF:
      memcpy(&ck, &metadata[offset], sizeof(ck));
      if (ck == "FRM8")
      {
        offset += sizeof(ck);
      }
      memcpy(&id, &metadata[offset], sizeof(id));
      if (id == "DSD ")
      {
        offset += sizeof(id);
      }
      while (offset + sizeof(ck) < metadata.size())
      {
        memcpy(&ck, &metadata[offset], sizeof(ck));
        offset += sizeof(ck);
        if (ck == "DSD ")
        {
        }
        else if (ck == "ID3 ")
        {
          if (id3_start == -1)
          {
            id3_start = offset - sizeof(ck);
          }
          offset += (int)ck.get_size();
          offset += offset & 1;
          id3_end = std::max(id3_end, offset);
        }
        else
        {
          offset += (int)ck.get_size();
          offset += offset & 1;
        }
      }
      if (id3_start == -1)
      {
        id3_end = id3_start = metadata.size();
      }
      else
      {
        metadata.erase(metadata.begin() + id3_start, metadata.begin() + id3_end);
        id3_end = id3_start;
      }
      for (auto& tag : m_id3_tagger.get_info())
      {
        if (tag.value.size() > 0)
        {
          ck.set_id("ID3 ");
          ck.set_size(tag.value.size());
          metadata.insert(metadata.begin() + id3_end, sizeof(ck), 0);
          memcpy(&metadata[id3_end], &ck, sizeof(ck));
          id3_end += sizeof(ck);
          metadata.insert(metadata.begin() + id3_end, tag.value.begin(), tag.value.end());
          id3_end += tag.value.size();
          if (tag.value.size() & 1)
          {
            metadata.insert(metadata.begin() + id3_end, 0);
            id3_end += 1;
          }
        }
      }
      break;
    case WP_FORMAT_DSF:
      if (m_tracklist.size() == 1)
      {
        metadata.erase(metadata.begin() + offset, metadata.end());
        for (auto& tag : m_id3_tagger.get_info())
        {
          if (tag.value.size() > 0)
          {
            metadata.insert(metadata.end(), tag.value.begin(), tag.value.end());
          }
        }
      }
      break;
  }
}

void sacd_wavpack_t::update_size(std::vector<uint8_t>& metadata, int64_t delta)
{
  Chunk ck;
  size_t offset = 0;
  switch (WavpackGetFileFormat(m_wpc))
  {
    case WP_FORMAT_DFF:
      memcpy(&ck, &metadata[offset], sizeof(ck));
      if (ck == "FRM8")
      {
        ck.set_size(ck.get_size() + delta);
        memcpy(&metadata[offset], &ck, sizeof(ck));
      }
      break;
    case WP_FORMAT_DSF:
      memcpy(&ck, &metadata[offset], sizeof(ck));
      offset += sizeof(ck);
      if (ck == "DSD ")
      {
        uint64_t old_size;
        uint64_t old_meta;
        uint64_t new_size;
        uint64_t new_meta;
        memcpy(&old_size, &metadata[offset], 8);
        memcpy(&old_meta, &metadata[offset + 8], 8);
        new_size = old_size + delta;
        if (old_meta == 0)
        {
          if (new_size > old_size)
          {
            new_meta = old_size;
          }
          else
          {
            new_meta = 0;
          }
        }
        else
        {
          if (new_size > old_meta)
          {
            new_meta = old_meta;
          }
          else
          {
            new_meta = 0;
          }
        }
        memcpy(&metadata[offset], &new_size, 8);
        memcpy(&metadata[offset + 8], &new_meta, 8);
      }
      break;
  }
}

file_wavpack_t::file_wavpack_t()
{
  m_file = nullptr;
}

file_wavpack_t::~file_wavpack_t()
{
  detach();
}

void file_wavpack_t::attach(sacd_media_t* p_file)
{
  m_file = p_file;
}

void file_wavpack_t::detach()
{
  m_file = nullptr;
}

std::vector<uint8_t>& file_wavpack_t::get_alt_header()
{
  return m_alt_header_value;
}

std::vector<uint8_t>& file_wavpack_t::get_alt_trailer()
{
  return m_alt_trailer_value;
}

void file_wavpack_t::load_alt_header_and_trailer()
{
  bool ok = true;
  int64_t current_pos = wavpack_reader.get_pos(m_file);
  WavpackHeader wph;
  int blocks_found;
  m_alt_header_value.resize(0);
  m_alt_header_offset = -1;
  blocks_found = 0;
  for (int64_t file_pos = 0; file_pos < wavpack_reader.get_length(m_file); file_pos++)
  {
    wavpack_reader.set_pos_abs(m_file, file_pos);
    if (!check_wavpack_header(wph))
    {
      continue;
    }
    blocks_found++;
    wavpack_reader.set_pos_abs(m_file, file_pos);
    if (select_metadata(ID_ALT_HEADER, m_alt_header_value))
    {
      m_alt_header_offset = file_pos;
      break;
    }
    else
    {
      if (blocks_found > 0)
      {
        break;
      }
    }
  }
  m_alt_trailer_value.resize(0);
  m_alt_trailer_offset = -1;
  blocks_found = 0;
  for (int64_t i = 0; i < wavpack_reader.get_length(m_file); i++)
  {
    auto file_pos = wavpack_reader.get_length(m_file) - 1 - i;
    wavpack_reader.set_pos_abs(m_file, file_pos);
    if (!check_wavpack_header(wph))
    {
      continue;
    }
    blocks_found++;
    wavpack_reader.set_pos_abs(m_file, file_pos);
    if (select_metadata(ID_ALT_TRAILER, m_alt_trailer_value))
    {
      m_alt_trailer_offset = file_pos;
      break;
    }
    else
    {
      if (blocks_found > 0)
      {
        break;
      }
    }
  }
  wavpack_reader.set_pos_abs(m_file, current_pos);
}

void file_wavpack_t::save_alt_header_and_trailer()
{
  if (m_alt_header_offset != -1)
  {
    wavpack_reader.set_pos_abs(m_file, m_alt_header_offset);
    update_metadata(ID_ALT_HEADER, m_alt_header_value);
  }
  if (m_alt_trailer_offset == -1 && !m_alt_trailer_value.empty())
  {
    if (create_alt_trailer_block())
    {
      m_alt_trailer_offset = wavpack_reader.get_pos(m_file);
    }
  }
  if (m_alt_trailer_offset != -1)
  {
    wavpack_reader.set_pos_abs(m_file, m_alt_trailer_offset);
    update_metadata(ID_ALT_TRAILER, m_alt_trailer_value);
  }
}

bool file_wavpack_t::check_wavpack_header(WavpackHeader& wph)
{
  if (wavpack_reader.read_bytes(m_file, &wph, sizeof(wph)) != sizeof(wph))
  {
    return false;
  }
  return memcmp(wph.ckID, "wvpk", sizeof(wph.ckID)) == 0 && wph.version >= 0X402 &&
         wph.version <= 0X410;
}

bool file_wavpack_t::select_metadata(int metadata_id, std::vector<uint8_t>& metadata_value)
{
  bool metadata_selected = false;
  WavpackHeader wph;
  if (!check_wavpack_header(wph))
  {
    return metadata_selected;
  }
  int32_t metadata_size = wph.ckSize + 8 - sizeof(WavpackHeader);
  while (metadata_size >= 2)
  {
    uint8_t id = 0;
    int32_t ws = 0;
    uint8_t id_type;
    int32_t id_read;
    int32_t id_size;
    if (wavpack_reader.read_bytes(m_file, &id, 1) != sizeof(id))
    {
      return metadata_selected;
    }
    metadata_size -= sizeof(id);
    auto ws_size = (id & ID_LARGE) ? 3 : 1;
    if (wavpack_reader.read_bytes(m_file, &ws, ws_size) != ws_size)
    {
      return metadata_selected;
    }
    metadata_size -= ws_size;
    id_type = id & ID_UNIQUE;
    id_read = ws << 1;
    id_size = id_read - ((id & ID_ODD_SIZE) ? 1 : 0);
    if (id_type == metadata_id)
    {
      if (id_size > 0)
      {
        auto old_size = metadata_value.size();
        metadata_value.resize(old_size + id_size);
        if (wavpack_reader.read_bytes(m_file, &metadata_value[old_size], id_size) != id_size)
        {
          return metadata_selected;
        }
      }
      if (id & ID_ODD_SIZE)
      {
        wavpack_reader.set_pos_rel(m_file, 1, SEEK_CUR);
      }
      metadata_size -= id_read;
      metadata_selected = true;
    }
    else
    {
      wavpack_reader.set_pos_rel(m_file, id_read, SEEK_CUR);
      metadata_size -= id_read;
    }
  }
  return metadata_selected;
}

bool file_wavpack_t::update_metadata(int metadata_id, const std::vector<uint8_t>& metadata_value)
{
  auto block_offset = wavpack_reader.get_pos(m_file);
  bool metadata_updated = false;
  WavpackHeader wph;
  if (!check_wavpack_header(wph))
  {
    return metadata_updated;
  }
  std::vector<uint8_t> uwp(sizeof(WavpackHeader));
  int32_t metadata_size = wph.ckSize + 8 - sizeof(WavpackHeader);
  uint8_t id, id_type, uid;
  uint32_t ws, uws;
  int32_t id_read, id_size;
  while (metadata_size >= 2)
  {
    id = 0;
    metadata_size -= wavpack_reader.read_bytes(m_file, &id, 1);
    ws = 0;
    auto ws_size = (id & ID_LARGE) ? 3 : 1;
    metadata_size -= wavpack_reader.read_bytes(m_file, &ws, ws_size);
    id_type = id & ID_UNIQUE;
    id_read = ws << 1;
    id_size = id_read - ((id & ID_ODD_SIZE) ? 1 : 0);
    if (id_type == metadata_id)
    {
      wavpack_reader.set_pos_rel(m_file, id_read, SEEK_CUR);
      metadata_size -= id_read;
      uid = id & ~(ID_ODD_SIZE | ID_LARGE);
      uws = (metadata_value.size() + 1) / 2;
      if (metadata_value.size() & 1)
      {
        uid |= ID_ODD_SIZE;
      }
      if (uws > 255)
      {
        uid |= ID_LARGE;
      }
      uwp.push_back(uid);
      uwp.push_back(uws);
      if (uws > 255)
      {
        uwp.push_back(uws >> 8);
        uwp.push_back(uws >> 16);
      }
      uwp.insert(uwp.end(), metadata_value.begin(), metadata_value.end());
      if (uid & ID_ODD_SIZE)
      {
        uwp.push_back(0);
      }
      metadata_updated = true;
    }
    else
    {
      uwp.push_back(id);
      uwp.push_back(ws);
      if (id & ID_LARGE)
      {
        uwp.push_back(ws >> 8);
        uwp.push_back(ws >> 16);
      }
      for (auto i = 0; i < id_read; i++)
      {
        uint8_t b;
        metadata_size -= wavpack_reader.read_bytes(m_file, &b, 1);
        uwp.push_back(b);
      }
    }
  }
  wavpack_reader.set_pos_rel(m_file, metadata_size, SEEK_CUR);
  WavpackHeader* uwph = reinterpret_cast<WavpackHeader*>(uwp.data());
  *uwph = wph;
  uwph->ckSize = uwp.size() - 8;
  block_update_checksum(uwp.data());
  if (metadata_updated)
  {
    wavpack_reader.set_pos_abs(m_file, block_offset);
    if (wph.ckSize == uwph->ckSize)
    {
      wavpack_reader.write_bytes(m_file, uwp.data(), uwp.size());
    }
    else
    {
      std::vector<uint8_t> rest_data;
      wavpack_reader.set_pos_rel(m_file, wph.ckSize + 8, SEEK_CUR);
      auto rest_length = wavpack_reader.get_length(m_file) - wavpack_reader.get_pos(m_file);
      rest_data.resize((size_t)rest_length);
      wavpack_reader.read_bytes(m_file, rest_data.data(), rest_data.size());
      wavpack_reader.set_pos_abs(m_file, block_offset);
      wavpack_reader.truncate_here(m_file);
      wavpack_reader.write_bytes(m_file, uwp.data(), uwp.size());
      wavpack_reader.write_bytes(m_file, rest_data.data(), rest_data.size());
    }
  }
  return metadata_updated;
}

bool file_wavpack_t::create_alt_trailer_block()
{
  bool ok = false;
  int64_t file_pos;
  WavpackHeader wph;
  for (int64_t i = 0; i < wavpack_reader.get_length(m_file); i++)
  {
    file_pos = wavpack_reader.get_length(m_file) - 1 - i;
    wavpack_reader.set_pos_abs(m_file, file_pos);
    if (check_wavpack_header(wph))
    {
      ok = true;
      break;
    }
  }
  if (!ok)
  {
    return ok;
  }
  file_pos += wph.ckSize + 8;
  wavpack_reader.set_pos_abs(m_file, file_pos);
  std::vector<uint8_t> uwp(sizeof(WavpackHeader));
  uint8_t uid;
  uint32_t uws;
  uid = ID_ALT_TRAILER;
  uws = 0;
  uwp.push_back(uid);
  uwp.push_back(uws);
  uid = ID_BLOCK_CHECKSUM;
  uws = 2;
  uwp.push_back(uid);
  uwp.push_back(uws);
  uwp.push_back(0);
  uwp.push_back(0);
  uwp.push_back(0);
  uwp.push_back(0);
  WavpackHeader* uwph = reinterpret_cast<WavpackHeader*>(uwp.data());
  *uwph = wph;
  uint64_t block_index = ((uint64_t)wph.block_index_u8 << 32) + (uint64_t)wph.block_index + 1;
  uwph->block_index_u8 = (uint8_t)(block_index >> 32);
  uwph->block_index = (uint32_t)block_index;
  uwph->block_samples = 0;
  uwph->flags = HAS_CHECKSUM;
  uwph->ckSize = uwp.size() - 8;
  block_update_checksum(uwp.data());
  std::vector<uint8_t> rest_data;
  auto rest_length = wavpack_reader.get_length(m_file) - file_pos;
  rest_data.resize((size_t)rest_length);
  wavpack_reader.read_bytes(m_file, rest_data.data(), rest_data.size());
  wavpack_reader.set_pos_abs(m_file, file_pos);
  wavpack_reader.truncate_here(m_file);
  wavpack_reader.write_bytes(m_file, uwp.data(), uwp.size());
  wavpack_reader.write_bytes(m_file, rest_data.data(), rest_data.size());
  wavpack_reader.set_pos_abs(m_file, file_pos);
  return ok;
}
