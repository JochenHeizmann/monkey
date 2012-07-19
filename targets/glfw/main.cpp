
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <GL/glfw.h>

#include <al.h>
#include <alc.h>

#define STBI_HEADER_FILE_ONLY
#include <stb_image.c>

#undef LoadString

//For monkey main to set...
int (*runner)();

//${CONFIG_BEGIN}
//${CONFIG_END}

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
#if _WIN32
	return _wfopen( (String("data/")+path).ToCString<wchar_t>(),L"rb" );
#else
	return fopen( (String("data/")+path).ToCString<char>(),"rb" );
#endif
}

unsigned char *loadImage( String path,int *width,int *height,int *depth ){
	return stbi_load( (String("data/")+path).ToCString<char>(),width,height,depth,0 );
}

void unloadImage( unsigned char *data ){
	stbi_image_free( data );
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

	glfwSetWindowTitle( CFG_GLFW_WINDOW_TITLE );
	
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
	
#ifdef INITGLES20
	InitGLES20();
#endif
	
	try{
	
		bb_std_main( argc,argv );

		if( runner ) seh_call( runner );

	}catch( const char *err ){

		warn( ( String("Monkey runtime error: ")+err+"\n"+StackTrace() ).ToCString<char>() );
	}
	
	if( alcContext ) alcDestroyContext( alcContext );

	if( alcDevice ) alcCloseDevice( alcDevice );

	glfwTerminate();

	return 0;
}
