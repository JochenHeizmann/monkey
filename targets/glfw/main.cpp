
#include "main.h"

//${CONFIG_BEGIN}
//${CONFIG_END}

#define _QUOTE(X) #X
#define _STRINGIZE( X ) _QUOTE(X)

//For monkey main to set...
int (*runner)();

//${TRANSCODE_BEGIN}
void GameMain(){
	glClearColor( .5,0,1,1 );
	while( glfwGetWindowParam( GLFW_OPENED ) ){
		glClear( GL_COLOR_BUFFER_BIT );
		glfwSwapBuffers();
	}
}
//${TRANSCODE_END}

FILE *fopenFile( String path,const char *mode ){
	if( path.ToLower().StartsWith( "monkey://data/" ) ) path=String("data/")+path.Slice(14);
#if _WIN32
	return _wfopen( path.ToCString<wchar_t>(),L"rb" );
#else
	return fopen( path.ToCString<char>(),"rb" );
#endif
}

static String extractExt( String path ){
	int i=path.FindLast( "." )+1;
	if( i && path.Find( "/",i )==-1 && path.Find( "\\",i )==-1 ) return path.Slice( i );
	return "";
}

unsigned char *loadImage( String path,int *width,int *height,int *depth ){
	FILE *f=fopenFile( path,"rb" );
	if( !f ) return 0;
	unsigned char *data=stbi_load_from_file( f,width,height,depth,0 );
	fclose( f );
	return data;
}

unsigned char *loadImage( unsigned char *data,int length,int *width,int *height,int *depth ){
	return stbi_load_from_memory( data,length,width,height,depth,0 );
}

void unloadImage( unsigned char *data ){
	stbi_image_free( data );
}

//for reading WAV file...
static const char *readTag( FILE *f ){
	static char buf[8];
	if( fread( buf,4,1,f )!=1 ) return "";
	buf[4]=0;
	return buf;
}

static int readInt( FILE *f ){
	unsigned char buf[4];
	if( fread( buf,4,1,f )!=1 ) return -1;
	return (buf[3]<<24) | (buf[2]<<16) | (buf[1]<<8) | buf[0];
}

static int readShort( FILE *f ){
	unsigned char buf[2];
	if( fread( buf,2,1,f )!=1 ) return -1;
	return (buf[1]<<8) | buf[0];
}

static void skipBytes( int n,FILE *f ){
	char *p=(char*)malloc( n );
	fread( p,n,1,f );
	free(p);
}

static unsigned char *loadSound_wav( String path,int *plength,int *pchannels,int *pformat,int *phertz ){

	if( FILE *f=fopenFile( path,"rb" ) ){
		if( !strcmp( readTag( f ),"RIFF" ) ){
			int len=readInt( f )-8;len=len;
			if( !strcmp( readTag( f ),"WAVE" ) ){
				if( !strcmp( readTag( f ),"fmt " ) ){
					int len2=readInt( f );
					int comp=readShort( f );
					if( comp==1 ){
						int chans=readShort( f );
						int hertz=readInt( f );
						int bytespersec=readInt( f );bytespersec=bytespersec;
						int pad=readShort( f );pad=pad;
						int bits=readShort( f );
						int format=bits/8;
						if( len2>16 ) skipBytes( len2-16,f );
						for(;;){
							const char *p=readTag( f );
							if( feof( f ) ) break;
							int size=readInt( f );
							if( strcmp( p,"data" ) ){
								skipBytes( size,f );
								continue;
							}
							unsigned char *data=(unsigned char*)malloc( size );
							if( fread( data,size,1,f )==1 ){
								*plength=size/(chans*format);
								*pchannels=chans;
								*pformat=format;
								*phertz=hertz;
								fclose( f );
								return data;
							}
							free( data );
						}
					}
				}
			}
		}
		fclose( f );
	}
	return 0;
}

static unsigned char *loadSound_ogg( String path,int *length,int *channels,int *format,int *hertz ){

	FILE *f=fopenFile( path,"rb" );
	if( !f ) return 0;
	
	int error;
	stb_vorbis *v=stb_vorbis_open_file( f,0,&error,0 );
	if( !v ){
		fclose( f );
		return 0;
	}
	
	stb_vorbis_info info=stb_vorbis_get_info( v );
	
	int limit=info.channels*4096;
	int offset=0,data_len=0,total=limit;

	short *data=(short*)malloc( total*sizeof(short) );
	
	for(;;){
		int n=stb_vorbis_get_frame_short_interleaved( v,info.channels,data+offset,total-offset );
		if( !n ) break;
	
		data_len+=n;
		offset+=n*info.channels;
		
		if( offset+limit>total ){
			total*=2;
			data=(short*)realloc( data,total*sizeof(short) );
		}
	}
	
	*length=data_len;
	*channels=info.channels;
	*format=2;
	*hertz=info.sample_rate;
	
	stb_vorbis_close(v);
	fclose( f );

	return (unsigned char*)data;
}

unsigned char *loadSound( String path,int *length,int *channels,int *format,int *hertz ){

	String ext=extractExt( path ).ToLower();
	
	if( ext=="wav" ) return loadSound_wav( path,length,channels,format,hertz );

	if( ext=="ogg" ) return loadSound_ogg( path,length,channels,format,hertz );
	
	return 0;
}

void unloadSound( unsigned char *data ){
	free( data );
}

ALCdevice *alcDevice;

ALCcontext *alcContext;

void warn( const char *p ){
	puts( p );
}

void fail( const char *p ){
	puts( p );
	exit( -1 );
}

int main( int argc,const char *argv[] ){

	if( !glfwInit() ){
		puts( "glfwInit failed" );
		exit( -1 );
	}

	GLFWvidmode desktopMode;
	glfwGetDesktopMode( &desktopMode );
	
	int w=CFG_GLFW_WINDOW_WIDTH;
	if( !w ) w=desktopMode.Width;
	
	int h=CFG_GLFW_WINDOW_HEIGHT;
	if( !h ) h=desktopMode.Height;
	
	glfwOpenWindowHint( GLFW_WINDOW_NO_RESIZE,CFG_GLFW_WINDOW_RESIZABLE ? GL_FALSE : GL_TRUE );
	
	if( !glfwOpenWindow( w,h, 0,0,0,0,CFG_OPENGL_DEPTH_BUFFER_ENABLED ? 32 : 0,0,CFG_GLFW_WINDOW_FULLSCREEN ? GLFW_FULLSCREEN : GLFW_WINDOW  ) ){
		fail( "glfwOpenWindow failed" );
	}

	glfwSetWindowPos( (desktopMode.Width-w)/2,(desktopMode.Height-h)/2 );	

	glfwSetWindowTitle( _STRINGIZE(CFG_GLFW_WINDOW_TITLE) );
	
	if( (alcDevice=alcOpenDevice( 0 )) ){
		if( (alcContext=alcCreateContext( alcDevice,0 )) ){
			if( alcMakeContextCurrent( alcContext ) ){
				//alc all go!
			}else{
				warn( "alcMakeContextCurrent failed" );
			}
		}else{
			warn( "alcCreateContext failed" );
		}
	}else{
		warn( "alcOpenDevice failed" );
	}
	
#if INIT_GL_EXTS
	Init_GL_Exts();
#endif

	try{
	
		bb_std_main( argc,argv );

		if( runner ) runner();
		
	}catch( ThrowableObject *ex ){
	
		Print( "Monkey Runtime Error : Uncaught Monkey Exception" );

	}catch( const char *err ){

	}
	
	if( alcContext ) alcDestroyContext( alcContext );

	if( alcDevice ) alcCloseDevice( alcDevice );

	glfwTerminate();

	return 0;
}
