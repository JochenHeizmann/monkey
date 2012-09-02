
#include "main.h"

//${CONFIG_BEGIN}
//${CONFIG_END}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}

FILE *fopenFile( String path,const char *mode ){
#if _WIN32
	return _wfopen( path.ToCString<wchar_t>(),L"rb" );
#else
	return fopen( path.ToCString<char>(),"rb" );
#endif
}

int main( int argc,const char **argv ){

	try{
	
		bb_std_main( argc,argv );
		
	}catch( ThrowableObject *ex ){
	
		Print( "Monkey Runtime Error : Uncaught Monkey Exception" );
	
	}catch( const char *err ){
	
	}
}
