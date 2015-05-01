// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_store.h"

namespace safe_browsing {
class PrefixSet;
}

class GURL;
class SafeBrowsingDatabase;

// Factory for creating SafeBrowsingDatabase. Tests implement this factory
// to create fake Databases for testing.
class SafeBrowsingDatabaseFactory {
 public:
  SafeBrowsingDatabaseFactory() { }
  virtual ~SafeBrowsingDatabaseFactory() { }
  virtual SafeBrowsingDatabase* CreateSafeBrowsingDatabase(
      bool enable_download_protection,
      bool enable_client_side_whitelist,
      bool enable_download_whitelist,
      bool enable_extension_blacklist,
      bool enable_side_effect_free_whitelist,
      bool enable_ip_blacklist,
      bool enable_unwanted_software_list) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseFactory);
};

// Encapsulates on-disk databases that for safebrowsing. There are
// four databases: browse, download, download whitelist and
// client-side detection (csd) whitelist databases. The browse database contains
// information about phishing and malware urls. The download database contains
// URLs for bad binaries (e.g: those containing virus) and hash of
// these downloaded contents. The download whitelist contains whitelisted
// download hosting sites as well as whitelisted binary signing certificates
// etc.  The csd whitelist database contains URLs that will never be considered
// as phishing by the client-side phishing detection. These on-disk databases
// are shared among all profiles, as it doesn't contain user-specific data. This
// object is not thread-safe, i.e. all its methods should be used on the same
// thread that it was created on, unless specified otherwise.
class SafeBrowsingDatabase {
 public:
  // Factory method for obtaining a SafeBrowsingDatabase implementation.
  // It is not thread safe.
  // The browse list and off-domain inclusion whitelist are always on;
  // availability of other lists is controlled by the flags on this method.
  static SafeBrowsingDatabase* Create(bool enable_download_protection,
                                      bool enable_client_side_whitelist,
                                      bool enable_download_whitelist,
                                      bool enable_extension_blacklist,
                                      bool side_effect_free_whitelist,
                                      bool enable_ip_blacklist,
                                      bool enable_unwanted_software_list);

  // Makes the passed |factory| the factory used to instantiate
  // a SafeBrowsingDatabase. This is used for tests.
  static void RegisterFactory(SafeBrowsingDatabaseFactory* factory) {
    factory_ = factory;
  }

  virtual ~SafeBrowsingDatabase();

  // Initializes the database with the given filename.
  virtual void Init(const base::FilePath& filename) = 0;

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase() = 0;

  // Returns false if |url| is not in the browse database or already was cached
  // as a miss.  If it returns true, |prefix_hits| contains sorted unique
  // matching hash prefixes which had no cached results and |cache_hits|
  // contains any matching cached gethash results.  This function is safe to
  // call from any thread.
  virtual bool ContainsBrowseUrl(
      const GURL& url,
      std::vector<SBPrefix>* prefix_hits,
      std::vector<SBFullHashResult>* cache_hits) = 0;

  // Returns true iff the given url is on the unwanted software blacklist.
  // Returns false if |url| is not in the browse database or already was cached
  // as a miss.  If it returns true, |prefix_hits| contains sorted unique
  // matching hash prefixes which had no cached results and |cache_hits|
  // contains any matching cached gethash results.  This function is safe to
  // call from any thread.
  virtual bool ContainsUnwantedSoftwareUrl(
      const GURL& url,
      std::vector<SBPrefix>* prefix_hits,
      std::vector<SBFullHashResult>* cache_hits) = 0;

  // Returns false if none of |urls| are in Download database. If it returns
  // true, |prefix_hits| should contain the prefixes for the URLs that were in
  // the database.  This function can ONLY be accessed from creation thread.
  virtual bool ContainsDownloadUrl(const std::vector<GURL>& urls,
                                   std::vector<SBPrefix>* prefix_hits) = 0;

  // Returns false if |url| is not on the client-side phishing detection
  // whitelist.  Otherwise, this function returns true.  Note: the whitelist
  // only contains full-length hashes so we don't return any prefix hit. This
  // function is safe to call from any thread.
  virtual bool ContainsCsdWhitelistedUrl(const GURL& url) = 0;

  // The download whitelist is used for two purposes: a white-domain list of
  // sites that are considered to host only harmless binaries as well as a
  // whitelist of arbitrary strings such as hashed certificate authorities that
  // are considered to be trusted.  The two methods below let you lookup the
  // whitelist either for a URL or an arbitrary string.  These methods will
  // return false if no match is found and true otherwise. This function is safe
  // to call from any thread.
  virtual bool ContainsDownloadWhitelistedUrl(const GURL& url) = 0;
  virtual bool ContainsDownloadWhitelistedString(const std::string& str) = 0;

  // Returns true if |url| is on the off-domain inclusion whitelist.
  virtual bool ContainsInclusionWhitelistedUrl(const GURL& url) = 0;

  // Populates |prefix_hits| with any prefixes in |prefixes| that have matches
  // in the database.
  //
  // This function can ONLY be accessed from the creation thread.
  virtual bool ContainsExtensionPrefixes(
      const std::vector<SBPrefix>& prefixes,
      std::vector<SBPrefix>* prefix_hits) = 0;

  // Returns false unless the hash of |url| is on the side-effect free
  // whitelist. This function is safe to call from any thread.
  virtual bool ContainsSideEffectFreeWhitelistUrl(const GURL& url) = 0;

  // Returns true iff the given IP is currently on the csd malware IP blacklist.
  // This function is safe to call from any thread.
  virtual bool ContainsMalwareIP(const std::string& ip_address) = 0;

  // A database transaction should look like:
  //
  // std::vector<SBListChunkRanges> lists;
  // if (db.UpdateStarted(&lists)) {
  //   // Do something with |lists|.
  //
  //   // Process add/sub commands.
  //   db.InsertChunks(list_name, chunks);
  //
  //   // Process adddel/subdel commands.
  //   db.DeleteChunks(chunks_deletes);
  //
  //   // If passed true, processes the collected chunk info and
  //   // rebuilds the filter.  If passed false, rolls everything
  //   // back.
  //   db.UpdateFinished(success);
  // }
  //
  // If UpdateStarted() returns true, the caller MUST eventually call
  // UpdateFinished().  If it returns false, the caller MUST NOT call
  // the other functions.
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists) = 0;
  virtual void InsertChunks(const std::string& list_name,
                            const std::vector<SBChunkData*>& chunks) = 0;
  virtual void DeleteChunks(
      const std::vector<SBChunkDelete>& chunk_deletes) = 0;
  virtual void UpdateFinished(bool update_succeeded) = 0;

  // Store the results of a GetHash response. In the case of empty results, we
  // cache the prefixes until the next update so that we don't have to issue
  // further GetHash requests we know will be empty. This function is safe to
  // call from any thread.
  virtual void CacheHashResults(
      const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& full_hits,
      const base::TimeDelta& cache_lifetime) = 0;

  // Returns true if the malware IP blacklisting killswitch URL is present
  // in the csd whitelist. This function is safe to call from any thread.
  virtual bool IsMalwareIPMatchKillSwitchOn() = 0;

  // Returns true if the whitelist killswitch URL is present in the csd
  // whitelist. This function is safe to call from any thread.
  virtual bool IsCsdWhitelistKillSwitchOn() = 0;

  // The name of the bloom-filter file for the given database file.
  // NOTE(shess): OBSOLETE.  Present for deleting stale files.
  static base::FilePath BloomFilterForFilename(
      const base::FilePath& db_filename);

  // The name of the prefix set file for the given database file.
  static base::FilePath PrefixSetForFilename(const base::FilePath& db_filename);

  // Filename for malware and phishing URL database.
  static base::FilePath BrowseDBFilename(
      const base::FilePath& db_base_filename);

  // Filename for download URL and download binary hash database.
  static base::FilePath DownloadDBFilename(
      const base::FilePath& db_base_filename);

  // Filename for client-side phishing detection whitelist databsae.
  static base::FilePath CsdWhitelistDBFilename(
      const base::FilePath& csd_whitelist_base_filename);

  // Filename for download whitelist databsae.
  static base::FilePath DownloadWhitelistDBFilename(
      const base::FilePath& download_whitelist_base_filename);

  // Filename for the off-domain inclusion whitelist databsae.
  static base::FilePath InclusionWhitelistDBFilename(
      const base::FilePath& inclusion_whitelist_base_filename);

  // Filename for extension blacklist database.
  static base::FilePath ExtensionBlacklistDBFilename(
      const base::FilePath& extension_blacklist_base_filename);

  // Filename for side-effect free whitelist database.
  static base::FilePath SideEffectFreeWhitelistDBFilename(
      const base::FilePath& side_effect_free_whitelist_base_filename);

  // Filename for the csd malware IP blacklist database.
  static base::FilePath IpBlacklistDBFilename(
      const base::FilePath& ip_blacklist_base_filename);

  // Filename for the unwanted software blacklist database.
  static base::FilePath UnwantedSoftwareDBFilename(
      const base::FilePath& db_filename);

  // Enumerate failures for histogramming purposes.  DO NOT CHANGE THE
  // ORDERING OF THESE VALUES.
  enum FailureType {
    FAILURE_DATABASE_CORRUPT,
    FAILURE_DATABASE_CORRUPT_HANDLER,
    FAILURE_BROWSE_DATABASE_UPDATE_BEGIN,
    FAILURE_BROWSE_DATABASE_UPDATE_FINISH,
    FAILURE_DATABASE_FILTER_MISSING_OBSOLETE,
    FAILURE_DATABASE_FILTER_READ_OBSOLETE,
    FAILURE_DATABASE_FILTER_WRITE_OBSOLETE,
    FAILURE_DATABASE_FILTER_DELETE,
    FAILURE_DATABASE_STORE_MISSING,
    FAILURE_DATABASE_STORE_DELETE,
    FAILURE_DOWNLOAD_DATABASE_UPDATE_BEGIN,
    FAILURE_DOWNLOAD_DATABASE_UPDATE_FINISH,
    FAILURE_WHITELIST_DATABASE_UPDATE_BEGIN,
    FAILURE_WHITELIST_DATABASE_UPDATE_FINISH,
    FAILURE_BROWSE_PREFIX_SET_READ,
    FAILURE_BROWSE_PREFIX_SET_WRITE,
    FAILURE_BROWSE_PREFIX_SET_DELETE,
    FAILURE_EXTENSION_BLACKLIST_UPDATE_BEGIN,
    FAILURE_EXTENSION_BLACKLIST_UPDATE_FINISH,
    FAILURE_EXTENSION_BLACKLIST_DELETE,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_UPDATE_BEGIN,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_UPDATE_FINISH,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_DELETE,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_READ,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_WRITE,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_DELETE,
    FAILURE_IP_BLACKLIST_UPDATE_BEGIN,
    FAILURE_IP_BLACKLIST_UPDATE_FINISH,
    FAILURE_IP_BLACKLIST_UPDATE_INVALID,
    FAILURE_IP_BLACKLIST_DELETE,
    FAILURE_UNWANTED_SOFTWARE_DATABASE_UPDATE_BEGIN,
    FAILURE_UNWANTED_SOFTWARE_DATABASE_UPDATE_FINISH,
    FAILURE_UNWANTED_SOFTWARE_PREFIX_SET_READ,
    FAILURE_UNWANTED_SOFTWARE_PREFIX_SET_WRITE,
    FAILURE_UNWANTED_SOFTWARE_PREFIX_SET_DELETE,

    // Memory space for histograms is determined by the max.  ALWAYS
    // ADD NEW VALUES BEFORE THIS ONE.
    FAILURE_DATABASE_MAX
  };

  static void RecordFailure(FailureType failure_type);

 private:
  // The factory used to instantiate a SafeBrowsingDatabase object.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingDatabase.
  static SafeBrowsingDatabaseFactory* factory_;
};

class SafeBrowsingDatabaseNew : public SafeBrowsingDatabase {
 public:
  // Create a database with the stores below. Takes ownership of all store
  // objects handed to this constructor. Ignores all future operations on lists
  // for which the store is initialized to NULL.
  SafeBrowsingDatabaseNew(SafeBrowsingStore* browse_store,
                          SafeBrowsingStore* download_store,
                          SafeBrowsingStore* csd_whitelist_store,
                          SafeBrowsingStore* download_whitelist_store,
                          SafeBrowsingStore* inclusion_whitelist_store,
                          SafeBrowsingStore* extension_blacklist_store,
                          SafeBrowsingStore* side_effect_free_whitelist_store,
                          SafeBrowsingStore* ip_blacklist_store,
                          SafeBrowsingStore* unwanted_software_store);

  // Create a database with a browse store. This is a legacy interface that
  // useds Sqlite.
  SafeBrowsingDatabaseNew();

  ~SafeBrowsingDatabaseNew() override;

  // Implement SafeBrowsingDatabase interface.
  void Init(const base::FilePath& filename) override;
  bool ResetDatabase() override;
  bool ContainsBrowseUrl(const GURL& url,
                         std::vector<SBPrefix>* prefix_hits,
                         std::vector<SBFullHashResult>* cache_hits) override;
  bool ContainsUnwantedSoftwareUrl(
      const GURL& url,
      std::vector<SBPrefix>* prefix_hits,
      std::vector<SBFullHashResult>* cache_hits) override;
  bool ContainsDownloadUrl(const std::vector<GURL>& urls,
                           std::vector<SBPrefix>* prefix_hits) override;
  bool ContainsCsdWhitelistedUrl(const GURL& url) override;
  bool ContainsDownloadWhitelistedUrl(const GURL& url) override;
  bool ContainsDownloadWhitelistedString(const std::string& str) override;
  bool ContainsInclusionWhitelistedUrl(const GURL& url) override;
  bool ContainsExtensionPrefixes(const std::vector<SBPrefix>& prefixes,
                                 std::vector<SBPrefix>* prefix_hits) override;
  bool ContainsSideEffectFreeWhitelistUrl(const GURL& url) override;
  bool ContainsMalwareIP(const std::string& ip_address) override;
  bool UpdateStarted(std::vector<SBListChunkRanges>* lists) override;
  void InsertChunks(const std::string& list_name,
                    const std::vector<SBChunkData*>& chunks) override;
  void DeleteChunks(const std::vector<SBChunkDelete>& chunk_deletes) override;
  void UpdateFinished(bool update_succeeded) override;
  void CacheHashResults(const std::vector<SBPrefix>& prefixes,
                        const std::vector<SBFullHashResult>& full_hits,
                        const base::TimeDelta& cache_lifetime) override;

  // Returns the value of malware_kill_switch_;
  bool IsMalwareIPMatchKillSwitchOn() override;

  // Returns true if the CSD whitelist has everything whitelisted.
  bool IsCsdWhitelistKillSwitchOn() override;

 private:
  friend class SafeBrowsingDatabaseTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, HashCaching);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, CachedFullMiss);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, CachedPrefixHitFullMiss);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, BrowseFullHashMatching);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest,
                           BrowseFullHashAndPrefixMatching);

  // A SafeBrowsing whitelist contains a list of whitelisted full-hashes (stored
  // in a sorted vector) as well as a boolean flag indicating whether all
  // lookups in the whitelist should be considered matches for safety.
  typedef std::pair<std::vector<SBFullHash>, bool> SBWhitelist;

  // This map holds a csd malware IP blacklist which maps a prefix mask
  // to a set of hashed blacklisted IP prefixes.  Each IP prefix is a hashed
  // IPv6 IP prefix using SHA-1.
  typedef std::map<std::string, base::hash_set<std::string> > IPBlacklist;

  typedef std::map<SBPrefix, SBCachedFullHashResult> PrefixGetHashCache;

  // The ThreadSafeStateManager holds the SafeBrowsingDatabase's state which
  // must be accessed in a thread-safe fashion. It must be constructed on the
  // SafeBrowsingDatabaseManager's main thread. The main thread will then be the
  // only thread on which this state can be modified; allowing for unlocked
  // reads on the main thread and thus avoiding contention while performing
  // intensive operations such as writing that state to disk. The state can only
  // be accessed via (Read|Write)Transactions obtained through this class which
  // will automatically handle thread-safety.
  class ThreadSafeStateManager {
   public:
    // Identifiers for stores held by the ThreadSafeStateManager. Allows helper
    // methods to start a transaction themselves and keep it as short as
    // possible rather than force callers to start the transaction early to pass
    // a store pointer to the said helper methods.
    enum class SBWhitelistId {
      CSD,
      DOWNLOAD,
      INCLUSION,
    };
    enum class PrefixSetId {
      BROWSE,
      SIDE_EFFECT_FREE_WHITELIST,
      UNWANTED_SOFTWARE,
    };

    // Obtained through BeginReadTransaction(NoLockOnMainThread)?(): a
    // ReadTransaction allows read-only observations of the
    // ThreadSafeStateManager's state. The |prefix_gethash_cache_| has a special
    // allowance to be writable from a ReadTransaction but can't benefit from
    // unlocked ReadTransactions. ReadTransaction should be held for the
    // shortest amount of time possible (e.g., release it before computing final
    // results if possible).
    class ReadTransaction;

    // Obtained through BeginWriteTransaction(): a WriteTransaction allows
    // modification of the ThreadSafeStateManager's state. It should be used for
    // the shortest amount of time possible (e.g., pre-compute the new state
    // before grabbing a WriteTransaction to swap it in atomically).
    class WriteTransaction;

    explicit ThreadSafeStateManager(const base::ThreadChecker& thread_checker);
    ~ThreadSafeStateManager();

    scoped_ptr<ReadTransaction> BeginReadTransaction();
    scoped_ptr<ReadTransaction> BeginReadTransactionNoLockOnMainThread();
    scoped_ptr<WriteTransaction> BeginWriteTransaction();

   private:
    // The SafeBrowsingDatabase's ThreadChecker, used to verify that writes are
    // only made on its main thread. This is important as it allows reading from
    // the main thread without holding the lock.
    const base::ThreadChecker& thread_checker_;

    // Lock for protecting access to this class' state.
    mutable base::Lock lock_;

    SBWhitelist csd_whitelist_;
    SBWhitelist download_whitelist_;
    SBWhitelist inclusion_whitelist_;

    // The IP blacklist should be small.  At most a couple hundred IPs.
    IPBlacklist ip_blacklist_;

    // PrefixSets to speed up lookups for particularly large lists. The
    // PrefixSet themselves are never modified, instead a new one is swapped in
    // on update.
    scoped_ptr<const safe_browsing::PrefixSet> browse_prefix_set_;
    scoped_ptr<const safe_browsing::PrefixSet>
        side_effect_free_whitelist_prefix_set_;
    scoped_ptr<const safe_browsing::PrefixSet> unwanted_software_prefix_set_;

    // Cache of gethash results for prefix stores. Entries should not be used if
    // they are older than their expire_after field.  Cached misses will have
    // empty full_hashes field.  Cleared on each update. The cache is "mutable"
    // as it can be written to from any transaction holding the lock, including
    // ReadTransactions.
    mutable PrefixGetHashCache prefix_gethash_cache_;

    DISALLOW_COPY_AND_ASSIGN(ThreadSafeStateManager);
  };

  // Forward the above inner-definitions to alleviate some verbosity in the
  // impl.
  using SBWhitelistId = ThreadSafeStateManager::SBWhitelistId;
  using PrefixSetId = ThreadSafeStateManager::PrefixSetId;
  using ReadTransaction = ThreadSafeStateManager::ReadTransaction;
  using WriteTransaction = ThreadSafeStateManager::WriteTransaction;

  // Manages the non-thread safe (i.e. only to be accessed to the database's
  // main thread) state of this class.
  class DatabaseStateManager {
   public:
    explicit DatabaseStateManager(const base::ThreadChecker& thread_checker)
        : thread_checker_(thread_checker),
          corruption_detected_(false),
          change_detected_(false) {}

    void init_filename_base(const base::FilePath& filename_base) {
      DCHECK(thread_checker_.CalledOnValidThread());
      DCHECK(filename_base_.empty()) << "filename already initialized";
      filename_base_ = filename_base;
    }

    const base::FilePath& filename_base() {
      DCHECK(thread_checker_.CalledOnValidThread());
      return filename_base_;
    }

    void set_corruption_detected() {
      DCHECK(thread_checker_.CalledOnValidThread());
      corruption_detected_ = true;
    }

    void reset_corruption_detected() {
      DCHECK(thread_checker_.CalledOnValidThread());
      corruption_detected_ = false;
    }

    bool corruption_detected() {
      DCHECK(thread_checker_.CalledOnValidThread());
      return corruption_detected_;
    }

    void set_change_detected() {
      DCHECK(thread_checker_.CalledOnValidThread());
      change_detected_ = true;
    }

    void reset_change_detected() {
      DCHECK(thread_checker_.CalledOnValidThread());
      change_detected_ = false;
    }

    bool change_detected() {
      DCHECK(thread_checker_.CalledOnValidThread());
      return change_detected_;
    }

   private:
    // The SafeBrowsingDatabase's ThreadChecker, used to verify that this class'
    // state is only ever accessed from the database's main thread.
    const base::ThreadChecker& thread_checker_;

    // The base filename passed to Init(), used to generate the store and prefix
    // set filenames used to store data on disk.
    base::FilePath filename_base_;

    // Set if corruption is detected during the course of an update.
    // Causes the update functions to fail with no side effects, until
    // the next call to |UpdateStarted()|.
    bool corruption_detected_;

    // Set to true if any chunks are added or deleted during an update.
    // Used to optimize away database update.
    bool change_detected_;

    DISALLOW_COPY_AND_ASSIGN(DatabaseStateManager);
  };

  bool PrefixSetContainsUrl(const GURL& url,
                            PrefixSetId prefix_set_id,
                            std::vector<SBPrefix>* prefix_hits,
                            std::vector<SBFullHashResult>* cache_hits);

  // Exposed for testing of PrefixSetContainsUrlHashes() on the
  // PrefixSet backing kMalwareList.
  bool ContainsBrowseUrlHashesForTesting(
      const std::vector<SBFullHash>& full_hashes,
      std::vector<SBPrefix>* prefix_hits,
      std::vector<SBFullHashResult>* cache_hits);

  bool PrefixSetContainsUrlHashes(const std::vector<SBFullHash>& full_hashes,
                                  PrefixSetId prefix_set_id,
                                  std::vector<SBPrefix>* prefix_hits,
                                  std::vector<SBFullHashResult>* cache_hits);

  // Returns true if the whitelist is disabled or if any of the given hashes
  // matches the whitelist.
  bool ContainsWhitelistedHashes(SBWhitelistId whitelist_id,
                                 const std::vector<SBFullHash>& hashes);

  // Return the store matching |list_id|.
  SafeBrowsingStore* GetStore(int list_id);

  // Deletes the files on disk.
  bool Delete();

  // Load the prefix set in "|db_filename| Prefix Set" off disk, if available,
  // and stores it in the PrefixSet identified by |prefix_set_id|.
  // |read_failure_type| provides a caller-specific error code to be used on
  // failure.  This method should only ever be called during initialization as
  // it performs some disk IO while holding a transaction (for the sake of
  // avoiding uncessary back-and-forth interactions with the lock during
  // Init()).
  void LoadPrefixSet(const base::FilePath& db_filename,
                     ThreadSafeStateManager::WriteTransaction* txn,
                     PrefixSetId prefix_set_id,
                     FailureType read_failure_type);

  // Writes the current prefix set "|db_filename| Prefix Set" on disk.
  // |write_failure_type| provides a caller-specific error code to be used on
  // failure.
  void WritePrefixSet(const base::FilePath& db_filename,
                      PrefixSetId prefix_set_id,
                      FailureType write_failure_type);

  // Loads the given full-length hashes to the given whitelist.  If the number
  // of hashes is too large or if the kill switch URL is on the whitelist
  // we will whitelist everything.
  void LoadWhitelist(const std::vector<SBAddFullHash>& full_hashes,
                     SBWhitelistId whitelist_id);

  // Parses the IP blacklist from the given full-length hashes.
  void LoadIpBlacklist(const std::vector<SBAddFullHash>& full_hashes);

  // Helpers for handling database corruption.
  // |OnHandleCorruptDatabase()| runs |ResetDatabase()| and sets
  // |corruption_detected_|, |HandleCorruptDatabase()| posts
  // |OnHandleCorruptDatabase()| to the current thread, to be run
  // after the current task completes.
  // TODO(shess): Wire things up to entirely abort the update
  // transaction when this happens.
  void HandleCorruptDatabase();
  void OnHandleCorruptDatabase();

  // Helpers for InsertChunks().
  void InsertAddChunk(SafeBrowsingStore* store,
                      safe_browsing_util::ListType list_id,
                      const SBChunkData& chunk);
  void InsertSubChunk(SafeBrowsingStore* store,
                      safe_browsing_util::ListType list_id,
                      const SBChunkData& chunk);

  // Updates the |store| and stores the result on disk under |store_filename|.
  void UpdateHashPrefixStore(const base::FilePath& store_filename,
                             SafeBrowsingStore* store,
                             FailureType failure_type);

  // Updates a PrefixStore store for URLs (|url_store|) which is backed on disk
  // by a "|db_filename| Prefix Set" file. Specific failure types are provided
  // to highlight the specific store who made the initial request on failure.
  // |store_full_hashes_in_prefix_set| dictates whether full_hashes from the
  // |url_store| should be cached in the |prefix_set| as well.
  void UpdatePrefixSetUrlStore(const base::FilePath& db_filename,
                               SafeBrowsingStore* url_store,
                               PrefixSetId prefix_set_id,
                               FailureType finish_failure_type,
                               FailureType write_failure_type,
                               bool store_full_hashes_in_prefix_set);

  void UpdateUrlStore(SafeBrowsingStore* url_store,
                      PrefixSetId prefix_set_id,
                      FailureType failure_type);

  void UpdateWhitelistStore(const base::FilePath& store_filename,
                            SafeBrowsingStore* store,
                            SBWhitelistId whitelist_id);
  void UpdateIpBlacklistStore();

  // Returns a raw pointer to ThreadSafeStateManager's PrefixGetHashCache for
  // testing. This should only be used in unit tests (where multi-threading and
  // synchronization are not problematic).
  PrefixGetHashCache* GetUnsynchronizedPrefixGetHashCacheForTesting();

  // Records a file size histogram for the database or PrefixSet backed by
  // |filename|.
  void RecordFileSizeHistogram(const base::FilePath& file_path);

  base::ThreadChecker thread_checker_;

  ThreadSafeStateManager state_manager_;

  DatabaseStateManager db_state_manager_;

  // Underlying persistent stores for chunk data:
  //   - |browse_store_|: For browsing related (phishing and malware URLs)
  //     chunks and prefixes.
  //   - |download_store_|: For download related (download URL and binary hash)
  //     chunks and prefixes.
  //   - |csd_whitelist_store_|: For the client-side phishing detection
  //     whitelist chunks and full-length hashes.  This list only contains 256
  //     bit hashes.
  //   - |download_whitelist_store_|: For the download whitelist chunks and
  //     full-length hashes.  This list only contains 256 bit hashes.
  //   - |inclusion_whitelist_store_|: For the inclusion whitelist. Same format
  //     as |download_whitelist_store_|.
  //   - |extension_blacklist_store_|: For extension IDs.
  //   - |side_effect_free_whitelist_store_|: For side-effect free whitelist.
  //   - |ip_blacklist_store_|: For IP blacklist.
  //   - |unwanted_software_store_|: For unwanted software list (format
  //     identical to browsing lists).
  //
  // The stores themselves will be modified throughout the existence of this
  // database, but shouldn't ever be swapped out (hence the const scoped_ptr --
  // which could be swapped for C++11's std::optional when that's available).
  // They are NonThreadSafe and should thus only be accessed on the database's
  // main thread as enforced by SafeBrowsingStoreFile's implementation.
  const scoped_ptr<SafeBrowsingStore> browse_store_;
  const scoped_ptr<SafeBrowsingStore> download_store_;
  const scoped_ptr<SafeBrowsingStore> csd_whitelist_store_;
  const scoped_ptr<SafeBrowsingStore> download_whitelist_store_;
  const scoped_ptr<SafeBrowsingStore> inclusion_whitelist_store_;
  const scoped_ptr<SafeBrowsingStore> extension_blacklist_store_;
  const scoped_ptr<SafeBrowsingStore> side_effect_free_whitelist_store_;
  const scoped_ptr<SafeBrowsingStore> ip_blacklist_store_;
  const scoped_ptr<SafeBrowsingStore> unwanted_software_store_;

  // Used to schedule resetting the database because of corruption. This factory
  // and the WeakPtrs it issues should only be used on the database's main
  // thread.
  base::WeakPtrFactory<SafeBrowsingDatabaseNew> reset_factory_;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_