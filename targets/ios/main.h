
#import <UIKit/UIKit.h>

#import <QuartzCore/QuartzCore.h>

#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>

#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>

#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

#include <OpenAL/al.h>
#include <OpenAL/alc.h>

// ***** MonkeyView *****

@interface MonkeyView : UIView{
@public
	EAGLContext *context;
	
	// The pixel dimensions of the CAEAGLLayer
	GLint backingWidth;
	GLint backingHeight;
	
	// The OpenGL names for the framebuffer and renderbuffer used to render to this view
	GLuint defaultFramebuffer;
	GLuint colorRenderbuffer;
	GLuint depthRenderbuffer;
}

-(void)presentRenderbuffer;

@end

// ***** MonkeyWindow *****

@interface MonkeyWindow : UIWindow{
}

//-(void)sendEvent:(UIEvent*)event;

@end

// ***** MonkeyViewController *****

@interface MonkeyViewController : UIViewController{
@public
}

@end

// ***** MonkeyAppDelegate *****

@interface MonkeyAppDelegate : NSObject <UIApplicationDelegate,UIAccelerometerDelegate>{
@public
	UIWindow *window;
	MonkeyView *view;
	MonkeyViewController *viewController;
}
	
@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet MonkeyView *view;
@property (nonatomic, retain) IBOutlet MonkeyViewController *viewController;

-(void)drawView:(MonkeyView*)view;
-(void)onEvent:(UIEvent*)event;
-(void)accelerometer:(UIAccelerometer*)accelerometer didAccelerate:(UIAcceleration *)acceleration;

@end
