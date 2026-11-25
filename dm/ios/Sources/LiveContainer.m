#import "LiveContainer.h"
#include <stdlib.h>

static NSString *const kSuppressedKey = @"DMLiveContainerWarningSuppressed";

BOOL dm_is_live_container(void) {
    static BOOL found;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        found = [NSFileManager.defaultManager fileExistsAtPath:[NSHomeDirectory() stringByAppendingPathComponent:@"LCContainerInfo.plist"]];
    });
    return found;
}

@interface DmLiveContainerVC : UIViewController
@property (nonatomic, copy) void (^proceed)(void);
@property (nonatomic, strong) UISwitch *remember;
@end

@implementation DmLiveContainerVC

- (UIButton *)buttonWithTitle:(NSString *)title color:(UIColor *)color weight:(UIFontWeight)weight action:(SEL)action {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    [b setTitle:title forState:UIControlStateNormal];
    [b setTitleColor:color forState:UIControlStateNormal];
    b.titleLabel.font = [UIFont systemFontOfSize:17 weight:weight];
    [b addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    [b.heightAnchor constraintEqualToConstant:46].active = YES;
    return b;
}

- (UIView *)separator {
    UIView *v = [UIView new];
    v.backgroundColor = UIColor.separatorColor;
    [v.heightAnchor constraintEqualToConstant:1.0 / UIScreen.mainScreen.scale].active = YES;
    return v;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithWhite:0 alpha:0.4];

    UILabel *title = [UILabel new];
    title.text = @"LiveContainer installation";
    title.font = [UIFont systemFontOfSize:17 weight:UIFontWeightSemibold];
    title.textAlignment = NSTextAlignmentCenter;
    title.numberOfLines = 0;
    title.textColor = UIColor.labelColor;

    UILabel *message = [UILabel new];
    message.text = @"It looks like you’re on LiveContainer, the file picker won’t work as-is, to fix this, go to this app’s settings on LiveContainer, and toggle “Fix file picker”";
    message.font = [UIFont systemFontOfSize:13];
    message.textAlignment = NSTextAlignmentCenter;
    message.numberOfLines = 0;
    message.textColor = UIColor.labelColor;

    UILabel *rememberLabel = [UILabel new];
    rememberLabel.text = @"Don’t show again";
    rememberLabel.font = [UIFont systemFontOfSize:15];
    rememberLabel.textColor = UIColor.labelColor;
    self.remember = [UISwitch new];
    UIStackView *rememberRow = [[UIStackView alloc] initWithArrangedSubviews:@[rememberLabel, self.remember]];
    rememberRow.axis = UILayoutConstraintAxisHorizontal;
    rememberRow.alignment = UIStackViewAlignmentCenter;
    rememberRow.distribution = UIStackViewDistributionEqualSpacing;

    UIStackView *text = [[UIStackView alloc] initWithArrangedSubviews:@[title, message, rememberRow]];
    text.axis = UILayoutConstraintAxisVertical;
    text.spacing = 10;
    text.layoutMarginsRelativeArrangement = YES;
    text.layoutMargins = UIEdgeInsetsMake(20, 20, 16, 20);

    UIButton *close = [self buttonWithTitle:@"Close App" color:UIColor.systemRedColor weight:UIFontWeightRegular action:@selector(closeApp)];
    UIButton *dismiss = [self buttonWithTitle:@"Dismiss" color:self.view.tintColor weight:UIFontWeightSemibold action:@selector(dismissTapped)];

    UIStackView *column = [[UIStackView alloc] initWithArrangedSubviews:@[text, [self separator], close, [self separator], dismiss]];
    column.axis = UILayoutConstraintAxisVertical;
    column.translatesAutoresizingMaskIntoConstraints = NO;

    UIVisualEffectView *card = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial]];
    card.translatesAutoresizingMaskIntoConstraints = NO;
    card.layer.cornerRadius = 14;
    card.clipsToBounds = YES;
    [card.contentView addSubview:column];
    [self.view addSubview:card];

    NSLayoutConstraint *width = [card.widthAnchor constraintEqualToConstant:340];
    width.priority = UILayoutPriorityDefaultHigh;
    [NSLayoutConstraint activateConstraints:@[
        [column.topAnchor constraintEqualToAnchor:card.contentView.topAnchor],
        [column.bottomAnchor constraintEqualToAnchor:card.contentView.bottomAnchor],
        [column.leadingAnchor constraintEqualToAnchor:card.contentView.leadingAnchor],
        [column.trailingAnchor constraintEqualToAnchor:card.contentView.trailingAnchor],
        [card.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        [card.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
        [card.leadingAnchor constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:24],
        [card.trailingAnchor constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-24],
        width,
    ]];
}

- (void)saveChoice {
    if (self.remember.isOn) [NSUserDefaults.standardUserDefaults setBool:YES forKey:kSuppressedKey];
}

- (void)closeApp {
    [self saveChoice];
    exit(0);
}

- (void)dismissTapped {
    [self saveChoice];
    void (^proceed)(void) = self.proceed;
    [self dismissViewControllerAnimated:YES completion:^{ if (proceed) proceed(); }];
}

@end

void dm_live_container_warn(UIViewController *presenter, void (^proceed)(void)) {
    if (!dm_is_live_container() || [NSUserDefaults.standardUserDefaults boolForKey:kSuppressedKey]) {
        proceed();
        return;
    }
    if ([presenter.presentedViewController isKindOfClass:DmLiveContainerVC.class]) return;
    DmLiveContainerVC *vc = [DmLiveContainerVC new];
    vc.proceed = proceed;
    vc.modalPresentationStyle = UIModalPresentationOverFullScreen;
    vc.modalTransitionStyle = UIModalTransitionStyleCrossDissolve;
    [presenter presentViewController:vc animated:YES completion:nil];
}
