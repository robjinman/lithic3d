#include "application.hpp"
#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <GameController/GameController.h>

using lithic3d::GamepadButton;

namespace lithic3d
{

WindowDelegatePtr createWindowDelegate(CAMetalLayer* metalLayer);

} // namespace lithic3d

// -- BEGIN EventRelay------------------------------------------------------------------------------
@interface EventRelay : NSObject
@property (nonatomic, copy, nullable) void(^onTouchBegin)(float x, float y);
@property (nonatomic, copy, nullable) void(^onTouchMove)(float x, float y);
@property (nonatomic, copy, nullable) void(^onTouchEnd)(float x, float y);
@end

@implementation EventRelay
@end
// -- END EventRelay--------------------------------------------------------------------------------

// -- BEGIN MetalView ------------------------------------------------------------------------------
@interface MetalView : UIView
- (instancetype)initWithFrame:(CGRect)frame andEventRelay:(EventRelay*)eventRelay;

@property (strong) EventRelay* eventRelay;
@end

@implementation MetalView
+ (Class)layerClass
{
  return [CAMetalLayer class];
}

+ (CGPoint)toPixels:(CGPoint)pt
{
  CGFloat scale = [UIScreen mainScreen].scale;
  return CGPointMake(pt.x * scale, pt.y * scale);
}

- (instancetype)initWithFrame:(CGRect)frame andEventRelay:(EventRelay*)eventRelay
{
  self = [super initWithFrame:frame];

  if (self) {
    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    metalLayer.device = MTLCreateSystemDefaultDevice();
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.framebufferOnly = YES;

    self.eventRelay = eventRelay;
  }

  return self;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  UITouch* touch = [touches anyObject];
  CGPoint pt = [touch locationInView:self];
  CGPoint pos = [MetalView toPixels:pt];

  if (self.eventRelay.onTouchBegin) {
    self.eventRelay.onTouchBegin(pos.x, pos.y);
  }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  UITouch* touch = [touches anyObject];
  CGPoint pt = [touch locationInView:self];
  CGPoint pos = [MetalView toPixels:pt];

  if (self.eventRelay.onTouchMove) {
    self.eventRelay.onTouchMove(pos.x, pos.y);
  }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  UITouch* touch = [touches anyObject];
  CGPoint pt = [touch locationInView:self];
  CGPoint pos = [MetalView toPixels:pt];

  if (self.eventRelay.onTouchEnd) {
    self.eventRelay.onTouchEnd(pos.x, pos.y);
  }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
  NSLog(@"Touch cancel");
}
@end
// -- END MetalView --------------------------------------------------------------------------------

// -- BEGIN ViewController -------------------------------------------------------------------------
@interface ViewController : UIViewController
@property (strong, nonatomic) CADisplayLink* displayLink;
@end

@implementation ViewController
{
  lithic3d::ApplicationPtr _application;
}

- (void)loadView
{
  EventRelay* relay = [EventRelay new];
  relay.onTouchBegin = ^(float x, float y){ _application->onTouchBegin(x, y); };
  relay.onTouchMove = ^(float x, float y){ _application->onTouchMove(x, y); };
  relay.onTouchEnd = ^(float x, float y){ _application->onTouchEnd(x, y); };

  self.view = [[MetalView alloc] initWithFrame:[UIScreen mainScreen].bounds andEventRelay:relay];
}

- (BOOL)prefersStatusBarHidden
{
  return YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden
{
  return YES;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
  return UIRectEdgeAll;
}

- (void)viewDidLoad
{
  [super viewDidLoad];

  self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  CAMetalLayer* metalLayer = (CAMetalLayer*)self.view.layer;

  NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
  const char* bundlePathCStr = [bundlePath UTF8String];

  NSError* err = nil;
  NSURL* appSupport = [NSFileManager.defaultManager URLForDirectory:NSApplicationSupportDirectory
    inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&err];
  const char* appSupportPathStr = [[appSupport path] UTF8String];

  _application = lithic3d::createApplication(bundlePathCStr, appSupportPathStr,
    lithic3d::createWindowDelegate(metalLayer));
  [self MF_onViewResize];

  self.view.insetsLayoutMarginsFromSafeArea = NO;
  self.view.directionalLayoutMargins = NSDirectionalEdgeInsetsZero;
  self.view.layoutMargins = UIEdgeInsetsZero;

  self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(MF_updateLoop:)];
  [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];

  NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
  [nc addObserver:self selector:@selector(MF_onControllerConnect:)
    name:GCControllerDidConnectNotification object:nil];

  for (GCController* c in GCController.controllers) {
    [self MF_registerController:c];
  }
}

- (void)MF_registerController:(GCController*)c
{
  GCExtendedGamepad *gp = c.extendedGamepad;
  if (gp) {
    gp.buttonA.pressedChangedHandler = ^(GCControllerButtonInput* b, float value, BOOL pressed) {
      if (pressed) {
        _application->hideMobileControls();
        _application->onButtonDown(GamepadButton::A);
      }
      else {
        _application->onButtonUp(GamepadButton::A);
      }
    };
    gp.buttonB.pressedChangedHandler = ^(GCControllerButtonInput* b, float value, BOOL pressed) {
      if (pressed) {
        _application->hideMobileControls();
        _application->onButtonDown(GamepadButton::B);
      }
      else {
        _application->onButtonUp(GamepadButton::B);
      }
    };
    gp.leftThumbstick.valueChangedHandler = ^(GCControllerDirectionPad* pad, float x, float y) {
      _application->hideMobileControls();
      _application->onLeftStickMove(x, -y);
    };
    gp.rightThumbstick.valueChangedHandler = ^(GCControllerDirectionPad* pad, float x, float y) {
      _application->hideMobileControls();
      _application->onRightStickMove(x, -y);
    };
    gp.dpad.valueChangedHandler = ^(GCControllerDirectionPad* pad, float x, float y) {
      if (pad.left.isPressed) {
        _application->hideMobileControls();
        _application->onButtonDown(GamepadButton::Left);
      }
      else {
        _application->onButtonUp(GamepadButton::Left);
      }

      if (pad.right.isPressed) {
        _application->hideMobileControls();
        _application->onButtonDown(GamepadButton::Right);
      }
      else {
        _application->onButtonUp(GamepadButton::Right);
      }

      if (pad.up.isPressed) {
        _application->hideMobileControls();
        _application->onButtonDown(GamepadButton::Up);
      }
      else {
        _application->onButtonUp(GamepadButton::Up);
      }

      if (pad.down.isPressed) {
        _application->hideMobileControls();
        _application->onButtonDown(GamepadButton::Down);
      }
      else {
        _application->onButtonUp(GamepadButton::Down);
      }
    };
  }
}

- (void)MF_onControllerConnect:(NSNotification*)n
{
  [self MF_registerController:(GCController*)n.object];
}

- (void)MF_onViewResize
{
  [self MF_onViewResize:self.view.bounds.size];
}

- (void)MF_onViewResize:(CGSize)size
{
  CAMetalLayer* metalLayer = (CAMetalLayer*)self.view.layer;

  CGFloat scale = [UIScreen mainScreen].scale;
  CGSize drawableSize = CGSizeMake(size.width * scale, size.height * scale);

  metalLayer.contentsScale = scale;
  metalLayer.bounds = CGRectMake(0, 0, size.width, size.height);
  metalLayer.drawableSize = drawableSize;

  _application->onViewResize(drawableSize.width, drawableSize.height);
}

- (void)viewWillLayoutSubviews
{
  [super viewWillLayoutSubviews];
  [self MF_onViewResize:self.view.bounds.size];
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
    [self MF_onViewResize:size];
  } completion:nil];
}

- (BOOL)shouldAutorotate
{
  return YES;
}

- (NSUInteger)supportedInterfaceOrientations
{
  return UIInterfaceOrientationMaskLandscape;
}

- (void)MF_updateLoop:(CADisplayLink*)sender
{
  if (!_application->update()) {
    // On iOS, we can't quit programmatically without it looking like a crash
  }
}

- (void)dealloc
{
  [self.displayLink invalidate];
  [super dealloc];
}
@end
// -- END ViewController ---------------------------------------------------------------------------

// -- BEGIN AppDelegate ----------------------------------------------------------------------------
@interface AppDelegate : UIResponder<UIApplicationDelegate>
@property (strong, nonatomic) UIWindow* window;
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  self.window.rootViewController = [[ViewController alloc] init];
  [self.window makeKeyAndVisible];
  return YES;
}
@end
// -- END AppDelegate ------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}
