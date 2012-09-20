
#import "main.h"

//${CONFIG_BEGIN}
//${CONFIG_END}

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
		if( CFG_IOS_RETINA_ENABLED ){
			if( [self respondsToSelector:@selector(contentScaleFactor)] ){
				float scaleFactor=[[UIScreen mainScreen] scale];
				[self setContentScaleFactor:scaleFactor];
			}
		}
    
		// Get the layer
		CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
		      
		eaglLayer.opaque=TRUE;
		eaglLayer.drawableProperties=[NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:FALSE],kEAGLDrawablePropertyRetainedBacking,kEAGLColorFormatRGBA8,kEAGLDrawablePropertyColorFormat,nil];
		
		if( CFG_OPENGL_GLES20_ENABLED ){
			context=[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
			if( !context || ![EAGLContext setCurrentContext:context] ) exit(-1);
			
			glGenFramebuffers( 1,&defaultFramebuffer );
			glBindFramebuffer( GL_FRAMEBUFFER,defaultFramebuffer );
			glGenRenderbuffers( 1,&colorRenderbuffer );
			glBindRenderbuffer( GL_RENDERBUFFER,colorRenderbuffer );
			glFramebufferRenderbuffer( GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colorRenderbuffer );
			if( CFG_OPENGL_DEPTH_BUFFER_ENABLED ){
				glGenRenderbuffers( 1,&depthRenderbuffer );
				glBindRenderbuffer( GL_RENDERBUFFER,depthRenderbuffer );
				glFramebufferRenderbuffer( GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depthRenderbuffer );
				glBindRenderbuffer( GL_RENDERBUFFER,colorRenderbuffer );
			}
		}else{
			context=[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
			if( !context || ![EAGLContext setCurrentContext:context] ) exit(-1);

			glGenFramebuffersOES( 1,&defaultFramebuffer );
			glBindFramebufferOES( GL_FRAMEBUFFER_OES,defaultFramebuffer );
			glGenRenderbuffersOES( 1,&colorRenderbuffer );
			glBindRenderbufferOES( GL_RENDERBUFFER_OES,colorRenderbuffer );
			glFramebufferRenderbufferOES( GL_FRAMEBUFFER_OES,GL_COLOR_ATTACHMENT0_OES,GL_RENDERBUFFER_OES,colorRenderbuffer );
			if( CFG_OPENGL_DEPTH_BUFFER_ENABLED ){
				glGenRenderbuffersOES( 1,&depthRenderbuffer );
				glBindRenderbufferOES( GL_RENDERBUFFER_OES,depthRenderbuffer );
				glFramebufferRenderbufferOES( GL_FRAMEBUFFER_OES,GL_DEPTH_ATTACHMENT_OES,GL_RENDERBUFFER_OES,depthRenderbuffer );
				glBindRenderbufferOES( GL_RENDERBUFFER_OES,colorRenderbuffer );
			}
		}
	}
	return self;
}

-(void)drawView:(id)sender{
	MonkeyAppDelegate *delegate=(MonkeyAppDelegate*) [[UIApplication sharedApplication] delegate];
	[delegate drawView:self];
}

-(void)presentRenderbuffer{
	if( CFG_OPENGL_GLES20_ENABLED ){
		[context presentRenderbuffer:GL_RENDERBUFFER];
	}else{
		[context presentRenderbuffer:GL_RENDERBUFFER_OES];
	}
}

-(BOOL)resizeFromLayer:(CAEAGLLayer *)layer{

	// Allocate color buffer backing based on the current layer size
	if( CFG_OPENGL_GLES20_ENABLED ){
	
		glBindRenderbuffer( GL_RENDERBUFFER,colorRenderbuffer );
		[context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
		glGetRenderbufferParameteriv( GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&backingWidth );
		glGetRenderbufferParameteriv( GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&backingHeight );
		if( CFG_OPENGL_DEPTH_BUFFER_ENABLED ){
			glBindRenderbuffer( GL_RENDERBUFFER,depthRenderbuffer );
			glRenderbufferStorage( GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,backingWidth,backingHeight );
			glBindRenderbuffer( GL_RENDERBUFFER,colorRenderbuffer );
		}
		if( glCheckFramebufferStatus( GL_FRAMEBUFFER )!=GL_FRAMEBUFFER_COMPLETE ) exit(-1);
		
	}else{
	
		glBindRenderbufferOES( GL_RENDERBUFFER_OES,colorRenderbuffer );
		[context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:layer];
		glGetRenderbufferParameterivOES( GL_RENDERBUFFER_OES,GL_RENDERBUFFER_WIDTH_OES,&backingWidth );
		glGetRenderbufferParameterivOES( GL_RENDERBUFFER_OES,GL_RENDERBUFFER_HEIGHT_OES,&backingHeight );
		if( CFG_OPENGL_DEPTH_BUFFER_ENABLED ){
			glBindRenderbufferOES( GL_RENDERBUFFER_OES,depthRenderbuffer );
			glRenderbufferStorageOES( GL_RENDERBUFFER_OES,GL_DEPTH_COMPONENT16_OES,backingWidth,backingHeight );
			glBindRenderbufferOES( GL_RENDERBUFFER_OES,colorRenderbuffer );
		}
		if( glCheckFramebufferStatusOES( GL_FRAMEBUFFER_OES )!=GL_FRAMEBUFFER_COMPLETE_OES ) exit(-1);
		
	}
    
	return YES;
}

-(void)layoutSubviews{
	[self resizeFromLayer:(CAEAGLLayer*)self.layer];
	
	[self drawView:nil];
}

-(void)dealloc{
	if( CFG_OPENGL_GLES20_ENABLED ){
		glDeleteFramebuffers( 1,&defaultFramebuffer );
		glDeleteRenderbuffers( 1,&colorRenderbuffer );
		if( depthRenderbuffer ) glDeleteRenderbuffers( 1,&depthRenderbuffer );
		if( [EAGLContext currentContext]==context ) [EAGLContext setCurrentContext:nil];
		[context release];
	}else{
		glDeleteFramebuffersOES( 1,&defaultFramebuffer );
		glDeleteRenderbuffersOES( 1,&colorRenderbuffer );
		if( depthRenderbuffer ) glDeleteRenderbuffersOES( 1,&depthRenderbuffer );
		if( [EAGLContext currentContext]==context ) [EAGLContext setCurrentContext:nil];
		[context release];
	}
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
//${TRANSCODE_END}

FILE *fopenFile( String path,const char *mode ){
	if( path.StartsWith( "monkey://data/" ) ) path=String("data/")+path.Slice(14);
	return fopen( path.ToCString<char>(),"rb" );
}

//***** main.m *****

int main(int argc, char *argv[]) {

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
    UIApplicationMain( argc,argv,nil,nil );
    
    [pool release];
	
	return 0;
}
