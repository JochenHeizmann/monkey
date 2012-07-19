
#if _WIN32

static void (__stdcall* glGenBuffers)( GLsizei n,GLuint *buffers );
static void (__stdcall* glDeleteBuffers)( GLsizei n,GLuint *buffers );
static void (__stdcall* glBufferData)( GLenum target,GLsizei size,const GLvoid *data,GLenum usage );
static void (__stdcall* glBufferSubData)( GLenum target,GLsizei offset,GLsizei size,const GLvoid *data );
static void (__stdcall* glBindBuffer)( GLenum target,GLuint buffer );
static int  (__stdcall* glIsBuffer)( GLuint buffer );
static void (__stdcall* glGetBufferParameteriv)( GLenum target,GLenum pname,GLint *params );
static void (__stdcall* glCompressedTexImage2D)( GLenum target,GLint level,GLenum internalformat,GLsizei width,GLsizei height,GLint border,GLsizei imageSize,const GLvoid *data );
static void (__stdcall* glCompressedTexSubImage2D)( GLenum target,GLint level,GLint xoffset,GLint yoffset,GLsizei width,GLsizei height,GLenum format,GLsizei imageSize,const GLvoid *data );
static int _init_gl_exts_done;

static void _init_gl_exts(){
	_init_gl_exts_done=1;
	(void*&)glGenBuffers=wglGetProcAddress( "glGenBuffers" );
	(void*&)glDeleteBuffers=wglGetProcAddress( "glDeleteBuffers" );
	(void*&)glBufferData=wglGetProcAddress( "glBufferData" );
	(void*&)glBufferSubData=wglGetProcAddress( "glBufferSubData" );
	(void*&)glBindBuffer=wglGetProcAddress( "glBindBuffer" );
	(void*&)glIsBuffer=wglGetProcAddress( "glIsBuffer" );
	(void*&)glGetBufferParameteriv=wglGetProcAddress( "glGetBufferParameteriv" );
	(void*&)glCompressedTexImage2D=wglGetProcAddress( "glCompressedTexImage2D" );
	(void*&)glCompressedTexSubImage2D=wglGetProcAddress( "glCompressedTexSubImage2D" );
}

#define INIT_GL_EXTS if( !_init_gl_exts_done ) _init_gl_exts();

#else

#define INIT_GL_EXTS

#endif

DataBuffer *LoadImageData( String path,Array<int> info ){
	int width,height,depth;
	unsigned char *data=loadImage( path,&width,&height,&depth );
	if( !data || depth<1 || depth>4 ) return 0;
	
	int size=width*height;
	DataBuffer *buf=DataBuffer::Create( size*4 );
	
	unsigned char *src=data,*dst=(unsigned char*)buf->WritePointer();
	int i;
	
	switch( depth ){
	case 1:for( i=0;i<size;++i ){ *dst++=*src;*dst++=*src;*dst++=*src++;*dst++=255; } break;
	case 2:for( i=0;i<size;++i ){ *dst++=*src;*dst++=*src;*dst++=*src++;*dst++=*src++; } break;
	case 3:for( i=0;i<size;++i ){ *dst++=*src++;*dst++=*src++;*dst++=*src++;*dst++=255; } break;
	case 4:for( i=0;i<size;++i ){ *dst++=*src++;*dst++=*src++;*dst++=*src++;*dst++=*src++; } break;
	}
	
	if( info.Length()>0 ) info[0]=width;
	if( info.Length()>1 ) info[1]=height;
	
	return buf;
}

void _glGenBuffers( int n,Array<int> buffers,int offset ){
	INIT_GL_EXTS
	glGenBuffers( n,(GLuint*)&buffers[offset] );
}

void _glDeleteBuffers( int n,Array<int> buffers,int offset ){
	INIT_GL_EXTS
	glDeleteBuffers( n,(GLuint*)&buffers[offset] );
}

void _glGenTextures( int n,Array<int> textures,int offset ){
	glGenTextures( n,(GLuint*)&textures[offset] );
}

void _glDeleteTextures( int n,Array<int> textures,int offset ){
	glDeleteTextures( n,(GLuint*)&textures[offset] );
}

void _glClipPlanef( int plane,Array<Float> equation,int offset ){
	double buf[4];
	for( int i=0;i<4;++i ) buf[i]=equation[offset+i];
	glClipPlane( plane,buf );
}

void _glGetClipPlanef( int plane,Array<Float> equation,int offset ){
	double buf[4];
	glGetClipPlane( plane,buf );
	for( int i=0;i<4;++i ) equation[offset+i]=buf[i];
}

void _glFogfv( int pname,Array<Float> params,int offset ){
	glFogfv( pname,&params[offset] );
}

void _glGetFloatv( int pname,Array<Float> params,int offset ){
	glGetFloatv( pname,&params[offset] );
}

void _glGetLightfv( int target,int pname,Array<Float> params,int offset ){
	glGetLightfv( target,pname,&params[offset] );
}

void _glGetMaterialfv( int target,int pname,Array<Float> params,int offset ){
	glGetMaterialfv( target,pname,&params[offset] );
}

void _glGetTexEnvfv( int target,int pname,Array<Float> params,int offset ){
	glGetTexEnvfv( target,pname,&params[offset] );
}

void _glGetTexParameterfv( int target,int pname,Array<Float> params,int offset ){
	glGetTexParameterfv( target,pname,&params[offset] );
}

void _glLightfv( int target,int pname,Array<Float> params,int offset ){
	glLightfv( target,pname,&params[offset] );
}

void _glLightModelfv( int pname,Array<Float> params,int offset ){
	glLightModelfv( pname,&params[offset] );
}

void _glLoadMatrixf( Array<Float> params,int offset ){
	glLoadMatrixf( &params[offset] );
}

void _glMaterialfv( int target,int pname,Array<Float> params,int offset ){
	glMaterialfv( target,pname,&params[offset] );
}

void _glMultMatrixf( Array<Float> params,int offset ){
	glMultMatrixf( &params[offset] );
}

void _glTexEnvfv( int target,int pname,Array<Float> params,int offset ){
	glTexEnvfv( target,pname,&params[offset] );
}

void _glTexParameterfv( int target,int pname,Array<Float> params,int offset ){
	glTexParameterfv( target,pname,&params[offset] );
}

void _glGetIntegerv( int pname,Array<int> params,int offset ){
	glGetIntegerv( pname,&params[offset] );
}

String _glGetString( int name ){
	return String( glGetString( name ) );
}

void _glGetTexEnviv( int target,int pname,Array<int> params,int offset ){
	glGetTexEnviv( target,pname,&params[offset] );
}

void _glGetTexParameteriv( int target,int pname,Array<int> params,int offset ){
	glGetTexParameteriv( target,pname,&params[offset] );
}

void _glTexEnviv( int target,int pname,Array<int> params,int offset ){
	glTexEnviv( target,pname,&params[offset] );
}

void _glTexParameteriv( int target,int pname,Array<int> params,int offset ){
	glTexParameteriv( target,pname,&params[offset] );
}

void _glVertexPointer( int size,int type,int stride,DataBuffer *pointer ){
	glVertexPointer( size,type,stride,pointer->ReadPointer() );
}

void _glVertexPointer( int size,int type,int stride,int offset ){
	glVertexPointer( size,type,stride,(const void*)offset );
}

void _glColorPointer( int size,int type,int stride,DataBuffer *pointer ){
	glColorPointer( size,type,stride,pointer->ReadPointer() );
}

void _glColorPointer( int size,int type,int stride,int offset ){
	glColorPointer( size,type,stride,(const void*)offset );
}

void _glNormalPointer( int size,int type,int stride,DataBuffer *pointer ){
	glNormalPointer( type,stride,pointer->ReadPointer() );
}

void _glNormalPointer( int size,int type,int stride,int offset ){
	glNormalPointer( type,stride,(const void*)offset );
}

void _glTexCoordPointer( int size,int type,int stride,DataBuffer *pointer ){
	glTexCoordPointer( size,type,stride,pointer->ReadPointer() );
}

void _glTexCoordPointer( int size,int type,int stride,int offset ){
	glTexCoordPointer( size,type,stride,(const void*)offset );
}

void _glDrawElements( int mode,int count,int type,DataBuffer *indices ){
	glDrawElements( mode,count,type,indices->ReadPointer() );
}

void _glDrawElements( int mode,int count,int type,int offset ){
	glDrawElements( mode,count,type,(const GLvoid*)offset );
}

void _glBindBuffer( int target,int buffer ){
	INIT_GL_EXTS
	glBindBuffer( target,buffer );
}

int _glIsBuffer( int buffer ){
	INIT_GL_EXTS
	return glIsBuffer( buffer );
}

void _glGetBufferParameteriv( int target,int pname,Array<int> params,int offset ){
	INIT_GL_EXTS
	glGetBufferParameteriv( target,pname,&params[offset] );
}

void _glBufferData( int target,int size,DataBuffer *data,int usage ){
	INIT_GL_EXTS
	glBufferData( target,size,data->ReadPointer(),usage );
}

void _glBufferSubData( int target,int offset,int size,DataBuffer *data ){
	INIT_GL_EXTS
	glBufferSubData( target,offset,size,data->ReadPointer() );
}

void _glTexImage2D( int target,int level,int internalformat,int width,int height,int border,int format,int type,DataBuffer *pixels ){
	glTexImage2D( target,level,internalformat,width,height,border,format,type,pixels->ReadPointer() );
}

void _glTexSubImage2D( int target,int level,int xoffset,int yoffset,int width,int height,int format,int type,DataBuffer *pixels ){
	glTexSubImage2D( target,level,xoffset,yoffset,width,height,format,type,pixels->ReadPointer() );
}

void _glCompressedTexImage2D( int target,int level,int internalformat,int width,int height,int border,int imageSize,DataBuffer *pixels ){
	glCompressedTexImage2D( target,level,internalformat,width,height,border,imageSize,pixels->ReadPointer() );
}

void _glCompressedTexSubImage2D( int target,int level,int xoffset,int yoffset,int width,int height,int format,int imageSize,DataBuffer *pixels ){
	glCompressedTexSubImage2D( target,level,xoffset,yoffset,width,height,format,imageSize,pixels->ReadPointer() );
}

void _glReadPixels( int x,int y,int width,int height,int format,int type,DataBuffer *pixels ){
	glReadPixels( x,y,width,height,format,type,pixels->WritePointer() );
}

void _glFrustumf( float left,float right,float bottom,float top,float nearVal,float farVal ){
	glFrustum( left,right,bottom,top,nearVal,farVal );
}

void _glOrthof( float left,float right,float bottom,float top,float nearVal,float farVal ){
	glOrtho( left,right,bottom,top,nearVal,farVal );
}

void _glClearDepthf( float depth  ){
	glClearDepth( depth );
}

void _glDepthRangef( float nearVal,float farVal ){
	glDepthRange( nearVal,farVal );
}
