#import <UIKit/UIKit.h>
#include "front.h"

UIView *front_ios_overlay_root(void);

static NSString *const kModeTitles[] = {@"Auto", @"Left", @"Center", @"Right", @"Stretch", @"Expand"};
static const float kSteps[] = {1.0f, 4.0f, 16.0f};

@interface DmNudgeButton : UIButton
@property (nonatomic, copy) void (^fire)(int ticks);
@property (nonatomic, strong) NSTimer *timer;
@property (nonatomic) int ticks;
@end

@implementation DmNudgeButton
- (instancetype)initWithFrame:(CGRect)f {
    if ((self = [super initWithFrame:f])) {
        [self addTarget:self action:@selector(pressed) forControlEvents:UIControlEventTouchDown];
        [self addTarget:self action:@selector(released) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel];
    }
    return self;
}
- (void)pressed {
    self.ticks = 0;
    if (self.fire) self.fire(0);
    __weak DmNudgeButton *weak = self;
    self.timer = [NSTimer scheduledTimerWithTimeInterval:0.06 repeats:YES block:^(NSTimer *t) {
        DmNudgeButton *s = weak;
        if (!s) { [t invalidate]; return; }
        s.ticks++;
        if (s.ticks >= 6 && s.fire) s.fire(s.ticks);
    }];
}
- (void)released {
    [self.timer invalidate];
    self.timer = nil;
}
@end

@interface DmLayoutCatcher : UIView
@end

@implementation DmLayoutCatcher
@end

@interface DmLayoutEditor : NSObject
@property (nonatomic, strong) UIView *root;
@property (nonatomic, strong) UIView *panel;
@property (nonatomic, strong) UIView *catcher;
@property (nonatomic, strong) UIButton *pill;
@property (nonatomic, strong) UILabel *keyLabel;
@property (nonatomic, strong) UILabel *infoLabel;
@property (nonatomic, strong) UISegmentedControl *modes;
@property (nonatomic, strong) UISegmentedControl *steps;
@property (nonatomic, strong) UISwitch *select;
@property (nonatomic, strong) NSArray<UIControl *> *needsSelection;
@property (nonatomic) BOOL hasSelection;
+ (instancetype)shared;
- (void)show;
- (void)updateKey:(NSString *)key mode:(int)mode index:(int)index total:(int)total dx:(float)dx dy:(float)dy placements:(int)placements;
@end

static UIButton *panel_button(NSString *title, CGRect frame, id target, SEL action) {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    b.frame = frame;
    [b setTitle:title forState:UIControlStateNormal];
    b.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightSemibold];
    b.backgroundColor = [UIColor colorWithWhite:1 alpha:0.10];
    b.layer.cornerRadius = 8;
    [b setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    [b setTitleColor:[UIColor colorWithWhite:1 alpha:0.3] forState:UIControlStateDisabled];
    [b addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];
    return b;
}

@implementation DmLayoutEditor

+ (instancetype)shared {
    static DmLayoutEditor *e;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ e = [DmLayoutEditor new]; });
    return e;
}

- (void)show {
    UIView *root = front_ios_overlay_root();
    if (!root) return;
    if (self.root) return;
    self.root = root;
    UIEdgeInsets ins = root.safeAreaInsets;

    self.pill = [UIButton buttonWithType:UIButtonTypeSystem];
    self.pill.frame = CGRectMake(ins.left + 12, ins.top + 8, 74, 30);
    self.pill.autoresizingMask = UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin;
    self.pill.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.85];
    self.pill.layer.cornerRadius = 15;
    self.pill.layer.borderWidth = 1;
    self.pill.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.2].CGColor;
    self.pill.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightSemibold];
    [self.pill setTitle:@"Layout" forState:UIControlStateNormal];
    [self.pill setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    [self.pill addTarget:self action:@selector(openPanel) forControlEvents:UIControlEventTouchUpInside];
    [root addSubview:self.pill];

    CGFloat w = 340, h = 392;
    self.panel = [[UIView alloc] initWithFrame:CGRectMake(MAX(root.bounds.size.width - ins.right - w - 12, 8), ins.top + 8, w, h)];
    self.panel.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleBottomMargin;
    self.panel.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.94];
    self.panel.layer.cornerRadius = 14;
    self.panel.layer.borderWidth = 1;
    self.panel.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.14].CGColor;
    self.panel.hidden = YES;

    UIView *header = [[UIView alloc] initWithFrame:CGRectMake(0, 0, w, 40)];
    header.backgroundColor = UIColor.clearColor;
    [header addGestureRecognizer:[[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(dragged:)]];
    UIView *grab = [[UIView alloc] initWithFrame:CGRectMake((w - 36) / 2, 7, 36, 4)];
    grab.backgroundColor = [UIColor colorWithWhite:1 alpha:0.28];
    grab.layer.cornerRadius = 2;
    [header addSubview:grab];
    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(14, 14, 200, 22)];
    title.text = @"Fullscreen layout";
    title.textColor = UIColor.whiteColor;
    title.font = [UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];
    [header addSubview:title];
    UIButton *closeButton = panel_button(@"\u2715", CGRectMake(w - 46, 8, 34, 30), self, @selector(closePanel));
    [header addSubview:closeButton];
    [self.panel addSubview:header];

    UILabel *selLabel = [[UILabel alloc] initWithFrame:CGRectMake(14, 48, 120, 31)];
    selLabel.text = @"Select";
    selLabel.textColor = UIColor.whiteColor;
    selLabel.font = [UIFont systemFontOfSize:14 weight:UIFontWeightMedium];
    [self.panel addSubview:selLabel];
    self.select = [[UISwitch alloc] init];
    self.select.frame = CGRectMake(w - 14 - 51, 48, 51, 31);
    [self.select addTarget:self action:@selector(selectToggled) forControlEvents:UIControlEventValueChanged];
    [self.panel addSubview:self.select];

    self.keyLabel = [[UILabel alloc] initWithFrame:CGRectMake(14, 84, w - 28, 48)];
    self.keyLabel.numberOfLines = 3;
    self.keyLabel.lineBreakMode = NSLineBreakByTruncatingHead;
    self.keyLabel.textColor = UIColor.whiteColor;
    self.keyLabel.font = [UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightRegular];
    [self.panel addSubview:self.keyLabel];
    self.infoLabel = [[UILabel alloc] initWithFrame:CGRectMake(14, 134, w - 28, 18)];
    self.infoLabel.textColor = [UIColor colorWithWhite:1 alpha:0.6];
    self.infoLabel.font = [UIFont monospacedDigitSystemFontOfSize:12 weight:UIFontWeightRegular];
    [self.panel addSubview:self.infoLabel];

    self.modes = [[UISegmentedControl alloc] initWithItems:@[kModeTitles[0], kModeTitles[1], kModeTitles[2], kModeTitles[3], kModeTitles[4], kModeTitles[5]]];
    self.modes.frame = CGRectMake(14, 160, w - 28, 32);
    self.modes.apportionsSegmentWidthsByContent = NO;
    [self.modes setTitleTextAttributes:@{NSFontAttributeName: [UIFont systemFontOfSize:11 weight:UIFontWeightSemibold]} forState:UIControlStateNormal];
    [self.modes addTarget:self action:@selector(modeChanged) forControlEvents:UIControlEventValueChanged];
    [self.panel addSubview:self.modes];

    UIButton *smaller = panel_button(@"\u2193 Smaller", CGRectMake(14, 202, (w - 36) / 2, 34), self, @selector(smaller));
    UIButton *larger = panel_button(@"\u2191 Larger", CGRectMake(22 + (w - 36) / 2, 202, (w - 36) / 2, 34), self, @selector(larger));
    [self.panel addSubview:smaller];
    [self.panel addSubview:larger];

    CGFloat cx = 104, cy = 288, bs = 44;
    __weak DmLayoutEditor *weak = self;
    NSArray *dirs = @[@[@"\u2190", @(-1), @0, @(cx - bs - 4), @(cy)], @[@"\u2192", @1, @0, @(cx + bs + 4), @(cy)], @[@"\u2191", @0, @1, @(cx), @(cy - bs - 4)], @[@"\u2193", @0, @(-1), @(cx), @(cy + bs + 4)]];
    NSMutableArray *nudges = [NSMutableArray array];
    for (NSArray *d in dirs) {
        DmNudgeButton *b = [[DmNudgeButton alloc] initWithFrame:CGRectMake([d[3] doubleValue] - bs / 2, [d[4] doubleValue] - bs / 2, bs, bs)];
        b.backgroundColor = [UIColor colorWithWhite:1 alpha:0.10];
        b.layer.cornerRadius = 8;
        b.titleLabel.font = [UIFont systemFontOfSize:20 weight:UIFontWeightSemibold];
        [b setTitle:d[0] forState:UIControlStateNormal];
        [b setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
        [b setTitleColor:[UIColor colorWithWhite:1 alpha:0.3] forState:UIControlStateDisabled];
        float sx = [d[1] floatValue], sy = [d[2] floatValue];
        b.fire = ^(int ticks) {
            DmLayoutEditor *s = weak;
            if (!s || !s.hasSelection) return;
            float step = kSteps[MAX(s.steps.selectedSegmentIndex, 0)] * (ticks > 40 ? 4.0f : 1.0f);
            dm_layout_cmd(DM_LAYOUT_NUDGE, sx * step, sy * step);
        };
        [self.panel addSubview:b];
        [nudges addObject:b];
    }

    UILabel *stepLabel = [[UILabel alloc] initWithFrame:CGRectMake(196, 254, 130, 18)];
    stepLabel.text = @"Nudge step";
    stepLabel.textColor = [UIColor colorWithWhite:1 alpha:0.6];
    stepLabel.font = [UIFont systemFontOfSize:12];
    [self.panel addSubview:stepLabel];
    self.steps = [[UISegmentedControl alloc] initWithItems:@[@"1", @"4", @"16"]];
    self.steps.frame = CGRectMake(196, 276, w - 196 - 14, 32);
    self.steps.selectedSegmentIndex = 0;
    [self.panel addSubview:self.steps];

    UIButton *reset = panel_button(@"Reset", CGRectMake(14, 344, 96, 34), self, @selector(resetTapped));
    UIButton *deselect = panel_button(@"Deselect", CGRectMake(122, 344, 96, 34), self, @selector(deselectTapped));
    UIButton *save = panel_button(@"Save", CGRectMake(230, 344, w - 230 - 14, 34), self, @selector(saveTapped));
    save.backgroundColor = [UIColor colorWithRed:0.2 green:0.5 blue:1.0 alpha:0.6];
    [self.panel addSubview:reset];
    [self.panel addSubview:deselect];
    [self.panel addSubview:save];

    NSMutableArray *needs = [NSMutableArray arrayWithObjects:self.modes, smaller, larger, reset, deselect, nil];
    [needs addObjectsFromArray:nudges];
    self.needsSelection = needs;
    [root addSubview:self.panel];
    [self updateKey:@"" mode:0 index:0 total:0 dx:0 dy:0 placements:0];
}

- (void)openPanel {
    self.panel.hidden = NO;
    self.pill.hidden = YES;
    [self.root bringSubviewToFront:self.panel];
}

- (void)closePanel {
    self.select.on = NO;
    [self selectToggled];
    self.panel.hidden = YES;
    self.pill.hidden = NO;
}

- (void)dragged:(UIPanGestureRecognizer *)pan {
    CGPoint t = [pan translationInView:self.root];
    [pan setTranslation:CGPointZero inView:self.root];
    CGRect b = self.root.bounds;
    CGPoint c = self.panel.center;
    CGSize s = self.panel.bounds.size;
    c.x = MIN(MAX(c.x + t.x, 60 - s.width / 2), b.size.width + s.width / 2 - 60);
    c.y = MIN(MAX(c.y + t.y, s.height / 2), b.size.height - 40 + s.height / 2);
    self.panel.center = c;
}

- (void)selectToggled {
    if (self.select.on) {
        if (!self.catcher) {
            DmLayoutCatcher *c = [[DmLayoutCatcher alloc] initWithFrame:self.root.bounds];
            c.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
            c.backgroundColor = UIColor.clearColor;
            [c addGestureRecognizer:[[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(tapped:)]];
            self.catcher = c;
        }
        [self.root insertSubview:self.catcher belowSubview:self.pill];
        if (!self.panel.hidden) [self.root bringSubviewToFront:self.panel];
    } else {
        [self.catcher removeFromSuperview];
        dm_layout_cmd(DM_LAYOUT_DESELECT, 0, 0);
    }
}

- (void)tapped:(UITapGestureRecognizer *)tap {
    UIView *v = tap.view;
    CGPoint p = [tap locationInView:v];
    CGFloat w = v.bounds.size.width, h = v.bounds.size.height;
    dm_fit_picture(&w, &h);
    CGFloat x0 = (v.bounds.size.width - w) / 2, y0 = (v.bounds.size.height - h) / 2;
    float nx = (float)fmin(fmax((p.x - x0) / w, 0), 0.999), ny = (float)fmin(fmax((p.y - y0) / h, 0), 0.999);
    dm_layout_pick(nx, ny);
}

- (void)modeChanged { if (self.hasSelection) dm_layout_cmd(DM_LAYOUT_MODE, (float)self.modes.selectedSegmentIndex, 0); }
- (void)smaller { dm_layout_cmd(DM_LAYOUT_STEP, -1, 0); }
- (void)larger { dm_layout_cmd(DM_LAYOUT_STEP, 1, 0); }
- (void)resetTapped { dm_layout_cmd(DM_LAYOUT_RESET, 0, 0); }
- (void)deselectTapped { dm_layout_cmd(DM_LAYOUT_DESELECT, 0, 0); }
- (void)saveTapped { dm_layout_cmd(DM_LAYOUT_SAVE, 0, 0); }

- (void)updateKey:(NSString *)key mode:(int)mode index:(int)index total:(int)total dx:(float)dx dy:(float)dy placements:(int)placements {
    self.hasSelection = key.length > 0;
    for (UIControl *c in self.needsSelection) c.enabled = self.hasSelection;
    if (self.hasSelection) {
        self.keyLabel.text = [key stringByReplacingOccurrencesOfString:@"/" withString:@" / "];
        self.infoLabel.text = [NSString stringWithFormat:@"%d of %d   dx %.0f   dy %.0f   saved set: %d", index, total, dx, dy, placements];
        if (mode >= 0 && mode < 6) self.modes.selectedSegmentIndex = mode;
    } else {
        self.keyLabel.text = self.select.on ? @"Tap an element on the screen" : @"Turn Select on, then tap an element";
        self.infoLabel.text = [NSString stringWithFormat:@"placements: %d", placements];
        self.modes.selectedSegmentIndex = UISegmentedControlNoSegment;
    }
}

@end

void front_layout_panel(int show) {
    if (!show) return;
    dispatch_async(dispatch_get_main_queue(), ^{ [[DmLayoutEditor shared] show]; });
}

void front_layout_state(const char *key, int mode, int index, int total, float dx, float dy, int placements) {
    NSString *k = key ? @(key) : @"";
    dispatch_async(dispatch_get_main_queue(), ^{ [[DmLayoutEditor shared] updateKey:k mode:mode index:index total:total dx:dx dy:dy placements:placements]; });
}
