/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayerStackingNodeIterator_h
#define LayerStackingNodeIterator_h

#include "wtf/Noncopyable.h"

namespace blink {

enum ChildrenIteration {
    NegativeZOrderChildren = 1,
    NormalFlowChildren = 1 << 1,
    PositiveZOrderChildren = 1 << 2,
    AllChildren = NegativeZOrderChildren | NormalFlowChildren | PositiveZOrderChildren
};

class LayerStackingNode;

// This iterator walks the LayerStackingNode lists in the following order:
// NegativeZOrderChildren -> NormalFlowChildren -> PositiveZOrderChildren.
class LayerStackingNodeIterator {
    WTF_MAKE_NONCOPYABLE(LayerStackingNodeIterator);
public:
    LayerStackingNodeIterator(const LayerStackingNode& root, unsigned whichChildren)
        : m_root(root)
        , m_remainingChildren(whichChildren)
        , m_index(0)
    {
    }

    LayerStackingNode* next();

private:
    const LayerStackingNode& m_root;
    unsigned m_remainingChildren;
    unsigned m_index;
};

// This iterator is similar to LayerStackingNodeIterator but it walks the lists in reverse order
// (from the last item to the first one).
class LayerStackingNodeReverseIterator {
    WTF_MAKE_NONCOPYABLE(LayerStackingNodeReverseIterator);
public:
    LayerStackingNodeReverseIterator(const LayerStackingNode& root, unsigned whichChildren)
        : m_root(root)
        , m_remainingChildren(whichChildren)
    {
        setIndexToLastItem();
    }

    LayerStackingNode* next();

private:
    void setIndexToLastItem();

    const LayerStackingNode& m_root;
    unsigned m_remainingChildren;
    int m_index;
};

} // namespace blink

#endif // LayerStackingNodeIterator_h
