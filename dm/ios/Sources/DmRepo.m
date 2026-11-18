// Repository the app was built from (see DmRepo.h). The value is validated as owner/name so a malformed Info.plist entry can never reshape a URL.
#import "DmRepo.h"

static NSString *const kDefaultRepo = @"Vastrilliant/LoR-NX";   // only when Info.plist has no valid DMRepository (an IPA from before this key existed)

static BOOL valid_part(NSString *s) {
    if (!s.length) return NO;
    NSCharacterSet *ok = [NSCharacterSet characterSetWithCharactersInString:@"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-"];
    return [s rangeOfCharacterFromSet:ok.invertedSet].location == NSNotFound && ![s isEqualToString:@"."] && ![s isEqualToString:@".."];
}

NSString *dm_repo_slug(void) {
    static NSString *slug;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        id v = [NSBundle.mainBundle objectForInfoDictionaryKey:@"DMRepository"];
        NSArray<NSString *> *p = [v isKindOfClass:NSString.class] ? [(NSString *)v componentsSeparatedByString:@"/"] : nil;
        slug = (p.count == 2 && valid_part(p[0]) && valid_part(p[1])) ? (NSString *)v : kDefaultRepo;
    });
    return slug;
}

NSURL *dm_repo_url(NSString *path) {
    return [NSURL URLWithString:[NSString stringWithFormat:@"https://github.com/%@/%@", dm_repo_slug(), path]];
}

NSURL *dm_repo_api_url(NSString *path) {
    return [NSURL URLWithString:[NSString stringWithFormat:@"https://api.github.com/repos/%@/%@", dm_repo_slug(), path]];
}
