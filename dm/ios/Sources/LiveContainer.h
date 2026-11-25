#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

BOOL dm_is_live_container(void);

void dm_live_container_warn(UIViewController *presenter, void (^proceed)(void));

NS_ASSUME_NONNULL_END
