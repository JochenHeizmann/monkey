
#import "main.h"

@implementation MonkeyView

+(Class)layerClass{
	return [CAEAGLLayer class];
}

-(id)initWithCoder:(NSCoder*)coder{

	defaultFramebuffer=0;
	colorRenderbuffer=0;
	depthRenderbuffer=0;

	if( self=[super initWithCoder:coder] ){
	
		// Enable retina display
#if RETINA_ENABLED
		if( [self respondsToSelector:@selector(contentScaleFactor)] ){
			float scaleFactor=[[UIScreen mainScreen] scale];
			[self setContentScaleFactor:scaleFactor];
		}
#endif
    
		// Get the layer
		CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
		      
		eaglLayer.opaque=TRUE;
		eaglLayer.drawableProperties=[NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:FALSE],kEAGLDrawablePropertyRetainedBacking,kEAGLColorFormatRGBA8,kEAGLDrawablePropertyColorFormat,nil];
		
		//ES1 renderer...
		context=[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
		      
		if( !context || ![EAGLContext setCurrentContext:context] ){
			[self release];
			return nil;
		}
		
		glGenFramebuffersOES( 1,&defaultFramebuffer );
		glBindFramebufferOES( GL_FRAMEBUFFER_OES,defaultFramebuffer );
		glGenRenderbuffersOES( 1,&colorRenderbuffer );
		glBindRenderbufferOES( GL_RENDERBUFFER_OES,colorRenderbuffer );
		glFramebufferRenderbufferOES( GL_FRAMEBUFFER_OES,GL_COLOR_ATTACHMENT0_OES,GL_RENDERBUFFER_OES,colorRenderbuffer );

#if DEPTH_BUFFER_ENABLED
		glGenRenderbuffersOES( 1,&depthRenderbuffer );
		glBindRenderbufferOES( GL_RENDERBUFFER_OES,depthRenderbuffer );
		glFramebufferRenderbufferOES( GL_FRAMEBUFFER_OES,GL_DEPTH_ATTACHMENT_OES,GL_RENDERBUFFER_OES,depthRenderbuffer );
		glBindRenderbufferOES( GL_RENDERBUFFER_OES,colorRenderbuffer );
#endif
		
	}
	return self;
}

-(void)drawView:(id)sender{
	MonkeyAppDelegate *delegate=(MonkeyAppDelegate*) [[UIApplication sharedApplication] delegate];
	[delegate drawView:self];
}

-(void)presentRenderbuffer{
	[context presentRenderbuffer:GL_RENDERBUFFER_OES];
}

-(BOOL)resizeFromLayer:(CAEAGLLayer *)layer{

	// Allocate color buffer backing based on the current layer size
	glBindRenderbufferOES( GL_RENDERBUFFER_OES,colorRenderbuffer );
	[context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:layer];
	glGetRenderbufferParameterivOES( GL_RENDERBUFFER_OES,GL_RENDERBUFFER_WIDTH_OES,&backingWidth );
	glGetRenderbufferParameterivOES( GL_RENDERBUFFER_OES,GL_RENDERBUFFER_HEIGHT_OES,&backingHeight );
	
#if DEPTH_BUFFER_ENABLED
	glBindRenderbufferOES( GL_RENDERBUFFER_OES,depthRenderbuffer );
	glRenderbufferStorageOES( GL_RENDERBUFFER_OES,GL_DEPTH_COMPONENT16_OES,backingWidth,backingHeight );
	glBindRenderbufferOES( GL_RENDERBUFFER_OES,colorRenderbuffer );
#endif
	
	if( glCheckFramebufferStatusOES( GL_FRAMEBUFFER_OES )!=GL_FRAMEBUFFER_COMPLETE_OES ){
		NSLog( @"Failed to make complete framebuffer object %x",glCheckFramebufferStatusOES( GL_FRAMEBUFFER_OES ) );
		abort();
	}
    
	return YES;
}

-(void)layoutSubviews{
	[self resizeFromLayer:(CAEAGLLayer*)self.layer];
	
	[self drawView:nil];
}

-(void)dealloc{

	// Tear down GL
	if( defaultFramebuffer ){
		glDeleteFramebuffersOES( 1,&defaultFramebuffer );
		defaultFramebuffer=0;
	}

	if( colorRenderbuffer ){
		glDeleteRenderbuffersOES( 1,&colorRenderbuffer );
		colorRenderbuffer=0;
	}
	
	if( depthRenderbuffer ){
		glDeleteRenderbuffersOES( 1,&depthRenderbuffer );
		depthRenderbuffer=0;
	}	
	
	// Tear down context
	if( [EAGLContext currentContext]==context ){
        [EAGLContext setCurrentContext:nil];
	}

	[context release];
	context = nil;
	
	[super dealloc];
}

@end

@implementation MonkeyWindow

/*
-(void)sendEvent:(UIEvent*)event{
	[super sendEvent:event];
	MonkeyAppDelegate *delegate=(MonkeyAppDelegate*)[[UIApplication sharedApplication] delegate];
	[delegate onEvent:event];
}
*/

-(void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event{
	MonkeyAppDelegate *delegate=(MonkeyAppDelegate*)[[UIApplication sharedApplication] delegate];
	[delegate onEvent:event];
}

-(void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event{
	MonkeyAppDelegate *delegate=(MonkeyAppDelegate*)[[UIApplication sharedApplication] delegate];
	[delegate onEvent:event];
}

-(void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event{
	MonkeyAppDelegate *delegate=(MonkeyAppDelegate*)[[UIApplication sharedApplication] delegate];
	[delegate onEvent:event];
}

-(void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event{
	MonkeyAppDelegate *delegate=(MonkeyAppDelegate*)[[UIApplication sharedApplication] delegate];
	[delegate onEvent:event];
}

@end

@implementation MonkeyViewController

-(BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation{

	CFArrayRef array=(CFArrayRef)CFBundleGetValueForInfoDictionaryKey( CFBundleGetMainBundle(),CFSTR("UISupportedInterfaceOrientations") );
	if( !array ) return NO;
	
	CFRange range={ 0,CFArrayGetCount( array ) };

	switch( interfaceOrientation ){
	case UIInterfaceOrientationPortrait:
		return CFArrayContainsValue( array,range,CFSTR("UIInterfaceOrientationPortrait") );
	case UIInterfaceOrientationPortraitUpsideDown:
		return CFArrayContainsValue( array,range,CFSTR("UIInterfaceOrientationPortraitUpsideDown") );
	case UIInterfaceOrientationLandscapeLeft:
		return CFArrayContainsValue( array,range,CFSTR("UIInterfaceOrientationLandscapeLeft") );
	case UIInterfaceOrientationLandscapeRight:
		return CFArrayContainsValue( array,range,CFSTR("UIInterfaceOrientationLandscapeRight") );
	}
	return NO;
}

@end


//***** MONKEY CODE GOES HERE! *****

//${TRANSCODE_BEGIN}

// Note: MonkeyAppDelgate should be implemented by client code - this is just a sample implementation:

@implementation MonkeyAppDelegate

@synthesize window;
@synthesize view;
@synthesize viewController;

-(void)drawView:(MonkeyView*)aview{

	int width=aview->backingWidth;
	int height=aview->backingHeight;
	
	printf( "%i %i\n",width,height );

	glClearColor( 1,0,0,1 );
	glClear( GL_COLOR_BUFFER_BIT );
	[aview presentRenderbuffer];
}

-(void)onEvent:(UIEvent*)event{
}

-(BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions{    

	[[UIApplication sharedApplication] setStatusBarHidden:YES animated:NO];

	[viewController.view setFrame:[[UIScreen mainScreen] applicationFrame]];
	
    [window addSubview:viewController.view];
    [window makeKeyAndVisible];

	[[UIAccelerometer sharedAccelerometer] setUpdateInterval:(1.0/30)];
	[[UIAccelerometer sharedAccelerometer] setDelegate:self];
	[[UIApplication sharedApplication] setIdleTimerDisabled:YES];
	
	return YES;
}

-(void)applicationDidFinishLaunching:(UIApplication *)application{
}

-(void)applicationWillResignActive:(UIApplication *)application{
}

-(void)applicationDidBecomeActive:(UIApplication *)application{
}

-(void)applicationWillTerminate:(UIApplication *)application{
}

-(void)dealloc{
	[window release];
	[view release];
	[super dealloc];
}

@end

//${TRANSCODE_END}

//***** main.m *****

int main(int argc, char *argv[]) {

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
    UIApplicationMain( argc,argv,nil,nil );
    
    [pool release];
	
	return 0;
}
