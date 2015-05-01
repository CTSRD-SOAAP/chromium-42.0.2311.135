// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_mock_time_task_runner.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"

namespace base {

namespace {

// MockTickClock --------------------------------------------------------------

// TickClock that always returns the then-current mock time ticks of
// |task_runner| as the current time ticks.
class MockTickClock : public TickClock {
 public:
  explicit MockTickClock(
      scoped_refptr<const TestMockTimeTaskRunner> task_runner);

  // TickClock:
  TimeTicks NowTicks() override;

 private:
  scoped_refptr<const TestMockTimeTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockTickClock);
};

MockTickClock::MockTickClock(
    scoped_refptr<const TestMockTimeTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

TimeTicks MockTickClock::NowTicks() {
  return task_runner_->NowTicks();
}

// MockClock ------------------------------------------------------------------

// Clock that always returns the then-current mock time of |task_runner| as the
// current time.
class MockClock : public Clock {
 public:
  explicit MockClock(scoped_refptr<const TestMockTimeTaskRunner> task_runner);

  // Clock:
  Time Now() override;

 private:
  scoped_refptr<const TestMockTimeTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockClock);
};

MockClock::MockClock(scoped_refptr<const TestMockTimeTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

Time MockClock::Now() {
  return task_runner_->Now();
}

}  // namespace

// TestMockTimeTaskRunner -----------------------------------------------------

bool TestMockTimeTaskRunner::TemporalOrder::operator()(
    const TestPendingTask& first_task,
    const TestPendingTask& second_task) const {
  return first_task.GetTimeToRun() > second_task.GetTimeToRun();
}

TestMockTimeTaskRunner::TestMockTimeTaskRunner() : now_(Time::UnixEpoch()) {
}

TestMockTimeTaskRunner::~TestMockTimeTaskRunner() {
}

void TestMockTimeTaskRunner::FastForwardBy(TimeDelta delta) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(delta, TimeDelta());

  const TimeTicks original_now_ticks = now_ticks_;
  ProcessAllTasksNoLaterThan(delta);
  ForwardClocksUntilTickTime(original_now_ticks + delta);
}

void TestMockTimeTaskRunner::RunUntilIdle() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProcessAllTasksNoLaterThan(TimeDelta());
}

void TestMockTimeTaskRunner::FastForwardUntilNoTasksRemain() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProcessAllTasksNoLaterThan(TimeDelta::Max());
}

Time TestMockTimeTaskRunner::Now() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return now_;
}

TimeTicks TestMockTimeTaskRunner::NowTicks() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return now_ticks_;
}

scoped_ptr<Clock> TestMockTimeTaskRunner::GetMockClock() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return make_scoped_ptr(new MockClock(this));
}

scoped_ptr<TickClock> TestMockTimeTaskRunner::GetMockTickClock() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return make_scoped_ptr(new MockTickClock(this));
}

bool TestMockTimeTaskRunner::HasPendingTask() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return !tasks_.empty();
}

size_t TestMockTimeTaskRunner::GetPendingTaskCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return tasks_.size();
}

TimeDelta TestMockTimeTaskRunner::NextPendingTaskDelay() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return tasks_.empty() ? TimeDelta::Max()
                        : tasks_.top().GetTimeToRun() - now_ticks_;
}

bool TestMockTimeTaskRunner::RunsTasksOnCurrentThread() const {
  return thread_checker_.CalledOnValidThread();
}

bool TestMockTimeTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  AutoLock scoped_lock(tasks_lock_);
  tasks_.push(TestPendingTask(from_here, task, now_ticks_, delay,
                              TestPendingTask::NESTABLE));
  return true;
}

bool TestMockTimeTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  NOTREACHED();
  return false;
}

void TestMockTimeTaskRunner::OnBeforeSelectingTask() {
  // Empty default implementation.
}

void TestMockTimeTaskRunner::OnAfterTimePassed() {
  // Empty default implementation.
}

void TestMockTimeTaskRunner::OnAfterTaskRun() {
  // Empty default implementation.
}

void TestMockTimeTaskRunner::ProcessAllTasksNoLaterThan(TimeDelta max_delta) {
  DCHECK_GE(max_delta, TimeDelta());
  const TimeTicks original_now_ticks = now_ticks_;
  while (true) {
    OnBeforeSelectingTask();
    TestPendingTask task_info;
    if (!DequeueNextTask(original_now_ticks, max_delta, &task_info))
      break;
    // If tasks were posted with a negative delay, task_info.GetTimeToRun() will
    // be less than |now_ticks_|. ForwardClocksUntilTickTime() takes care of not
    // moving the clock backwards in this case.
    ForwardClocksUntilTickTime(task_info.GetTimeToRun());
    task_info.task.Run();
    OnAfterTaskRun();
  }
}

void TestMockTimeTaskRunner::ForwardClocksUntilTickTime(TimeTicks later_ticks) {
  if (later_ticks <= now_ticks_)
    return;

  now_ += later_ticks - now_ticks_;
  now_ticks_ = later_ticks;
  OnAfterTimePassed();
}

bool TestMockTimeTaskRunner::DequeueNextTask(const TimeTicks& reference,
                                             const TimeDelta& max_delta,
                                             TestPendingTask* next_task) {
  AutoLock scoped_lock(tasks_lock_);
  if (!tasks_.empty() &&
      (tasks_.top().GetTimeToRun() - reference) <= max_delta) {
    *next_task = tasks_.top();
    tasks_.pop();
    return true;
  }
  return false;
}

}  // namespace base