/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "../lib/libdsdpcm/DSDPCMConverter.h"

#include <kodi/General.h>

class CSACDSettings
{
public:
  static CSACDSettings& GetInstance()
  {
    static CSACDSettings settings;
    return settings;
  }

  bool Load();

  float GetVolumeAdjust() const { return m_volumeAdjust; }
  float GetLFEAdjust() const { return m_lfeAdjust; }
  int Samplerate() const { return m_samplerate; }
  int GetConverterMode() const { return m_dsd2pcmMode; }
  const std::string& GetConverterFirFile() const { return m_dsd2pcmFirFile; }
  conv_type_e GetConverterType() const;
  bool GetConverterFp64() const;
  int GetSpeakerArea() const { return m_speakerArea; }
  bool GetFullPlayback() const { return m_fullplayback; }

private:
  CSACDSettings() = default;

  float m_volumeAdjust = 0.0f;
  float m_lfeAdjust = 0.0f;
  int m_samplerate = 44100;
  int m_dsd2pcmMode = 0;
  std::string m_dsd2pcmFirFile;
  int m_speakerArea = 0;
  bool m_fullplayback = false;
};
