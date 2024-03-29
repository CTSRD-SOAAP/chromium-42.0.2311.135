// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of classes in the tracked_objects.h classes.

#include "base/tracked_objects.h"

#include <stddef.h>

#include "base/memory/scoped_ptr.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "base/tracking_info.h"
#include "testing/gtest/include/gtest/gtest.h"

const int kLineNumber = 1776;
const char kFile[] = "FixedUnitTestFileName";
const char kWorkerThreadName[] = "WorkerThread-1";
const char kMainThreadName[] = "SomeMainThreadName";
const char kStillAlive[] = "Still_Alive";

namespace tracked_objects {

class TrackedObjectsTest : public testing::Test {
 protected:
  TrackedObjectsTest() {
    // On entry, leak any database structures in case they are still in use by
    // prior threads.
    ThreadData::ShutdownSingleThreadedCleanup(true);

    test_time_ = 0;
    ThreadData::SetAlternateTimeSource(&TrackedObjectsTest::GetTestTime);
    ThreadData::now_function_is_time_ = true;
  }

  ~TrackedObjectsTest() override {
    // We should not need to leak any structures we create, since we are
    // single threaded, and carefully accounting for items.
    ThreadData::ShutdownSingleThreadedCleanup(false);
  }

  // Reset the profiler state.
  void Reset() {
    ThreadData::ShutdownSingleThreadedCleanup(false);
    test_time_ = 0;
  }

  // Simulate a birth on the thread named |thread_name|, at the given
  // |location|.
  void TallyABirth(const Location& location, const std::string& thread_name) {
    // If the |thread_name| is empty, we don't initialize system with a thread
    // name, so we're viewed as a worker thread.
    if (!thread_name.empty())
      ThreadData::InitializeThreadContext(kMainThreadName);

    // Do not delete |birth|.  We don't own it.
    Births* birth = ThreadData::TallyABirthIfActive(location);

    if (ThreadData::status() == ThreadData::DEACTIVATED)
      EXPECT_EQ(static_cast<Births*>(NULL), birth);
    else
      EXPECT_NE(static_cast<Births*>(NULL), birth);
  }

  // Helper function to verify the most common test expectations.
  void ExpectSimpleProcessData(const ProcessDataSnapshot& process_data,
                               const std::string& function_name,
                               const std::string& birth_thread,
                               const std::string& death_thread,
                               int count,
                               int run_ms,
                               int queue_ms) {
    ASSERT_EQ(1u, process_data.tasks.size());

    EXPECT_EQ(kFile, process_data.tasks[0].birth.location.file_name);
    EXPECT_EQ(function_name,
              process_data.tasks[0].birth.location.function_name);
    EXPECT_EQ(kLineNumber, process_data.tasks[0].birth.location.line_number);

    EXPECT_EQ(birth_thread, process_data.tasks[0].birth.thread_name);

    EXPECT_EQ(count, process_data.tasks[0].death_data.count);
    EXPECT_EQ(count * run_ms,
              process_data.tasks[0].death_data.run_duration_sum);
    EXPECT_EQ(run_ms, process_data.tasks[0].death_data.run_duration_max);
    EXPECT_EQ(run_ms, process_data.tasks[0].death_data.run_duration_sample);
    EXPECT_EQ(count * queue_ms,
              process_data.tasks[0].death_data.queue_duration_sum);
    EXPECT_EQ(queue_ms, process_data.tasks[0].death_data.queue_duration_max);
    EXPECT_EQ(queue_ms, process_data.tasks[0].death_data.queue_duration_sample);

    EXPECT_EQ(death_thread, process_data.tasks[0].death_thread_name);

    EXPECT_EQ(0u, process_data.descendants.size());

    EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
  }

  // Sets time that will be returned by ThreadData::Now().
  static void SetTestTime(unsigned int test_time) { test_time_ = test_time; }

 private:
  // Returns test time in milliseconds.
  static unsigned int GetTestTime() { return test_time_; }

  // Test time in milliseconds.
  static unsigned int test_time_;
};

// static
unsigned int TrackedObjectsTest::test_time_;

TEST_F(TrackedObjectsTest, TaskStopwatchNoStartStop) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  // Check that creating and destroying a stopwatch without starting it doesn't
  // crash.
  TaskStopwatch stopwatch;
}

TEST_F(TrackedObjectsTest, MinimalStartupShutdown) {
  // Minimal test doesn't even create any tasks.
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  ThreadData* data = ThreadData::Get();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  ASSERT_TRUE(data);
  EXPECT_FALSE(data->next());
  EXPECT_EQ(data, ThreadData::Get());
  ThreadData::BirthMap birth_map;
  ThreadData::DeathMap death_map;
  ThreadData::ParentChildSet parent_child_set;
  data->SnapshotMaps(false, &birth_map, &death_map, &parent_child_set);
  EXPECT_EQ(0u, birth_map.size());
  EXPECT_EQ(0u, death_map.size());
  EXPECT_EQ(0u, parent_child_set.size());

  // Clean up with no leaking.
  Reset();

  // Do it again, just to be sure we reset state completely.
  EXPECT_TRUE(ThreadData::InitializeAndSetTrackingStatus(
      ThreadData::PROFILING_CHILDREN_ACTIVE));
  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  data = ThreadData::Get();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  ASSERT_TRUE(data);
  EXPECT_FALSE(data->next());
  EXPECT_EQ(data, ThreadData::Get());
  birth_map.clear();
  death_map.clear();
  parent_child_set.clear();
  data->SnapshotMaps(false, &birth_map, &death_map, &parent_child_set);
  EXPECT_EQ(0u, birth_map.size());
  EXPECT_EQ(0u, death_map.size());
  EXPECT_EQ(0u, parent_child_set.size());
}

TEST_F(TrackedObjectsTest, TinyStartupShutdown) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  // Instigate tracking on a single tracked object, on our thread.
  const char kFunction[] = "TinyStartupShutdown";
  Location location(kFunction, kFile, kLineNumber, NULL);
  Births* first_birth = ThreadData::TallyABirthIfActive(location);

  ThreadData* data = ThreadData::first();
  ASSERT_TRUE(data);
  EXPECT_FALSE(data->next());
  EXPECT_EQ(data, ThreadData::Get());
  ThreadData::BirthMap birth_map;
  ThreadData::DeathMap death_map;
  ThreadData::ParentChildSet parent_child_set;
  data->SnapshotMaps(false, &birth_map, &death_map, &parent_child_set);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(1, birth_map.begin()->second->birth_count());  // 1 birth.
  EXPECT_EQ(0u, death_map.size());                         // No deaths.
  EXPECT_EQ(0u, parent_child_set.size());                  // No children.


  // Now instigate another birth, while we are timing the run of the first
  // execution.
  ThreadData::PrepareForStartOfRun(first_birth);
  // Create a child (using the same birth location).
  // TrackingInfo will call TallyABirth() during construction.
  const int32 start_time = 1;
  base::TimeTicks kBogusBirthTime = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(start_time);
  base::TrackingInfo pending_task(location, kBogusBirthTime);
  SetTestTime(1);
  TaskStopwatch stopwatch;
  stopwatch.Start();
  // Finally conclude the outer run.
  const int32 time_elapsed = 1000;
  SetTestTime(start_time + time_elapsed);
  stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, stopwatch);

  birth_map.clear();
  death_map.clear();
  parent_child_set.clear();
  data->SnapshotMaps(false, &birth_map, &death_map, &parent_child_set);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(2, birth_map.begin()->second->birth_count());  // 2 births.
  EXPECT_EQ(1u, death_map.size());                         // 1 location.
  EXPECT_EQ(1, death_map.begin()->second.count());         // 1 death.
  if (ThreadData::TrackingParentChildStatus()) {
    EXPECT_EQ(1u, parent_child_set.size());                  // 1 child.
    EXPECT_EQ(parent_child_set.begin()->first,
              parent_child_set.begin()->second);
  } else {
    EXPECT_EQ(0u, parent_child_set.size());                  // no stats.
  }

  // The births were at the same location as the one known death.
  EXPECT_EQ(birth_map.begin()->second, death_map.begin()->first);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);

  ASSERT_EQ(1u, process_data.tasks.size());
  EXPECT_EQ(kFile, process_data.tasks[0].birth.location.file_name);
  EXPECT_EQ(kFunction, process_data.tasks[0].birth.location.function_name);
  EXPECT_EQ(kLineNumber, process_data.tasks[0].birth.location.line_number);
  EXPECT_EQ(kWorkerThreadName, process_data.tasks[0].birth.thread_name);
  EXPECT_EQ(1, process_data.tasks[0].death_data.count);
  EXPECT_EQ(time_elapsed, process_data.tasks[0].death_data.run_duration_sum);
  EXPECT_EQ(time_elapsed, process_data.tasks[0].death_data.run_duration_max);
  EXPECT_EQ(time_elapsed, process_data.tasks[0].death_data.run_duration_sample);
  EXPECT_EQ(0, process_data.tasks[0].death_data.queue_duration_sum);
  EXPECT_EQ(0, process_data.tasks[0].death_data.queue_duration_max);
  EXPECT_EQ(0, process_data.tasks[0].death_data.queue_duration_sample);
  EXPECT_EQ(kWorkerThreadName, process_data.tasks[0].death_thread_name);

  if (ThreadData::TrackingParentChildStatus()) {
    ASSERT_EQ(1u, process_data.descendants.size());
    EXPECT_EQ(kFile, process_data.descendants[0].parent.location.file_name);
    EXPECT_EQ(kFunction,
              process_data.descendants[0].parent.location.function_name);
    EXPECT_EQ(kLineNumber,
              process_data.descendants[0].parent.location.line_number);
    EXPECT_EQ(kWorkerThreadName,
              process_data.descendants[0].parent.thread_name);
    EXPECT_EQ(kFile, process_data.descendants[0].child.location.file_name);
    EXPECT_EQ(kFunction,
              process_data.descendants[0].child.location.function_name);
    EXPECT_EQ(kLineNumber,
              process_data.descendants[0].child.location.line_number);
    EXPECT_EQ(kWorkerThreadName, process_data.descendants[0].child.thread_name);
  } else {
    EXPECT_EQ(0u, process_data.descendants.size());
  }
}

TEST_F(TrackedObjectsTest, DeathDataTest) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  scoped_ptr<DeathData> data(new DeathData());
  ASSERT_NE(data, static_cast<DeathData*>(NULL));
  EXPECT_EQ(data->run_duration_sum(), 0);
  EXPECT_EQ(data->run_duration_sample(), 0);
  EXPECT_EQ(data->queue_duration_sum(), 0);
  EXPECT_EQ(data->queue_duration_sample(), 0);
  EXPECT_EQ(data->count(), 0);

  int32 run_ms = 42;
  int32 queue_ms = 8;

  const int kUnrandomInt = 0;  // Fake random int that ensure we sample data.
  data->RecordDeath(queue_ms, run_ms, kUnrandomInt);
  EXPECT_EQ(data->run_duration_sum(), run_ms);
  EXPECT_EQ(data->run_duration_sample(), run_ms);
  EXPECT_EQ(data->queue_duration_sum(), queue_ms);
  EXPECT_EQ(data->queue_duration_sample(), queue_ms);
  EXPECT_EQ(data->count(), 1);

  data->RecordDeath(queue_ms, run_ms, kUnrandomInt);
  EXPECT_EQ(data->run_duration_sum(), run_ms + run_ms);
  EXPECT_EQ(data->run_duration_sample(), run_ms);
  EXPECT_EQ(data->queue_duration_sum(), queue_ms + queue_ms);
  EXPECT_EQ(data->queue_duration_sample(), queue_ms);
  EXPECT_EQ(data->count(), 2);

  DeathDataSnapshot snapshot(*data);
  EXPECT_EQ(2, snapshot.count);
  EXPECT_EQ(2 * run_ms, snapshot.run_duration_sum);
  EXPECT_EQ(run_ms, snapshot.run_duration_max);
  EXPECT_EQ(run_ms, snapshot.run_duration_sample);
  EXPECT_EQ(2 * queue_ms, snapshot.queue_duration_sum);
  EXPECT_EQ(queue_ms, snapshot.queue_duration_max);
  EXPECT_EQ(queue_ms, snapshot.queue_duration_sample);
}

TEST_F(TrackedObjectsTest, DeactivatedBirthOnlyToSnapshotWorkerThread) {
  // Start in the deactivated state.
  if (!ThreadData::InitializeAndSetTrackingStatus(ThreadData::DEACTIVATED)) {
    return;
  }

  const char kFunction[] = "DeactivatedBirthOnlyToSnapshotWorkerThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, std::string());

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  EXPECT_EQ(0u, process_data.tasks.size());
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

TEST_F(TrackedObjectsTest, DeactivatedBirthOnlyToSnapshotMainThread) {
  // Start in the deactivated state.
  if (!ThreadData::InitializeAndSetTrackingStatus(ThreadData::DEACTIVATED)) {
    return;
  }

  const char kFunction[] = "DeactivatedBirthOnlyToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  EXPECT_EQ(0u, process_data.tasks.size());
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

TEST_F(TrackedObjectsTest, BirthOnlyToSnapshotWorkerThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "BirthOnlyToSnapshotWorkerThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, std::string());

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kWorkerThreadName,
                          kStillAlive, 1, 0, 0);
}

TEST_F(TrackedObjectsTest, BirthOnlyToSnapshotMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "BirthOnlyToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName, kStillAlive,
                          1, 0, 0);
}

TEST_F(TrackedObjectsTest, LifeCycleToSnapshotMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "LifeCycleToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const TrackedTime kTimePosted = TrackedTime::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const unsigned int kStartOfRun = 5;
  const unsigned int kEndOfRun = 7;
  SetTestTime(kStartOfRun);
  TaskStopwatch stopwatch;
  stopwatch.Start();
  SetTestTime(kEndOfRun);
  stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, stopwatch);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName,
                          kMainThreadName, 1, 2, 4);
}

// We will deactivate tracking after the birth, and before the death, and
// demonstrate that the lifecycle is completely tallied. This ensures that
// our tallied births are matched by tallied deaths (except for when the
// task is still running, or is queued).
TEST_F(TrackedObjectsTest, LifeCycleMidDeactivatedToSnapshotMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "LifeCycleMidDeactivatedToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const TrackedTime kTimePosted = TrackedTime::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  // Turn off tracking now that we have births.
  EXPECT_TRUE(ThreadData::InitializeAndSetTrackingStatus(
      ThreadData::DEACTIVATED));

  const unsigned int kStartOfRun = 5;
  const unsigned int kEndOfRun = 7;
  SetTestTime(kStartOfRun);
  TaskStopwatch stopwatch;
  stopwatch.Start();
  SetTestTime(kEndOfRun);
  stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, stopwatch);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName,
                          kMainThreadName, 1, 2, 4);
}

// We will deactivate tracking before starting a life cycle, and neither
// the birth nor the death will be recorded.
TEST_F(TrackedObjectsTest, LifeCyclePreDeactivatedToSnapshotMainThread) {
  // Start in the deactivated state.
  if (!ThreadData::InitializeAndSetTrackingStatus(ThreadData::DEACTIVATED)) {
    return;
  }

  const char kFunction[] = "LifeCyclePreDeactivatedToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const TrackedTime kTimePosted = TrackedTime::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const unsigned int kStartOfRun = 5;
  const unsigned int kEndOfRun = 7;
  SetTestTime(kStartOfRun);
  TaskStopwatch stopwatch;
  stopwatch.Start();
  SetTestTime(kEndOfRun);
  stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, stopwatch);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  EXPECT_EQ(0u, process_data.tasks.size());
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

TEST_F(TrackedObjectsTest, LifeCycleToSnapshotWorkerThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "LifeCycleToSnapshotWorkerThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  // Do not delete |birth|.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(static_cast<Births*>(NULL), birth);

  const unsigned int kTimePosted = 1;
  const unsigned int kStartOfRun = 5;
  const unsigned int kEndOfRun = 7;
  SetTestTime(kStartOfRun);
  TaskStopwatch stopwatch;
  stopwatch.Start();
  SetTestTime(kEndOfRun);
  stopwatch.Stop();

  ThreadData::TallyRunOnWorkerThreadIfTracking(
  birth, TrackedTime() + Duration::FromMilliseconds(kTimePosted), stopwatch);

  // Call for the ToSnapshot, but tell it to not reset the maxes after scanning.
  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kWorkerThreadName,
                          kWorkerThreadName, 1, 2, 4);

  // Call for the ToSnapshot, but tell it to reset the maxes after scanning.
  // We'll still get the same values, but the data will be reset (which we'll
  // see in a moment).
  ProcessDataSnapshot process_data_pre_reset;
  ThreadData::Snapshot(true, &process_data_pre_reset);
  ExpectSimpleProcessData(process_data, kFunction, kWorkerThreadName,
                          kWorkerThreadName, 1, 2, 4);

  // Call for the ToSnapshot, and now we'll see the result of the last
  // translation, as the max will have been pushed back to zero.
  ProcessDataSnapshot process_data_post_reset;
  ThreadData::Snapshot(true, &process_data_post_reset);
  ASSERT_EQ(1u, process_data_post_reset.tasks.size());
  EXPECT_EQ(kFile, process_data_post_reset.tasks[0].birth.location.file_name);
  EXPECT_EQ(kFunction,
            process_data_post_reset.tasks[0].birth.location.function_name);
  EXPECT_EQ(kLineNumber,
            process_data_post_reset.tasks[0].birth.location.line_number);
  EXPECT_EQ(kWorkerThreadName,
            process_data_post_reset.tasks[0].birth.thread_name);
  EXPECT_EQ(1, process_data_post_reset.tasks[0].death_data.count);
  EXPECT_EQ(2, process_data_post_reset.tasks[0].death_data.run_duration_sum);
  EXPECT_EQ(0, process_data_post_reset.tasks[0].death_data.run_duration_max);
  EXPECT_EQ(2, process_data_post_reset.tasks[0].death_data.run_duration_sample);
  EXPECT_EQ(4, process_data_post_reset.tasks[0].death_data.queue_duration_sum);
  EXPECT_EQ(0, process_data_post_reset.tasks[0].death_data.queue_duration_max);
  EXPECT_EQ(4,
            process_data_post_reset.tasks[0].death_data.queue_duration_sample);
  EXPECT_EQ(kWorkerThreadName,
            process_data_post_reset.tasks[0].death_thread_name);
  EXPECT_EQ(0u, process_data_post_reset.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data_post_reset.process_id);
}

TEST_F(TrackedObjectsTest, TwoLives) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "TwoLives";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const TrackedTime kTimePosted = TrackedTime::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const unsigned int kStartOfRun = 5;
  const unsigned int kEndOfRun = 7;
  SetTestTime(kStartOfRun);
  TaskStopwatch stopwatch;
  stopwatch.Start();
  SetTestTime(kEndOfRun);
  stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, stopwatch);

  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task2(location, kDelayedStartTime);
  pending_task2.time_posted = kTimePosted;  // Overwrite implied Now().
  SetTestTime(kStartOfRun);
  TaskStopwatch stopwatch2;
  stopwatch2.Start();
  SetTestTime(kEndOfRun);
  stopwatch2.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task2, stopwatch2);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName,
                          kMainThreadName, 2, 2, 4);
}

TEST_F(TrackedObjectsTest, DifferentLives) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  // Use a well named thread.
  ThreadData::InitializeThreadContext(kMainThreadName);
  const char kFunction[] = "DifferentLives";
  Location location(kFunction, kFile, kLineNumber, NULL);

  const TrackedTime kTimePosted = TrackedTime::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const unsigned int kStartOfRun = 5;
  const unsigned int kEndOfRun = 7;
  SetTestTime(kStartOfRun);
  TaskStopwatch stopwatch;
  stopwatch.Start();
  SetTestTime(kEndOfRun);
  stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, stopwatch);

  const int kSecondFakeLineNumber = 999;
  Location second_location(kFunction, kFile, kSecondFakeLineNumber, NULL);

  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task2(second_location, kDelayedStartTime);
  pending_task2.time_posted = kTimePosted;  // Overwrite implied Now().

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ASSERT_EQ(2u, process_data.tasks.size());

  EXPECT_EQ(kFile, process_data.tasks[0].birth.location.file_name);
  EXPECT_EQ(kFunction, process_data.tasks[0].birth.location.function_name);
  EXPECT_EQ(kLineNumber, process_data.tasks[0].birth.location.line_number);
  EXPECT_EQ(kMainThreadName, process_data.tasks[0].birth.thread_name);
  EXPECT_EQ(1, process_data.tasks[0].death_data.count);
  EXPECT_EQ(2, process_data.tasks[0].death_data.run_duration_sum);
  EXPECT_EQ(2, process_data.tasks[0].death_data.run_duration_max);
  EXPECT_EQ(2, process_data.tasks[0].death_data.run_duration_sample);
  EXPECT_EQ(4, process_data.tasks[0].death_data.queue_duration_sum);
  EXPECT_EQ(4, process_data.tasks[0].death_data.queue_duration_max);
  EXPECT_EQ(4, process_data.tasks[0].death_data.queue_duration_sample);
  EXPECT_EQ(kMainThreadName, process_data.tasks[0].death_thread_name);
  EXPECT_EQ(kFile, process_data.tasks[1].birth.location.file_name);
  EXPECT_EQ(kFunction, process_data.tasks[1].birth.location.function_name);
  EXPECT_EQ(kSecondFakeLineNumber,
            process_data.tasks[1].birth.location.line_number);
  EXPECT_EQ(kMainThreadName, process_data.tasks[1].birth.thread_name);
  EXPECT_EQ(1, process_data.tasks[1].death_data.count);
  EXPECT_EQ(0, process_data.tasks[1].death_data.run_duration_sum);
  EXPECT_EQ(0, process_data.tasks[1].death_data.run_duration_max);
  EXPECT_EQ(0, process_data.tasks[1].death_data.run_duration_sample);
  EXPECT_EQ(0, process_data.tasks[1].death_data.queue_duration_sum);
  EXPECT_EQ(0, process_data.tasks[1].death_data.queue_duration_max);
  EXPECT_EQ(0, process_data.tasks[1].death_data.queue_duration_sample);
  EXPECT_EQ(kStillAlive, process_data.tasks[1].death_thread_name);
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

TEST_F(TrackedObjectsTest, TaskWithNestedExclusion) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "TaskWithNestedExclusion";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const TrackedTime kTimePosted = TrackedTime::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  SetTestTime(5);
  TaskStopwatch task_stopwatch;
  task_stopwatch.Start();
  {
    SetTestTime(8);
    TaskStopwatch exclusion_stopwatch;
    exclusion_stopwatch.Start();
    SetTestTime(12);
    exclusion_stopwatch.Stop();
  }
  SetTestTime(15);
  task_stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, task_stopwatch);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName,
                          kMainThreadName, 1, 6, 4);
}

TEST_F(TrackedObjectsTest, TaskWith2NestedExclusions) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "TaskWith2NestedExclusions";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const TrackedTime kTimePosted = TrackedTime::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  SetTestTime(5);
  TaskStopwatch task_stopwatch;
  task_stopwatch.Start();
  {
    SetTestTime(8);
    TaskStopwatch exclusion_stopwatch;
    exclusion_stopwatch.Start();
    SetTestTime(12);
    exclusion_stopwatch.Stop();

    SetTestTime(15);
    TaskStopwatch exclusion_stopwatch2;
    exclusion_stopwatch2.Start();
    SetTestTime(18);
    exclusion_stopwatch2.Stop();
  }
  SetTestTime(25);
  task_stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, task_stopwatch);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName,
                          kMainThreadName, 1, 13, 4);
}

TEST_F(TrackedObjectsTest, TaskWithNestedExclusionWithNestedTask) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE)) {
    return;
  }

  const char kFunction[] = "TaskWithNestedExclusionWithNestedTask";
  Location location(kFunction, kFile, kLineNumber, NULL);

  const int kSecondFakeLineNumber = 999;

  TallyABirth(location, kMainThreadName);

  const TrackedTime kTimePosted = TrackedTime::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  SetTestTime(5);
  TaskStopwatch task_stopwatch;
  task_stopwatch.Start();
  {
    SetTestTime(8);
    TaskStopwatch exclusion_stopwatch;
    exclusion_stopwatch.Start();
    {
      Location second_location(kFunction, kFile, kSecondFakeLineNumber, NULL);
      base::TrackingInfo nested_task(second_location, kDelayedStartTime);
       // Overwrite implied Now().
      nested_task.time_posted = TrackedTime::FromMilliseconds(8);
      SetTestTime(9);
      TaskStopwatch nested_task_stopwatch;
      nested_task_stopwatch.Start();
      SetTestTime(11);
      nested_task_stopwatch.Stop();
      ThreadData::TallyRunOnNamedThreadIfTracking(
          nested_task, nested_task_stopwatch);
    }
    SetTestTime(12);
    exclusion_stopwatch.Stop();
  }
  SetTestTime(15);
  task_stopwatch.Stop();

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, task_stopwatch);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);

  // The order in which the two task follow is platform-dependent.
  int t0 = (process_data.tasks[0].birth.location.line_number == kLineNumber) ?
      0 : 1;
  int t1 = 1 - t0;

  ASSERT_EQ(2u, process_data.tasks.size());
  EXPECT_EQ(kFile, process_data.tasks[t0].birth.location.file_name);
  EXPECT_EQ(kFunction, process_data.tasks[t0].birth.location.function_name);
  EXPECT_EQ(kLineNumber, process_data.tasks[t0].birth.location.line_number);
  EXPECT_EQ(kMainThreadName, process_data.tasks[t0].birth.thread_name);
  EXPECT_EQ(1, process_data.tasks[t0].death_data.count);
  EXPECT_EQ(6, process_data.tasks[t0].death_data.run_duration_sum);
  EXPECT_EQ(6, process_data.tasks[t0].death_data.run_duration_max);
  EXPECT_EQ(6, process_data.tasks[t0].death_data.run_duration_sample);
  EXPECT_EQ(4, process_data.tasks[t0].death_data.queue_duration_sum);
  EXPECT_EQ(4, process_data.tasks[t0].death_data.queue_duration_max);
  EXPECT_EQ(4, process_data.tasks[t0].death_data.queue_duration_sample);
  EXPECT_EQ(kMainThreadName, process_data.tasks[t0].death_thread_name);
  EXPECT_EQ(kFile, process_data.tasks[t1].birth.location.file_name);
  EXPECT_EQ(kFunction, process_data.tasks[t1].birth.location.function_name);
  EXPECT_EQ(kSecondFakeLineNumber,
            process_data.tasks[t1].birth.location.line_number);
  EXPECT_EQ(kMainThreadName, process_data.tasks[t1].birth.thread_name);
  EXPECT_EQ(1, process_data.tasks[t1].death_data.count);
  EXPECT_EQ(2, process_data.tasks[t1].death_data.run_duration_sum);
  EXPECT_EQ(2, process_data.tasks[t1].death_data.run_duration_max);
  EXPECT_EQ(2, process_data.tasks[t1].death_data.run_duration_sample);
  EXPECT_EQ(1, process_data.tasks[t1].death_data.queue_duration_sum);
  EXPECT_EQ(1, process_data.tasks[t1].death_data.queue_duration_max);
  EXPECT_EQ(1, process_data.tasks[t1].death_data.queue_duration_sample);
  EXPECT_EQ(kMainThreadName, process_data.tasks[t1].death_thread_name);
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

}  // namespace tracked_objects
