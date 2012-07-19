
//Note: OS_CHAR and OS_STR are declared in trans.system file system.cpp

namespace makemeta2{

int info_width;
int info_height;

int info_length;
int info_hertz;

char *C_STR( const String &t ){
	return t.ToCString<char>();
}

OS_CHAR *OS_STR( const String &t ){
	return t.ToCString<OS_CHAR>();
}

int get_info_png( String path ){
	if( FILE *f=fopen( path.ToCString<OS_CHAR>(),OS_STR("rb") ) ){
		unsigned char buf[24];
		int n=fread( buf,1,24,f );
		fclose( f );
		if( n==24 ){
			info_width=(buf[16]<<24)|(buf[17]<<16)|(buf[18]<<8)|(buf[19]);
			info_height=(buf[20]<<24)|(buf[21]<<16)|(buf[22]<<8)|(buf[23]);
			return 0;
		}
	}
	return -1;
}

int get_info_wav( String path ){
	if( FILE *f=fopen( path.ToCString<OS_CHAR>(),OS_STR("rb") ) ){
		fclose( f );
		info_length=0;
		info_hertz=0;
		return 0;
	}
	return -1;
}

}
