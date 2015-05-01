// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py. DO NOT MODIFY!

#ifndef TestPermissiveDictionary_h
#define TestPermissiveDictionary_h

#include "bindings/core/v8/Nullable.h"
#include "platform/heap/Handle.h"

namespace blink {

class TestPermissiveDictionary {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    TestPermissiveDictionary();

    bool hasBooleanMember() const { return !m_booleanMember.isNull(); }
    bool booleanMember() const { return m_booleanMember.get(); }
    void setBooleanMember(bool value) { m_booleanMember = value; }

    virtual void trace(Visitor*);

private:
    Nullable<bool> m_booleanMember;

    friend class V8TestPermissiveDictionary;
};

} // namespace blink

#endif // TestPermissiveDictionary_h