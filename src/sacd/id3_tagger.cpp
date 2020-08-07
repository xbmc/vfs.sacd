/*
* SACD Decoder plugin
* Copyright (c) 2011-2019 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "id3_tagger.h"

bool id3_tagger_t::load_info(const std::vector<uint8_t>& buffer,
                             kodi::addon::AudioDecoderInfoTag& info)
{
  auto ok = false;
  //   try
  //   {
  //     service_ptr_t<file> f;
  //     abort_callback_impl abort;
  //     filesystem::g_open_tempmem(f, abort);
  //     f->write(buffer.data(), buffer.size(), abort);
  //     tag_processor::read_id3v2(f, info, abort);
  //     ok = true;
  //   }
  //   catch (exception_io_data)
  //   {
  //   }
  return ok;
}

bool id3_tagger_t::save_info(std::vector<uint8_t>& buffer,
                             const kodi::addon::AudioDecoderInfoTag& info)
{
  auto ok = false;
  //   try
  //   {
  //     service_ptr_t<file> f;
  //     abort_callback_impl abort;
  //     filesystem::g_open_tempmem(f, abort);
  //     f->write(buffer.data(), buffer.size(), abort);
  //     tag_processor::write_id3v2(f, info, abort);
  //     buffer.resize((uint32_t)f->get_size(abort));
  //     f->seek(0, abort);
  //     f->read(buffer.data(), buffer.size(), abort);
  //     ok = true;
  //   }
  //   catch (exception_io_data)
  //   {
  //   }
  return ok;
}

id3_tagger_t::iterator id3_tagger_t::begin()
{
  return iterator(tagstore, 0);
}

id3_tagger_t::iterator id3_tagger_t::end()
{
  return iterator(tagstore, tagstore.size());
}

size_t id3_tagger_t::get_count()
{
  return tagstore.size();
}

std::vector<id3_tags_t>& id3_tagger_t::get_info()
{
  return tagstore;
}

bool id3_tagger_t::is_single_track()
{
  return single_track;
}

void id3_tagger_t::set_single_track(bool is_single)
{
  single_track = is_single;
}

void id3_tagger_t::append(const id3_tags_t& tags)
{
  tagstore.push_back(tags);
  if (tags.id == -1)
  {
    update_tags(tagstore.size() - 1);
  }
}

void id3_tagger_t::remove_all()
{
  tagstore.clear();
}

bool id3_tagger_t::get_info(size_t track_number, kodi::addon::AudioDecoderInfoTag& info)
{
  auto ok = false;
  for (size_t track_index = 0; track_index < tagstore.size(); track_index++)
  {
    if (track_number == tagstore[track_index].id || single_track)
    {
      ok = load_info(track_index, info);
      break;
    }
  }
  return ok;
}

bool id3_tagger_t::set_info(size_t track_number, const kodi::addon::AudioDecoderInfoTag& info)
{
  auto ok = false;
  auto track_found = false;
  for (size_t track_index = 0; track_index < tagstore.size(); track_index++)
  {
    if (track_number == tagstore[track_index].id || single_track)
    {
      ok = save_info(track_index, info);
      track_found = true;
      break;
    }
  }
  if (!track_found && !info.GetTitle().empty())
  {
    id3_tags_t t;
    t.id = track_number;
    append(t);
    ok = save_info(tagstore.size() - 1, info);
  }
  return ok;
}

void id3_tagger_t::update_tags()
{
  for (size_t track_index = 0; track_index < tagstore.size(); track_index++)
  {
    update_tags(track_index);
  }
}

bool id3_tagger_t::load_info(size_t track_index, kodi::addon::AudioDecoderInfoTag& info)
{
  auto ok = false;
  if (track_index < tagstore.size())
  {
    ok = load_info(tagstore[track_index].value, info);
  }
  return ok;
}

bool id3_tagger_t::save_info(size_t track_index, const kodi::addon::AudioDecoderInfoTag& info)
{
  auto ok = false;
  if (track_index < tagstore.size())
  {
    ok = save_info(tagstore[track_index].value, info);
  }
  return ok;
}

void id3_tagger_t::update_tags(size_t track_index)
{
  if (track_index < tagstore.size())
  {
    kodi::addon::AudioDecoderInfoTag info;
    if (load_info(track_index, info))
    {
      tagstore[track_index].id = info.GetTrack();
    }
  }
}

void id3_tagger_t::get_albumart(size_t albumart_id, std::vector<uint8_t>& albumart_data)
{
  if (tagstore.size() > 0)
  {
    albumart_data = tagstore[0].value;
    kodi::addon::AudioDecoderInfoTag info;
    save_info(albumart_data, info);
  }
}

void id3_tagger_t::set_albumart(size_t albumart_id, const std::vector<uint8_t>& albumart_data)
{
  if (tagstore.size() > 0)
  {
    kodi::addon::AudioDecoderInfoTag info;
    load_info(tagstore[0].value, info);
    tagstore[0].value = albumart_data;
    save_info(tagstore[0].value, info);
  }
}
