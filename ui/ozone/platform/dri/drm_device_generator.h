// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRM_DEVICE_GENERATOR_H_
#define UI_OZONE_PLATFORM_DRI_DRM_DEVICE_GENERATOR_H_

#include "base/files/file.h"
#include "base/memory/ref_counted.h"

namespace ui {

class DriWrapper;

class DrmDeviceGenerator {
 public:
  DrmDeviceGenerator();
  virtual ~DrmDeviceGenerator();

  // Creates a DRM device for |file|. |device_path| describes the location of
  // the DRM device.
  virtual scoped_refptr<DriWrapper> CreateDevice(
      const base::FilePath& device_path,
      base::File file);

 public:
  DISALLOW_COPY_AND_ASSIGN(DrmDeviceGenerator);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRM_DEVICE_GENERATOR_H_