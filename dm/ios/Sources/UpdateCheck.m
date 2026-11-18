// Update check. The release workflow (.github/workflows/build.yml) tags a release nxnative-v<VERSION>-<run number>; the app knows VERSION (NXVersion) and the run number (DMBuildNumber)
// from Info.plist, so the latest release is newer when its version is higher, or the same version with a higher build number. A build without a run number (a local one) never counts
// an equal version as outdated. Pre-release suffixes of a version are ignored in the comparison. The repository comes from DmRepo.h, so a fork checks its own releases.
// GitHub's "latest" is the newest published release that is not a draft or a pre-release, which is also the one the shader download uses.
#import "UpdateCheck.h"
#import "DmRepo.h"
#include <stdio.h>

static NSString *const kSkippedKey = @"DMSkippedUpdateTag";

// "nxnative-v1.2.3-45", "v1.2.3", "1.2.3-beta.1-45": core[3] = 1.2.3; build = 45, or -1 when the tag has no trailing -<digits>
static BOOL parse_version(NSString *s, unsigned long core[3], long *build) {
    if ([s.lowercaseString hasPrefix:@"nxnative-"]) s = [s substringFromIndex:9];
    if ([s hasPrefix:@"v"] || [s hasPrefix:@"V"]) s = [s substringFromIndex:1];
    int n = 0;
    if (sscanf(s.UTF8String, "%lu.%lu.%lu%n", &core[0], &core[1], &core[2], &n) != 3 || n <= 0) return NO;
    *build = -1;
    NSString *rest = [s substringFromIndex:(NSUInteger)n];
    NSRange dash = [rest rangeOfString:@"-" options:NSBackwardsSearch];
    if (dash.location != NSNotFound) {
        NSString *tail = [rest substringFromIndex:dash.location + 1];
        if (tail.length && [tail rangeOfCharacterFromSet:NSCharacterSet.decimalDigitCharacterSet.invertedSet].location == NSNotFound) *build = tail.integerValue;
    }
    return YES;
}

static NSString *display(const unsigned long core[3], long build, BOOL withBuild) {
    NSString *s = [NSString stringWithFormat:@"v%lu.%lu.%lu", core[0], core[1], core[2]];
    return withBuild && build >= 0 ? [s stringByAppendingFormat:@" (build %ld)", build] : s;
}

void dm_update_skip(NSString *tag) { [NSUserDefaults.standardUserDefaults setObject:tag forKey:kSkippedKey]; }

void dm_update_check(DmUpdateFound found) {
    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    NSString *cur = [info[@"NXVersion"] isKindOfClass:NSString.class] ? info[@"NXVersion"] : nil;
    NSString *curBuild = [info[@"DMBuildNumber"] isKindOfClass:NSString.class] ? info[@"DMBuildNumber"] : nil;
    unsigned long have[3]; long haveBuild = -1;
    if (!cur || !parse_version(cur, have, &haveBuild)) { fprintf(stderr, "boot: update check skipped (no NXVersion in Info.plist)\n"); return; }
    if (curBuild.length) haveBuild = curBuild.integerValue;
    const unsigned long h0 = have[0], h1 = have[1], h2 = have[2]; const long hb = haveBuild;   // scalars: the completion block copies these, not the array
    NSMutableURLRequest *rq = [NSMutableURLRequest requestWithURL:dm_repo_api_url(@"releases/latest") cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:10];
    [rq setValue:@"application/vnd.github+json" forHTTPHeaderField:@"Accept"];
    [rq setValue:@"LoR-NX-update-check" forHTTPHeaderField:@"User-Agent"];
    fprintf(stderr, "boot: checking %s for a newer release (this build: %s%s%s)\n", dm_repo_slug().UTF8String, cur.UTF8String, curBuild.length ? " build " : "", curBuild.length ? curBuild.UTF8String : "");
    [[NSURLSession.sharedSession dataTaskWithRequest:rq completionHandler:^(NSData *data, NSURLResponse *resp, NSError *err) {
        NSInteger code = [resp isKindOfClass:NSHTTPURLResponse.class] ? ((NSHTTPURLResponse *)resp).statusCode : 0;
        if (err || code != 200 || !data) { fprintf(stderr, "boot: update check: no answer (%s)\n", err ? err.localizedDescription.UTF8String : [NSString stringWithFormat:@"HTTP %ld", (long)code].UTF8String); return; }
        id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        NSString *tag = [json isKindOfClass:NSDictionary.class] && [json[@"tag_name"] isKindOfClass:NSString.class] ? json[@"tag_name"] : nil;
        unsigned long latest[3], now[3] = {h0, h1, h2}; long latestBuild = -1;
        if (!tag || !parse_version(tag, latest, &latestBuild)) { fprintf(stderr, "boot: update check: the latest release's tag is not a version\n"); return; }
        int c = 0;
        for (int i = 0; i < 3 && !c; i++) c = latest[i] > now[i] ? 1 : latest[i] < now[i] ? -1 : 0;
        BOOL newer = c > 0 || (c == 0 && hb >= 0 && latestBuild > hb);
        fprintf(stderr, "boot: latest release %s: %s\n", tag.UTF8String, newer ? "newer than this build" : "not newer");
        if (!newer || [[NSUserDefaults.standardUserDefaults stringForKey:kSkippedKey] isEqualToString:tag]) return;
        BOOL sameCore = c == 0;   // only then does the build number tell the two apart
        NSString *l = display(latest, latestBuild, sameCore), *h = display(now, hb, sameCore);
        dispatch_async(dispatch_get_main_queue(), ^{ found(tag, l, h, dm_repo_url(@"releases/latest")); });
    }] resume];
}
