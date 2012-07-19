
// iOS mojo runtime.
//
// Copyright 2011 Mark Sibly, all rights reserved.
// No warranty implied; use at your own risk.

class gxtkApp;
class gxtkGraphics;
class gxtkSurface;
class gxtkInput;
class gxtkAudio;
class gxtkSample;

#include <mach/mach_time.h>

#define KEY_LMB 1
#define KEY_TOUCH0 0x180

#define MAX_CHANNELS 32

gxtkApp *app;

String tostring( NSString *nsstr ){
	return String( [nsstr UTF8String] );
}

NSString *tonsstr( String str ){
	return [NSString stringWithUTF8String:str.ToCString<char>() ];
}

int Pow2Size( int n ){
	int i=1;
	while( i<n ) i*=2;
	return i;
}

BOOL CheckForExtension( NSString *name ){
	static NSArray *extensions;
	if( !extensions ){
		NSString *extensionsString=[NSString stringWithCString:(const char*)glGetString(GL_EXTENSIONS) encoding:NSASCIIStringEncoding];
		extensions=[extensionsString componentsSeparatedByString:@" "];
		[extensions retain];	//?Really needed?
	}
	return [extensions containsObject:name];
}

class gxtkObject : public Object{
public:
};

class gxtkApp : public gxtkObject{
public:
	MonkeyAppDelegate *appDelegate;

	gxtkGraphics *graphics;
	gxtkInput *input;
	gxtkAudio *audio;
	
	uint64_t startTime;
	mach_timebase_info_data_t timeInfo;
	
	int created;
	int updateRate;
	NSTimer *updateTimer;
	
	gxtkApp();
	
	virtual int InvokeOnCreate();
	virtual int InvokeOnUpdate();
	virtual int InvokeOnSuspend();
	virtial int InvokeOnResume();
	virtual int InvokeOnRender();

	//***** GXTK API *****

	virtual gxtkGraphics *GraphicsDevice();
	virtual gxtkInput *InputDevice();
	virtual gxtkAudio *AudioDevice();
	virtual String AppTitle();
	virtual String LoadState();
	virtual int SaveState( String state );
	virtual String LoadString( String path );
	virtual int SetUpdateRate( int hertz );
	virtual int MilliSecs();
	virtual int Loading();
	virtual int OnCreate();
	virtual int OnUpdate();
	virtual int OnSuspend();
	virtual int OnResume();
	virtual int OnRender();
	virtual int OnLoading();
};

//***** START OF COMMON OPENGL CODE *****

#define MAX_VERTS	1024

struct Vertex{
	float x,y;
	float u,v;
	unsigned c;
};

class gxtkGraphics : public gxtkObject{
public:

	int width;
	int height;

	Vertex verts[MAX_VERTS];
	int color,acolor;
	float r,g,b,alpha;
	float matrix[16];
	bool tformed;

	int vertCount;
	int primType;
	GLuint texture;
	
	gxtkGraphics();
	
	bool Validate();		
	void BeginRender();
	void EndRender();
	void Flush();
	void AddVertex( float x,float y );
	void AddVertex( float x,float y,float u,float v );
	
	//***** GXTK API *****
	virtual int Width();
	virtual int Height();

	virtual gxtkSurface *LoadSurface( String path );
	virtual int DestroySurface( gxtkSurface *surface );
	
	virtual int Cls( float r,float g,float b );
	virtual int SetAlpha( float alpha );
	virtual int SetColor( float r,float g,float b );
	virtual int SetBlend( int blend );
	virtual int SetScissor( int x,int y,int w,int h );
	virtual int SetMatrix( float ix,float iy,float jx,float jy,float tx,float ty );
	
	virtual int DrawRect( float x,float y,float w,float h );
	virtual int DrawLine( float x1,float y1,float x2,float y2 );
	virtual int DrawOval( float x1,float y1,float x2,float y2 );
	virtual int DrawSurface( gxtkSurface *surface,float x,float y );
	virtual int DrawSurface2( gxtkSurface *surface,float x,float y,int srcx,int srcy,int srcw,int srch );
};

//***** gxtkSurface *****

class gxtkSurface : public gxtkObject{
public:
	GLuint texture;
	int width;
	int height;
	float uscale;
	float vscale;
	
	gxtkSurface( GLuint texture,int width,int height,float uscale,float vscale );
	
	//***** GXTK API *****
	virtual int Width();
	virtual int Height();
	virtual int Loaded();

};

//***** gxtkGraphics *****

gxtkGraphics::gxtkGraphics(){
	width=height=0;
	memset( matrix,0,sizeof(matrix) );
	matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;
}

void gxtkGraphics::BeginRender(){
	glViewport( 0,0,width,height );

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrthof( 0,width,height,0,-1,1 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	
	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer( 2,GL_FLOAT,sizeof(Vertex),&verts[0].x );	
	
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2,GL_FLOAT,sizeof(Vertex),&verts[0].u );
	
	glEnableClientState( GL_COLOR_ARRAY );
	glColorPointer( 4,GL_UNSIGNED_BYTE,sizeof(Vertex),&verts[0].c );
	
	glDisable( GL_TEXTURE_2D );

	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE,GL_ONE_MINUS_SRC_ALPHA );
	
	vertCount=0;
}

void gxtkGraphics::Flush(){
	if( !vertCount ) return;

	if( texture ){
		glEnable( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D,texture );
	}
		
	switch( primType ){
	case 2:
		glDrawArrays( GL_LINES,0,vertCount );
		break;
	case 3:
		glDrawArrays( GL_TRIANGLES,0,vertCount );
		break;
	case 5:
		glDrawArrays( GL_TRIANGLE_FAN,0,vertCount );
		break;
	}

	if( texture ){
		glDisable( GL_TEXTURE_2D );
	}
	
	vertCount=0;
}

void gxtkGraphics::AddVertex( float x,float y ){
	verts[vertCount].x=x;
	verts[vertCount].y=y;
	verts[vertCount++].c=color;
}

void gxtkGraphics::AddVertex( float x,float y,float u,float v ){
	verts[vertCount].x=x;
	verts[vertCount].y=y;
	verts[vertCount].u=u;
	verts[vertCount].v=v;
	verts[vertCount++].c=acolor;
}

//***** GXTK API *****

int gxtkGraphics::Width(){
	return width;
}

int gxtkGraphics::Height(){
	return height;
}

int gxtkGraphics::Cls( float r,float g,float b ){
	vertCount=0;

	glClearColor( r/255.0f,g/255.0f,b/255.0f,1 );
	glClear( GL_COLOR_BUFFER_BIT );

	return 0;
}

int gxtkGraphics::SetAlpha( float alpha ){

	//Flushless!

	this->alpha=alpha;
	
	int a=int(alpha*255);
	
	color=(a<<24) | (int(b*alpha)<<16) | (int(g*alpha)<<8) | int(r*alpha);
	
	acolor=(a<<24) | (a<<16) | (a<<8) | a;
	
	return 0;
}

int gxtkGraphics::SetColor( float r,float g,float b ){

	//Flushless!
	
	this->r=r;
	this->g=g;
	this->b=b;

	int a=int(alpha*255);
	
	color=(a<<24) | (int(b*alpha)<<16) | (int(g*alpha)<<8) | int(r*alpha);
	
	acolor=(a<<24) | (a<<16) | (a<<8) | a;
	
	return 0;
}

int gxtkGraphics::SetBlend( int blend ){

	Flush();
	
	switch( blend ){
	case 1:
		glBlendFunc( GL_ONE,GL_ONE );
		break;
	default:
		glBlendFunc( GL_ONE,GL_ONE_MINUS_SRC_ALPHA );
	}

	return 0;
}

int gxtkGraphics::SetScissor( int x,int y,int w,int h ){

	Flush();
	
	if( x!=0 || y!=0 || w!=Width() || h!=Height() ){
		glEnable( GL_SCISSOR_TEST );
		y=Height()-y-h;
		glScissor( x,y,w,h );
	}else{
		glDisable( GL_SCISSOR_TEST );
	}
	return 0;
}

int gxtkGraphics::SetMatrix( float ix,float iy,float jx,float jy,float tx,float ty ){

	Flush();
	
	tformed=(ix!=1 || iy!=0 || jx!=0 || jy!=1 || tx!=0 || ty!=0);
	matrix[0]=ix;
	matrix[1]=iy;
	matrix[4]=jx;
	matrix[5]=jy;
	matrix[12]=tx;
	matrix[13]=ty;
	
	if( tformed ){
		glLoadMatrixf( matrix );
	}else{
		glLoadIdentity();
	}
	return 0;
}

int gxtkGraphics::DrawLine( float x1,float y1,float x2,float y2 ){

	if( vertCount==0 || primType!=2 || vertCount+2>MAX_VERTS || texture ){
		Flush();
		primType=2;
		texture=0;
	}
	
	AddVertex( x1,y1 );
	AddVertex( x2,y2 );

	return 0;
}
	
int gxtkGraphics::DrawRect( float x,float y,float w,float h ){
	
	if( vertCount==0 || primType!=3 || vertCount+6>MAX_VERTS || texture ){
		Flush();
		primType=3;
		texture=0;
	}
	
	AddVertex( x,y );
	AddVertex( x+w,y );
	AddVertex( x+w,y+h );
	
	AddVertex( x,y );
	AddVertex( x+w,y+h );
	AddVertex( x,y+h );

	return 0;
}

int gxtkGraphics::DrawOval( float x,float y,float w,float h ){
	
	Flush();
	
	primType=5;
	texture=0;
	
	float xr=w/2.0f;
	float yr=h/2.0f;

	int segs;	
	if( tformed ){
		float xx=xr*matrix[0],xy=xr*matrix[1],xd=sqrt(xx*xx+xy*xy);
		float yx=yr*matrix[4],yy=yr*matrix[5],yd=sqrt(yx*yx+yy*yy);
		segs=int( xd+yd );
	}else{
		segs=int( abs(xr)+abs(yr) );
	}

	if( segs>MAX_VERTS ){
		segs=MAX_VERTS;
	}else if( segs<12 ){
		segs=12;
	}else{
		segs&=~3;
	}
	
	x+=xr;
	y+=yr;

	for( int i=0;i<segs;++i ){
		float th=i * 6.28318531f / segs;
		AddVertex( x+cos(th)*xr,y+sin(th)*yr );
	}
	
	return 0;
}

int gxtkGraphics::DestroySurface( gxtkSurface *surface ){
	glDeleteTextures( 1,&surface->texture );
	return 0;
}

int gxtkGraphics::DrawSurface( gxtkSurface *surface,float x,float y ){

	if( vertCount==0 || primType!=3 || vertCount+6>MAX_VERTS || texture!=surface->texture ){
		Flush();
		primType=3;
		texture=surface->texture;
	}
		
	float w=surface->width;
	float h=surface->height;
	float u0=0,u1=w * surface->uscale;
	float v0=0,v1=h * surface->vscale;

	AddVertex( x,y,u0,v0 );
	AddVertex( x+w,y,u1,v0 );
	AddVertex( x+w,y+h,u1,v1 );
	
	AddVertex( x,y,u0,v0 );
	AddVertex( x+w,y+h,u1,v1 );
	AddVertex( x,y+h,u0,v1 );

	return 0;
}

int gxtkGraphics::DrawSurface2( gxtkSurface *surface,float x,float y,int srcx,int srcy,int srcw,int srch ){

	if( vertCount==0 || primType!=3 || vertCount+6>MAX_VERTS || texture!=surface->texture ){
		Flush();
		primType=3;
		texture=surface->texture;
	}
		
	float w=srcw;
	float h=srch;
	float u0=srcx * surface->uscale,u1=(srcx+srcw) * surface->uscale;
	float v0=srcy * surface->vscale,v1=(srcy+srch) * surface->vscale;

	AddVertex( x,y,u0,v0 );
	AddVertex( x+w,y,u1,v0 );
	AddVertex( x+w,y+h,u1,v1 );
	
	AddVertex( x,y,u0,v0 );
	AddVertex( x+w,y+h,u1,v1 );
	AddVertex( x,y+h,u0,v1 );

	return 0;
}

//***** gxtkSurface *****

gxtkSurface::gxtkSurface( GLuint texture,int width,int height,float uscale,float vscale ):
	texture(texture),width(width),height(height),uscale(uscale),vscale(vscale){
}

int gxtkSurface::Width(){
	return width;
}

int gxtkSurface::Height(){
	return height;
}

int gxtkSurface::Loaded(){
	return 1;
}

//***** END OF COMMON OPENGL CODE *****

bool gxtkGraphics::Validate(){
	width=app->appDelegate->view->backingWidth;
	height=app->appDelegate->view->backingHeight;
	return width>0 && height>0;
}

void gxtkGraphics::EndRender(){
	Flush();
	[app->appDelegate->view presentRenderbuffer];
}

gxtkSurface *gxtkGraphics::LoadSurface( String path ){

	//Lawks a lordy! Had a real hassle with attempting to use stringWithCharacters and towstr...gave up for now.
	//
	path=String( L"data/" )+path;
	NSString *nspath=tonsstr( path );
	
	UIImage *uiimage=[ UIImage imageNamed:nspath ];
	if( !uiimage ) return 0;
	
	CGImageRef cgimage=uiimage.CGImage;
	
	int width=CGImageGetWidth( cgimage );
	int height=CGImageGetHeight( cgimage );
	
	int texwidth,texheight;
	
	if( CheckForExtension( @"GL_APPLE_texture_2D_limited_npot" ) ){
		texwidth=width;
		texheight=height;
	}else{
		texwidth=Pow2Size( width );
		texheight=Pow2Size( height );
	}
	
	float uscale=1.0f/texwidth;
	float vscale=1.0f/texheight;

	void *data=calloc( texwidth*texheight,4 );
	
	CGContextRef context=CGBitmapContextCreate( data,width,height,8,texwidth*4,CGImageGetColorSpace(cgimage),kCGImageAlphaPremultipliedLast );
	CGContextDrawImage( context,CGRectMake(0,0,width,height),cgimage );
	CGContextRelease( context );
	
	GLuint texture;
	glGenTextures( 1,&texture );
	glBindTexture( GL_TEXTURE_2D,texture );

	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR );
	
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE );
	
	glTexImage2D( GL_TEXTURE_2D,0,GL_RGBA,texwidth,texheight,0,GL_RGBA,GL_UNSIGNED_BYTE,data );
	
	free( data );
	
	return new gxtkSurface( texture,width,height,uscale,vscale );
}

//***** END OF GRAPHICS *****


class gxtkInput : public gxtkObject{
public:
	int keyStates[512];
	UITouch *touches[32];
	float touchX[32];
	float touchY[32];
	float accelX,accelY,accelZ;
	
	gxtkInput();
	
	void OnEvent( UIEvent *event );
	void OnAcceleration( UIAcceleration *accel );
	
	void BeginUpdate();
	void EndUpdate();
	
	//***** GXTK API *****
	virtual int KeyDown( int key );
	virtual int KeyHit( int key );
	virtual int GetChar();
	
	virtual float MouseX();
	virtual float MouseY();

	virtual float JoyX( int index );
	virtual float JoyY( int index );
	virtual float JoyZ( int index );

	virtual float TouchX( int index );
	virtual float TouchY( int index );
	
	virtual float AccelX();
	virtual float AccelY();
	virtual float AccelZ();
};

class gxtkAudio : public gxtkObject{
public:

	ALuint al_sources[MAX_CHANNELS];
	int nextSource;

	gxtkAudio();

	virtual int AL_Source( int channel );
	
	//***** GXTK API *****
	virtual gxtkSample *LoadSample( String path );
	virtual int DestroySample( gxtkSample *sample );
	virtual int PlaySample( gxtkSample *sample,int channel,int flags );
	virtual int StopChannel( int channel );
	virtual int ChannelState( int channel );
	virtual int SetVolume( int channel,float volume );
	virtual int SetPan( int channel,float pan );
	virtual int SetRate( int channel,float rate );
};

class gxtkSample : public gxtkObject{
public:
	ALuint al_buffer;
	
	gxtkSample( String path );
	
	//***** GXTK API *****
};

//***** gxtkApp *****

gxtkApp::gxtkApp(){
	app=this;
	
	appDelegate=(MonkeyAppDelegate*)[[UIApplication sharedApplication] delegate];
	
	graphics=new gxtkGraphics;
	input=new gxtkInput;
	audio=new gxtkAudio;
	
	mach_timebase_info( &timeInfo );
	startTime=mach_absolute_time();
	
	created=0;
	updateRate=0;
	updateTimer=0;
}

int gxtkApp::InvokeOnCreate(){
	Die( "Interal Error" );
	return 0;
}

int gxtkApp::InvokeOnUpdate(){
	try{
		if( graphics->Validate() ){
			input->BeginUpdate();
			OnUpdate();
			input->EndUpdate();
		}
	}catch( const char *p ){
		Die( p );
	}
	return 0;
}

int gxtkApp::InvokeOnSuspend(){
	try{
		OnSuspend();
	}catch( const char *p ){
		Die( p );
	}
	return 0;
}

int gxtkApp::InvokeOnResume(){
	try{
		OnResume();
	}catch( const char *p ){
		Die( p );
	}
	return 0;
}

int gxtkApp::InvokeOnRender(){
	try{
		if( graphics->Validate() ){
			if( !created ){
				created=1;
				OnCreate();
			}
			graphics->BeginRender();
			OnRender();
			graphics->EndRender();
		}
	}catch( const char *p ){
		Die( p );
	}
	return 0;
}

gxtkGraphics *gxtkApp::GraphicsDevice(){
	return graphics;
}

gxtkInput *gxtkApp::InputDevice(){
	return input;
}

gxtkAudio *gxtkApp::AudioDevice(){
	return audio;
}

String gxtkApp::AppTitle(){
	return "<TODO>";
}

String gxtkApp::LoadState(){
	NSUserDefaults *prefs=[NSUserDefaults standardUserDefaults];
	NSString *nsstr=[prefs stringForKey:@"gxtkAppState"];
	if( nsstr ) return tostring( nsstr );
	return "";
}

int gxtkApp::SaveState( String state ){
	NSString *nsstr=tonsstr( state );
	NSUserDefaults *prefs=[NSUserDefaults standardUserDefaults];
	[prefs setObject:nsstr forKey:@"gxtkAppState"];
	return 0;
}

String gxtkApp::LoadString( String path ){
	if( path==L"" ) return L"";
//${TEXTFILES_BEGIN}
//${TEXTFILES_END}
}

int gxtkApp::SetUpdateRate( int hertz ){

	if( updateTimer ){
		//Kill acceler/timer
		//
#if ACCELEROMETER_ENABLED
		UIAccelerometer *acc=[UIAccelerometer sharedAccelerometer];
		UIApplication *app=[UIApplication sharedApplication];
		[acc setUpdateInterval:0.0];
		[acc setDelegate:0];
		[app setIdleTimerDisabled:NO];
#endif
		[updateTimer invalidate];
		updateTimer=0;
	}

	updateRate=hertz;

	if( updateRate ){
		//These added to runloop in order? ie: updates accel before timer?
		//
#if ACCELEROMETER_ENABLED
		UIAccelerometer *acc=[UIAccelerometer sharedAccelerometer];
		UIApplication *app=[UIApplication sharedApplication];
		[acc setUpdateInterval:1.0/updateRate];
		[acc setDelegate:[app delegate]];
		[app setIdleTimerDisabled:YES];
#endif
		updateTimer=[NSTimer 
		scheduledTimerWithTimeInterval:(NSTimeInterval)(1.0/updateRate)
		target:appDelegate
		selector:@selector(updateTimerFired) userInfo:nil repeats:TRUE];
	}
	return 0;
}

int gxtkApp::MilliSecs(){
	uint64_t nanos=mach_absolute_time()-startTime;
	nanos*=timeInfo.numer;
	nanos/=timeInfo.denom;
	return nanos/1000000L;
}

int gxtkApp::Loading(){
	return 0;
}

int gxtkApp::OnCreate(){
	return 0;
}

int gxtkApp::OnUpdate(){
	return 0;
}

int gxtkApp::OnSuspend(){
	return 0;
}

int gxtkApp::OnResume(){
	return 0;
}

int gxtkApp::OnRender(){
	return 0;
}

int gxtkApp::OnLoading(){
	return 0;
}

// ***** gxtkInput *****

gxtkInput::gxtkInput(){
	memset( keyStates,0,sizeof(keyStates) );
	memset( touches,0,sizeof(touches) );
	memset( touchX,0,sizeof(touchX) );
	memset( touchY,0,sizeof(touchY) );
	accelX=accelY=accelZ=0;
}

void gxtkInput::BeginUpdate(){
}

void gxtkInput::EndUpdate(){
	for( int i=0;i<512;++i ){
		keyStates[i]&=0x100;
	}
}

void gxtkInput::OnEvent( UIEvent *event ){
	if( [event type]==UIEventTypeTouches ){
	
		UIView *view=app->appDelegate->view;

		float scaleFactor=1.0f;
		if( [view respondsToSelector:@selector(contentScaleFactor)] ){
			scaleFactor=[view contentScaleFactor];
		}

		for( UITouch *touch in [event allTouches] ){
		
			int pid;
			for( pid=0;pid<32 && touches[pid]!=touch;++pid ) {}

			switch( [touch phase] ){
			case UITouchPhaseBegan:
				if( pid!=32 ){ pid=32;break; }
				for( pid=0;pid<32 && touches[pid];++pid ){}
				if( pid==32 ) break;
				touches[pid]=touch;
				keyStates[KEY_TOUCH0+pid]=0x101;
				break;
			case UITouchPhaseEnded:
			case UITouchPhaseCancelled:
				if( pid==32 ) break;
				touches[pid]=0;
				keyStates[KEY_TOUCH0+pid]=0;
				break;
			}
			if( pid==32 ){
				printf( "***** GXTK Touch Error *****\n" );fflush( stdout );
				continue;
			}
			
			CGPoint p=[touch locationInView:view];
			p.x*=scaleFactor;
			p.y*=scaleFactor;
			
			touchX[pid]=p.x;
			touchY[pid]=p.y;
		}
	}
}

void gxtkInput::OnAcceleration( UIAcceleration *accel ){
	accelX=accel.x;
	accelY=-accel.y;
	accelZ=-accel.z;
}

int gxtkInput::KeyDown( int key ){
	if( key>0 && key<512 ){
		if( key==KEY_LMB ) key=KEY_TOUCH0;
		return keyStates[key] >> 8;
	}
	return 0;
}

int gxtkInput::KeyHit( int key ){
	if( key>0 && key<512 ){
		if( key==KEY_LMB ) key=KEY_TOUCH0;
		return keyStates[key] & 0xff;
	}
	return 0;
}

int gxtkInput::GetChar(){
	return 0;
}
	
float gxtkInput::MouseX(){
	return touchX[0];
}

float gxtkInput::MouseY(){
	return touchY[0];
}

float gxtkInput::JoyX( int index ){
	return 0;
}

float gxtkInput::JoyY( int index ){
	return 0;
}

float gxtkInput::JoyZ( int index ){
	return 0;
}

float gxtkInput::TouchX( int index ){
	return touchX[index];
}

float gxtkInput::TouchY( int index ){
	return touchY[index];
}

float gxtkInput::AccelX(){
	return accelX;
}

float gxtkInput::AccelY(){
	return accelY;
}

float gxtkInput::AccelZ(){
	return accelZ;
}

//***** gxtkAudio *****
static void CheckAL(){
	ALenum err=alGetError();
	if( err!=AL_NO_ERROR ){
		printf( "AL Error:%i\n",err );
		fflush( stdout );
	}
}

gxtkAudio::gxtkAudio():nextSource(0){
	alDistanceModel( AL_NONE );
	memset( al_sources,0,sizeof(al_sources) );
}

int gxtkAudio::AL_Source( int channel ){
	if( channel<0 || channel>=MAX_CHANNELS ) return 0;
	if( !al_sources[channel] ) alGenSources( 1,&al_sources[channel] );
	return al_sources[channel];
}

gxtkSample *gxtkAudio::LoadSample( String path ){
	return new gxtkSample( path );
}

int gxtkAudio::DestroySample( gxtkSample *sample ){
	alDeleteBuffers( 1,&sample->al_buffer );
	return 0;
}

int gxtkAudio::PlaySample( gxtkSample *sample,int channel,int flags ){
	ALint source=AL_Source( channel );
	alSourceStop( source );
	alSourcei( source,AL_BUFFER,sample->al_buffer );
	alSourcei( source,AL_LOOPING,flags ? 1 : 0 );
	alSourcePlay( source );
	return 0;
}

int gxtkAudio::StopChannel( int channel ){
	alSourceStop( AL_Source( channel ) );
	return 0;
}

int gxtkAudio::ChannelState( int channel ){
	int state=0;
	alGetSourcei( AL_Source( channel ),AL_SOURCE_STATE,&state );
	return (state==AL_PLAYING) ? 1 : 0;
}

int gxtkAudio::SetVolume( int channel,float volume ){
	alSourcef( AL_Source( channel ),AL_GAIN,volume );
	return 0;
}

int gxtkAudio::SetPan( int channel,float pan ){
	alSource3f( AL_Source( channel ),AL_POSITION,pan,0,0 );
	return 0;
}

int gxtkAudio::SetRate( int channel,float rate ){
	alSourcef( AL_Source( channel ),AL_PITCH,rate );
	return 0;
}

//***** gxtkSample *****

static const char *ReadTag( FILE *f ){
	static char buf[8];
	if( fread( buf,4,1,f )!=1 ) return "";
	buf[4]=0;
	return buf;
}

static int ReadInt( FILE *f ){
	unsigned char buf[4];
	if( fread( buf,4,1,f )!=1 ) return -1;
	return (buf[3]<<24) | (buf[2]<<16) | (buf[1]<<8) | buf[0];
}

static int ReadShort( FILE *f ){
	unsigned char buf[2];
	if( fread( buf,2,1,f )!=1 ) return -1;
	return (buf[1]<<8) | buf[0];
}

static void SkipBytes( int n,FILE *f ){
	char *p=(char*)malloc( n );
	fread( p,n,1,f );
	free(p);
}

gxtkSample::gxtkSample( String path ):al_buffer(0){

	NSString *rpath=[[NSBundle mainBundle] pathForResource:tonsstr(path) ofType:nil inDirectory:@"data"];
	if( FILE *f=fopen([rpath cStringUsingEncoding:1],"rb" ) ){

	//if( FILE *f=fopen( (T("data/")+path).ToCString<char>(),"rb" ) ){
	
	
		if( !strcmp( ReadTag( f ),"RIFF" ) ){
			int len=ReadInt( f )-8;
			if( !strcmp( ReadTag( f ),"WAVE" ) ){
				if( !strcmp( ReadTag( f ),"fmt " ) ){
					int len2=ReadInt( f );
					int comp=ReadShort( f );
					if( comp==1 ){
						int chans=ReadShort( f );
						int hertz=ReadInt( f );
						int bytespersec=ReadInt( f );
						int pad=ReadShort( f );
						int bits=ReadShort( f );
						int format=0;
						if( bits==8 && chans==1 ){
							format=AL_FORMAT_MONO8;
						}else if( bits==8 && chans==2 ){
							format=AL_FORMAT_STEREO8;
						}else if( bits==16 && chans==1 ){
							format=AL_FORMAT_MONO16;
						}else if( bits==16 && chans==2 ){
							format=AL_FORMAT_STEREO16;
						}
						if( format ){
							if( len2>16 ) SkipBytes( len2-16,f );
							for(;;){
								const char *p=ReadTag( f );
								if( feof( f ) ) break;
								int size=ReadInt( f );
								if( strcmp( p,"data" ) ){
									SkipBytes( size,f );
									continue;
								}
								char *data=(char*)malloc( size );
								if( fread( data,size,1,f )==1 ){
									alGenBuffers( 1,&al_buffer );
									alBufferData( al_buffer,format,data,size,hertz );
								}
								free( data );
								break;
							}
						}
					}
				}
			}
		}
		fclose( f );
	}
}

// ***** Ok, we have to implement app delegate ourselves *****

int gc_millis;

void gc_collect(){
	gc_millis=app->MilliSecs();
	::gc_mark();
	::gc_sweep();
	gc_millis=app->MilliSecs()-gc_millis;
}

bool app_suspended;

@implementation MonkeyAppDelegate

@synthesize window;
@synthesize view;
@synthesize viewController;

-(void)drawView:(MonkeyView*)aview{
	if( app ){
		app->InvokeOnRender();
		gc_collect();
	}
}

-(void)onEvent:(UIEvent*)event{
	if( app ){
		app->input->OnEvent( event );
	}
}

-(void)accelerometer:(UIAccelerometer*)accelerometer didAccelerate:(UIAcceleration *)acceleration{
	if( app ){
		app->input->OnAcceleration( acceleration );
	}
}

-(BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions{
    
	[[UIApplication sharedApplication] setStatusBarHidden:YES animated:NO];
	
	[viewController.view setFrame:[[UIScreen mainScreen] applicationFrame]];

    [window addSubview:viewController.view];

    [window makeKeyAndVisible];

	ALCdevice *alcDevice=alcOpenDevice( 0 );
	if( !alcDevice ){
		puts( "alcOpenDevice failed" );
		exit( -1 );
	}
	ALCcontext *alcContext=alcCreateContext( alcDevice,0 );
	if( !alcContext ){
		puts( "alcCreateContext failed" );
		exit( -1 );
	}
	if( !alcMakeContextCurrent( alcContext ) ){
		puts( "alcmakeContextCurrent failed" );
		exit( -1 );
	}

	bb_std_main( 0,0 );
	
	if( !app ) exit( 0 );

	return YES;
}

-(void)applicationWillResignActive:(UIApplication *)application{
	if( !app ) return;
	
	if( !app_suspended ){
		app_suspended=true;
		app->InvokeOnSuspend();
	}
}

-(void)applicationDidBecomeActive:(UIApplication *)application{
	if( !app ) return;
	
	if( app_suspended ){
		app_suspended=false;
		app->InvokeOnResume();
	}
}

-(void)applicationDidEnterBackground:(UIApplication *)application{
	if( !app ) return;
}

-(void)applicationWillTerminate:(UIApplication *)application{
	if( !app ) return;
}

-(void)updateTimerFired{
	if( !app || !app->updateRate ) return;
	
	app->InvokeOnUpdate();
	app->InvokeOnRender();
	gc_collect();
}

-(void)dealloc{
	[window release];
	[view release];
	[super dealloc];
}

@end
