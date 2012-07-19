
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <GL/glfw.h>

#include <al.h>
#include <alc.h>

#define STBI_HEADER_FILE_ONLY
#include <stb_image.c>

#include "config.h"

#undef LoadString

void (*runner)();

//${TRANSCODE_BEGIN}
void GameMain(){
	glClearColor( .5,0,1,1 );
	while( glfwGetWindowParam( GLFW_OPENED ) ){
		glClear( GL_COLOR_BUFFER_BIT );
		glfwSwapBuffers();
	}
}
//${TRANSCODE_END}

String loadString( String path ){
	if( !path.Length() ) return "";
//${TEXTFILES_BEGIN}
//${TEXTFILES_END}
}

unsigned char *loadImage( String path,int *width,int *height,int *depth ){
	return stbi_load( (String("data/")+path).ToCString<char>(),width,height,depth,0 );
}

void unloadImage( unsigned char *data ){
	stbi_image_free( data );
}

FILE *fopenFile( String path,const char *mode ){
	return fopen( (String("data/")+path).ToCString<char>(),"rb" );
}

int main( int argc,const char *argv[] ){

	if( !glfwInit() ){
		puts( "glfwInit failed" );
		exit( -1 );
	}
	
	GLFWvidmode desktopMode;
	glfwGetDesktopMode( &desktopMode );
	
	int w=WINDOW_WIDTH;
	if( !w ) w=desktopMode.Width;
	
	int h=WINDOW_HEIGHT;
	if( !h ) h=desktopMode.Height;
	
	glfwOpenWindowHint( GLFW_WINDOW_NO_RESIZE,WINDOW_NO_RESIZE );
	glfwSetWindowTitle( WINDOW_TITLE );
	
	if( !glfwOpenWindow( w,h, 0,0,0,0,0,0, WINDOW_MODE ) ){
		puts( "glfwOpenWindow failed" );
		exit( -1 );
	}

	glfwSetWindowPos( (desktopMode.Width-w)/2,(desktopMode.Height-h)/2 );	

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
	
	bb_std_main( argc,argv );
	
	if( runner ) runner();
	
	alcDestroyContext( alcContext );

	alcCloseDevice( alcDevice );

	glfwTerminate();

	return 0;
}
