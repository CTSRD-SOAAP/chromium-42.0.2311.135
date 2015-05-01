// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_H_

namespace chromeos {

class NetworkError {
 public:
  enum UIState {
    UI_STATE_UNKNOWN = 0,
    UI_STATE_UPDATE,
    UI_STATE_SIGNIN,
    UI_STATE_SUPERVISED,
    UI_STATE_KIOSK_MODE,
    UI_STATE_LOCAL_STATE_ERROR,
    UI_STATE_AUTO_ENROLLMENT_ERROR,
    UI_STATE_ROLLBACK_ERROR,
  };

  enum ErrorState {
    ERROR_STATE_UNKNOWN = 0,
    ERROR_STATE_PORTAL,
    ERROR_STATE_OFFLINE,
    ERROR_STATE_PROXY,
    ERROR_STATE_AUTH_EXT_TIMEOUT,
    ERROR_STATE_NONE,
    // States above are being logged to histograms.
    // Please keep ERROR_STATE_NONE as the last one of the histogram values.
    // ERROR_STATE_KIOSK_ONLINE is a special case (not an actual error) and is
    // not reported in histogram.
    ERROR_STATE_KIOSK_ONLINE,
  };

  // Possible network error reasons.
  enum ErrorReason {
    ERROR_REASON_PROXY_AUTH_CANCELLED = 0,
    ERROR_REASON_PROXY_AUTH_SUPPLIED,
    ERROR_REASON_PROXY_CONNECTION_FAILED,
    ERROR_REASON_PROXY_CONFIG_CHANGED,
    ERROR_REASON_LOADING_TIMEOUT,
    ERROR_REASON_PORTAL_DETECTED,
    // Reason for a case when default network has changed.
    ERROR_REASON_NETWORK_STATE_CHANGED,
    // Reason for a case when JS side requires error screen update.
    ERROR_REASON_UPDATE,
    ERROR_REASON_FRAME_ERROR
  };

  static const char* ErrorReasonString(ErrorReason reason);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_H_