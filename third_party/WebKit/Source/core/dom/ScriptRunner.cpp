/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/ScriptRunner.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ScriptLoader.h"
#include "platform/heap/Handle.h"

namespace blink {

ScriptRunner::ScriptRunner(Document* document)
    : m_document(document)
    , m_timer(this, &ScriptRunner::timerFired)
{
    ASSERT(document);
}

ScriptRunner::~ScriptRunner()
{
#if !ENABLE(OILPAN)
    // Make sure that ScriptLoaders don't keep their PendingScripts alive.
    for (ScriptLoader* scriptLoader : m_scriptsToExecuteInOrder)
        scriptLoader->detach();
    for (ScriptLoader* scriptLoader : m_scriptsToExecuteSoon)
        scriptLoader->detach();
    for (ScriptLoader* scriptLoader : m_pendingAsyncScripts)
        scriptLoader->detach();
#endif
}

void ScriptRunner::addPendingAsyncScript(ScriptLoader* scriptLoader)
{
    m_document->incrementLoadEventDelayCount();
    m_pendingAsyncScripts.add(scriptLoader);
}

void ScriptRunner::queueScriptForExecution(ScriptLoader* scriptLoader, ExecutionType executionType)
{
    ASSERT(scriptLoader);

    switch (executionType) {
    case ASYNC_EXECUTION:
        addPendingAsyncScript(scriptLoader);
        break;

    case IN_ORDER_EXECUTION:
        m_document->incrementLoadEventDelayCount();
        m_scriptsToExecuteInOrder.append(scriptLoader);
        break;
    }
}

void ScriptRunner::suspend()
{
    m_timer.stop();
}

void ScriptRunner::resume()
{
    if (hasPendingScripts())
        m_timer.startOneShot(0, FROM_HERE);
}

void ScriptRunner::notifyScriptReady(ScriptLoader* scriptLoader, ExecutionType executionType)
{
    switch (executionType) {
    case ASYNC_EXECUTION:
        ASSERT(m_pendingAsyncScripts.contains(scriptLoader));
        m_scriptsToExecuteSoon.append(scriptLoader);
        m_pendingAsyncScripts.remove(scriptLoader);
        break;

    case IN_ORDER_EXECUTION:
        ASSERT(!m_scriptsToExecuteInOrder.isEmpty());
        break;
    }
    m_timer.startOneShot(0, FROM_HERE);
}

void ScriptRunner::notifyScriptLoadError(ScriptLoader* scriptLoader, ExecutionType executionType)
{
    switch (executionType) {
    case ASYNC_EXECUTION:
        ASSERT(m_pendingAsyncScripts.contains(scriptLoader));
        m_pendingAsyncScripts.remove(scriptLoader);
        scriptLoader->detach();
        m_document->decrementLoadEventDelayCount();
        break;

    case IN_ORDER_EXECUTION:
        ASSERT(!m_scriptsToExecuteInOrder.isEmpty());
        break;
    }
}

void ScriptRunner::movePendingAsyncScript(Document& oldDocument, Document& newDocument, ScriptLoader* scriptLoader)
{
    RefPtrWillBeRawPtr<Document> newContextDocument = newDocument.contextDocument().get();
    if (!newContextDocument) {
        // Document's contextDocument() method will return no Document if the
        // following conditions both hold:
        //
        //   - The Document wasn't created with an explicit context document
        //     and that document is otherwise kept alive.
        //   - The Document itself is detached from its frame.
        //
        // The script element's loader is in that case moved to document() and
        // its script runner, which is the non-null Document that contextDocument()
        // would return if not detached.
        ASSERT(!newDocument.frame());
        newContextDocument = &newDocument;
    }
    RefPtrWillBeRawPtr<Document> oldContextDocument = oldDocument.contextDocument().get();
    if (!oldContextDocument) {
        ASSERT(!oldDocument.frame());
        oldContextDocument = &oldDocument;
    }
    if (oldContextDocument != newContextDocument)
        oldContextDocument->scriptRunner()->movePendingAsyncScript(newContextDocument->scriptRunner(), scriptLoader);
}

void ScriptRunner::movePendingAsyncScript(ScriptRunner* newRunner, ScriptLoader* scriptLoader)
{
    if (m_pendingAsyncScripts.contains(scriptLoader)) {
        newRunner->addPendingAsyncScript(scriptLoader);
        m_pendingAsyncScripts.remove(scriptLoader);
        m_document->decrementLoadEventDelayCount();
    }
}

void ScriptRunner::timerFired(Timer<ScriptRunner>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);

    RefPtrWillBeRawPtr<Document> protect(m_document.get());

    WillBeHeapVector<RawPtrWillBeMember<ScriptLoader> > scriptLoaders;
    scriptLoaders.swap(m_scriptsToExecuteSoon);

    size_t numInOrderScriptsToExecute = 0;
    for (; numInOrderScriptsToExecute < m_scriptsToExecuteInOrder.size() && m_scriptsToExecuteInOrder[numInOrderScriptsToExecute]->isReady(); ++numInOrderScriptsToExecute)
        scriptLoaders.append(m_scriptsToExecuteInOrder[numInOrderScriptsToExecute]);
    if (numInOrderScriptsToExecute)
        m_scriptsToExecuteInOrder.remove(0, numInOrderScriptsToExecute);

    size_t size = scriptLoaders.size();
    for (size_t i = 0; i < size; ++i) {
        scriptLoaders[i]->execute();
        m_document->decrementLoadEventDelayCount();
    }
}

void ScriptRunner::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_document);
    visitor->trace(m_scriptsToExecuteInOrder);
    visitor->trace(m_scriptsToExecuteSoon);
    visitor->trace(m_pendingAsyncScripts);
#endif
}

}
