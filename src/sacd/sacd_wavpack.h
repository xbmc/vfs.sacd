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

#pragma once

#include "endianess.h"
#include "id3_tagger.h"
#include "sacd_dsd.h"
#include "sacd_reader.h"
#include "scarletbook.h"

#include <wavpack.h>

// #include <../helpers/cue_parser.h>

// using cue_parser::embeddedcue_metadata_manager;

bool g_dsd_in_wavpack(const char* file);

class ATTRIBUTE_HIDDEN file_wavpack_t
{
  sacd_media_t* m_file;
  int64_t m_alt_header_offset;
  std::vector<uint8_t> m_alt_header_value;
  int64_t m_alt_trailer_offset;
  std::vector<uint8_t> m_alt_trailer_value;

public:
  file_wavpack_t();
  ~file_wavpack_t();
  void attach(sacd_media_t* p_file);
  void detach();
  std::vector<uint8_t>& get_alt_header();
  std::vector<uint8_t>& get_alt_trailer();
  void load_alt_header_and_trailer();
  void save_alt_header_and_trailer();

private:
  bool check_wavpack_header(WavpackHeader& wph);
  bool select_metadata(int metadata_id, std::vector<uint8_t>& metadata_value);
  bool update_metadata(int metadata_id, const std::vector<uint8_t>& metadata_value);
  bool create_alt_trailer_block();
};

class ATTRIBUTE_HIDDEN sacd_wavpack_t : public sacd_reader_t
{
  sacd_media_t* m_file;
  uint32_t m_mode;
  WavpackContext* m_wpc;
  int m_version;
  uint32_t m_samplerate;
  int m_channel_count;
  int m_channel_mask;
  int m_loudspeaker_config;
  int m_framerate;
  uint64_t m_samples;
  double m_duration;
  std::vector<int32_t> m_data;
  uint32_t m_track_number;
  uint64_t m_track_start;
  uint64_t m_track_end;
  uint64_t m_track_position;
  tracklist_t m_tracklist;
  id3_tagger_t m_id3_tagger;
  bool m_has_id3;
  file_wavpack_t m_wv;
  //   embeddedcue_metadata_manager m_cuesheet;

public:
  sacd_wavpack_t();
  ~sacd_wavpack_t();
  uint32_t get_track_count(uint32_t mode);
  uint32_t get_track_number(uint32_t track_index);
  int get_channels(uint32_t track_number);
  int get_loudspeaker_config(uint32_t track_number);
  int get_samplerate(uint32_t track_number);
  int get_framerate(uint32_t track_number);
  double get_duration(uint32_t track_number);
  void set_mode(uint32_t mode);
  bool open(sacd_media_t* p_file);
  bool close();
  bool select_track(uint32_t track_number, uint32_t offset);
  bool read_frame(uint8_t* frame_data, size_t* frame_size, frame_type_e* frame_type);
  bool seek(double seconds);
  void get_info(uint32_t track_number, kodi::addon::AudioDecoderInfoTag& info) override;
  void set_info(uint32_t track_number, const kodi::addon::AudioDecoderInfoTag& info) override;
  void commit();

private:
  std::tuple<double, double> get_track_times(uint32_t track_number);
  void import_metainfo();
  void import_cuesheet();
  void update_tags(std::vector<uint8_t>& metadata);
  void update_size(std::vector<uint8_t>& metadata, int64_t delta);
};
