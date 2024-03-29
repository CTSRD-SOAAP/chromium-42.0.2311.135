// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!FileSystemMetadata} fileSystemMetadata
 * @struct
 * @constructor
 */
function ThumbnailModel(fileSystemMetadata) {
  /**
   * @private {!FileSystemMetadata}
   * @const
   */
  this.fileSystemMetadata_ = fileSystemMetadata;
}

/**
 * @param {!Array<!Entry>} entries
 * @return {Promise} Promise fulfilled with old format metadata list.
 */
ThumbnailModel.prototype.get = function(entries) {
  var results = {};
  return this.fileSystemMetadata_.get(
      entries,
      [
        'modificationTime',
        'customIconUrl',
        'thumbnailUrl',
        'present'
      ]).then(function(metadataList) {
        var contentRequestEntries = [];
        for (var i = 0; i < entries.length; i++) {
          var url = entries[i].toURL();
          // TODO(hirono): Use the provider results directly after removing code
          // using old metadata format.
          results[url] = {
            filesystem: {
              modificationTime: metadataList[i].modificationTime
            },
            external: {
              thumbnailUrl: metadataList[i].thumbnailUrl,
              customIconUrl: metadataList[i].customIconUrl,
              present: metadataList[i].present
            },
            thumbnail: {},
            media: {}
          };
          var canUseContentThumbnail =
              metadataList[i].present && FileType.isImage(entries[i]);
          if (canUseContentThumbnail)
            contentRequestEntries.push(entries[i]);
        }
        if (contentRequestEntries.length) {
          return this.fileSystemMetadata_.get(
              contentRequestEntries,
              [
                'contentThumbnailUrl',
                'contentThumbnailTransform',
                'contentImageTransform'
              ]).then(function(contentMetadataList) {
                for (var i = 0; i < contentRequestEntries.length; i++) {
                  var url = contentRequestEntries[i].toURL();
                  results[url].thumbnail.url =
                      contentMetadataList[i].contentThumbnailUrl;
                  results[url].thumbnail.transform =
                      contentMetadataList[i].contentThumbnailTransform;
                  results[url].media.imageTransform =
                      contentMetadataList[i].contentImageTransform;
                }
              });
        }
      }.bind(this)).then(function() {
        return entries.map(function(entry) { return results[entry.toURL()]; });
      });
};
