/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Addon.h"

#include "SACDAudio.h"
#include "SACDFile.h"
#include "Settings.h"

CMyAddon::CMyAddon()
{
  CSACDSettings::GetInstance().Load();
}

CMyAddon::~CMyAddon()
{
}

ADDON_STATUS CMyAddon::CreateInstance(int instanceType,
                                      const std::string& instanceID,
                                      KODI_HANDLE instance,
                                      const std::string& version,
                                      KODI_HANDLE& addonInstance)
{
  if (instanceType == ADDON_INSTANCE_VFS)
    addonInstance = new CSACDFile(instance, version);
  else if (instanceType == ADDON_INSTANCE_AUDIODECODER)
    addonInstance = new CSACDAudioDecoder(instance, version);
  return ADDON_STATUS_OK;
}

ADDON_STATUS CMyAddon::SetSetting(const std::string& settingName,
                                  const kodi::CSettingValue& settingValue)
{
  CSACDSettings::GetInstance().Load();
  return ADDON_STATUS_OK;
}

ADDONCREATOR(CMyAddon)
