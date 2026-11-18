// Update check: asks the repository the app was built from (DmRepo.h) for its latest release and tells the caller when that release is newer than this build (UpdateCheck.m). Used by front_ios.m.
#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN
// latest / current are display strings ("v1.2.0", "v1.2.0 (build 45)"); page is the release page. Called once on the main queue, only when the latest release is newer and the player has not skipped it
typedef void (^DmUpdateFound)(NSString *tag, NSString *latest, NSString *current, NSURL *page);
void dm_update_check(DmUpdateFound found);   // silent when offline, rate limited, without a release or up to date
void dm_update_skip(NSString *tag);          // "Skip This Version": that release is not offered again; a newer one is
NS_ASSUME_NONNULL_END
