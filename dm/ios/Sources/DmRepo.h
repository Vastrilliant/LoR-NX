// The GitHub repository this app was built from, resolved at build time: dm/ios/tools/build_ipa.sh writes it into Info.plist as DMRepository ("owner/name"; taken from DM_REPOSITORY,
// else GITHUB_REPOSITORY (set by GitHub Actions, so a fork's own builds point at the fork), else the checkout's origin remote). Used for the shader download (ShaderDownload.m) and the update check (UpdateCheck.m).
#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN
NSString *dm_repo_slug(void);                    // "owner/name"; the upstream repository when an older Info.plist has no valid DMRepository
NSURL *dm_repo_url(NSString *path);              // https://github.com/<owner>/<name>/<path>
NSURL *dm_repo_api_url(NSString *path);          // https://api.github.com/repos/<owner>/<name>/<path>
NS_ASSUME_NONNULL_END
