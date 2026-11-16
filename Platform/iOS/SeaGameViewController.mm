// SeaGameViewController.mm - hosts the engine: Metal view, frame loop, touch/keyboard input, audio session.
//
// NOTE: written without access to Xcode (see docs/PORTING.md, "Verification status").  The logic that
// decides *what* input means lives in SeaTouchRouter / SeaKeyMap (unit-tested); this file only forwards.
#import "SeaGameViewController.h"

#import <AVFoundation/AVFoundation.h>
#import <GameController/GameController.h>

#include <algorithm>
#include <memory>
#include <unordered_map>

#import "SeaMetalRenderer.h"
#import "SeaResourceStaging.h"
#include "SeaImageDecoder.h"
#include "../SeaTouchRouter.h"
#include "../SeaKeyMap.h"
#include "../DxCompat/DxLib.h"
#include "../../Seaurchin/GameHost.h"

static const int kVirtualWidth = 1280;     // SU_RES_WIDTH
static const int kVirtualHeight = 720;     // SU_RES_HEIGHT

@implementation SeaGameViewController {
    MTKView *_mtkView;
    std::unique_ptr<sea::MetalRenderer> _renderer;
    std::unique_ptr<sea::TouchRouter> _router;
    std::unordered_map<void *, int> _fingerIds;
    int _nextFingerId;
    BOOL _started;
    BOOL _failed;
    BOOL _autoplayOn;          // mirrors the engine default (autoplay unless the player pressed M on the select screen)
}

// ------------------------------------------------------------------ view ---
- (void)loadView
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    _mtkView = [[MTKView alloc] initWithFrame:UIScreen.mainScreen.bounds device:device];
    _mtkView.multipleTouchEnabled = YES;
    _mtkView.exclusiveTouch = YES;
    _mtkView.preferredFramesPerSecond = 60;     // the engine is time-based; raise for ProMotion if desired
    _mtkView.enableSetNeedsDisplay = NO;
    _mtkView.paused = NO;
    _mtkView.delegate = self;
    self.view = _mtkView;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    _renderer = std::make_unique<sea::MetalRenderer>(_mtkView, kVirtualWidth, kVirtualHeight);

    sea::TouchRouter::Sink sink;
    sink.sliderDown = [](int f, double x) { sea::GameHostTouchBegan(f, x); };
    sink.sliderMove = [](int f, double x) { sea::GameHostTouchMoved(f, x); };
    sink.sliderUp   = [](int f) { sea::GameHostTouchEnded(f); };
    __weak SeaGameViewController *weakSelf = self;
    sink.key = [weakSelf](sea::TouchRouter::Key k) {
        using K = sea::TouchRouter::Key;
        switch (k) {
            case K::Up:     sea::DxPulseKey(KEY_INPUT_UP); break;
            case K::Down:   sea::DxPulseKey(KEY_INPUT_DOWN); break;
            case K::Left:   sea::DxPulseKey(KEY_INPUT_LEFT); break;
            case K::Right:  sea::DxPulseKey(KEY_INPUT_RIGHT); break;
            case K::Return: sea::DxPulseKey(KEY_INPUT_RETURN); break;
            case K::Escape: sea::DxPulseKey(KEY_INPUT_ESCAPE); break;
            case K::ToggleAutoplay: {
                // The select-screen script: A = autoplay on, M = autoplay off (it shows a confirmation message).
                SeaGameViewController *s = weakSelf;
                if (!s) break;
                s->_autoplayOn = !s->_autoplayOn;
                sea::DxPulseKey(s->_autoplayOn ? KEY_INPUT_A : KEY_INPUT_M);
                break;
            }
        }
    };
    sea::TouchRouterConfig cfg;
    cfg.virtualWidth = kVirtualWidth;
    cfg.virtualHeight = kVirtualHeight;
    _autoplayOn = YES;
    _router = std::make_unique<sea::TouchRouter>(cfg, sink);

    [self setupHardwareKeyboard];

    NSNotificationCenter *nc = NSNotificationCenter.defaultCenter;
    [nc addObserver:self selector:@selector(appWillResignActive:) name:UIApplicationWillResignActiveNotification object:nil];
    [nc addObserver:self selector:@selector(appDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    if (_started) sea::GameHostStop();
}

// Landscape only, and keep the system from stealing edge swipes while the player is touching the bottom lanes.
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)shouldAutorotate { return YES; }

// ---------------------------------------------------------------- engine ---
- (void)configureAudioSession
{
    AVAudioSession *session = AVAudioSession.sharedInstance;
    NSError *error = nil;
    [session setCategory:AVAudioSessionCategoryPlayback error:&error];
    [session setPreferredIOBufferDuration:0.005 error:&error];      // small buffer = low latency (hardware may round up)
    [session setActive:YES error:&error];
    if (error) NSLog(@"Seaurchin: audio session: %@", error);
}

- (void)startGame
{
    _started = YES;
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;

    NSError *error = nil;
    if (![SeaResourceStaging stageBundledDataInto:documents error:&error]) {
        NSLog(@"Seaurchin: could not stage resources: %@", error);
        _failed = YES;
        return;
    }
    [self configureAudioSession];

    sea::GameHostConfig config;
    config.rootDirectory = documents.UTF8String;
    config.renderer = _renderer.get();
    config.imageDecoder = SeaMakeImageDecoder();
    config.width = kVirtualWidth;
    config.height = kVirtualHeight;

    _renderer->BeginFrame();                       // skins may create render targets / textures during start-up
    const bool ok = sea::GameHostStart(config);
    _renderer->EndFrame();
    if (!ok) { NSLog(@"Seaurchin: GameHostStart failed"); _failed = YES; return; }

    [self configureAudioSession];                  // BASS_Init may have changed the category; re-assert ours
}

- (void)drawInMTKView:(MTKView *)view
{
    if (_failed) return;
    if (!_started) { [self startGame]; return; }   // first callback only does the (slow) start-up
    _renderer->BeginFrame();
    sea::GameHostFrame();                          // Tick + Draw + ScreenFlip -> MetalRenderer::Present
    _renderer->EndFrame();
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {}

- (void)appWillResignActive:(NSNotification *)n
{
    _mtkView.paused = YES;
    sea::GameHostSuspend();
}

- (void)appDidBecomeActive:(NSNotification *)n
{
    sea::GameHostResume();
    _mtkView.paused = NO;
}

// ----------------------------------------------------------------- input ---
- (void)setupHardwareKeyboard
{
    void (^attach)(GCKeyboard *) = ^(GCKeyboard *keyboard) {
        keyboard.keyboardInput.keyChangedHandler = ^(GCKeyboardInput *kb, GCControllerButtonInput *key, GCKeyCode code, BOOL pressed) {
            const int dx = sea::HidUsageToDxKey((int)code);
            if (dx >= 0) sea::DxSetKeyHeld(dx, pressed);
        };
    };
    if (GCKeyboard.coalescedKeyboard) attach(GCKeyboard.coalescedKeyboard);
    [NSNotificationCenter.defaultCenter addObserverForName:GCKeyboardDidConnectNotification object:nil queue:NSOperationQueue.mainQueue
                                                usingBlock:^(NSNotification *note) { attach((GCKeyboard *)note.object); }];
}

// UIKit points -> virtual 1280x720 coordinates (inverse of the letterboxing done by the renderer)
- (void)virtualPointForTouch:(UITouch *)touch x:(double *)x y:(double *)y
{
    const CGPoint p = [touch locationInView:_mtkView];
    const CGRect r = _renderer->ContentRectInView();
    *x = std::min<double>(std::max<double>((p.x - r.origin.x) / r.size.width * kVirtualWidth, 0.0), kVirtualWidth);
    *y = std::min<double>(std::max<double>((p.y - r.origin.y) / r.size.height * kVirtualHeight, 0.0), kVirtualHeight);
}

- (int)fingerIdForTouch:(UITouch *)touch create:(BOOL)create
{
    void *key = (__bridge void *)touch;
    auto it = _fingerIds.find(key);
    if (it != _fingerIds.end()) return it->second;
    if (!create) return -1;
    const int id = _nextFingerId++;
    _fingerIds[key] = id;
    return id;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *t in touches) {
        double x, y; [self virtualPointForTouch:t x:&x y:&y];
        _router->Down([self fingerIdForTouch:t create:YES], x, y, t.timestamp);
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *t in touches) {
        const int id = [self fingerIdForTouch:t create:NO];
        if (id < 0) continue;
        double x, y; [self virtualPointForTouch:t x:&x y:&y];
        _router->Move(id, x, y, t.timestamp);
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *t in touches) {
        const int id = [self fingerIdForTouch:t create:NO];
        if (id < 0) continue;
        double x, y; [self virtualPointForTouch:t x:&x y:&y];
        _router->Up(id, x, y, t.timestamp);
        _fingerIds.erase((__bridge void *)t);
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *t in touches) {
        const int id = [self fingerIdForTouch:t create:NO];
        if (id < 0) continue;
        _router->Cancel(id);
        _fingerIds.erase((__bridge void *)t);
    }
}

@end
