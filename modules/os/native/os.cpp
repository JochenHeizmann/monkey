
// Stdcpp trans.system runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use as your own risk.

#if _WIN32

#include <windows.h>
#include <direct.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

typedef wchar_t OS_CHAR;
typedef struct _stat stat_t;

#define mkdir( X,Y ) _wmkdir( X )
#define rmdir _wrmdir
#define remove _wremove
#define rename _wrename
#define stat _wstat
#define fopen _wfopen
#define putenv _wputenv
#define getenv _wgetenv
#define system _wsystem
#define chdir _wchdir
#define getcwd _wgetcwd
#define realpath(X,Y) _wfullpath( Y,X,PATH_MAX )	//Note: first args SWAPPED to be posix-like!
#define opendir _wopendir
#define readdir _wreaddir
#define closedir _wclosedir
#define DIR _WDIR
#define dirent _wdirent

#elif __APPLE__

#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <dirent.h>

typedef char OS_CHAR;
typedef struct stat stat_t;

#elif __linux

#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

typedef char OS_CHAR;
typedef struct stat stat_t;

#endif

static String _appPath;
static Array<String> _appArgs;

static char *C_STR( const String &t ){
	return t.ToCString<char>();
}

static OS_CHAR *OS_STR( const String &t ){
	return t.ToCString<OS_CHAR>();
}

String HostOS(){
#if _WIN32
	return "winnt";
#elif __APPLE__
	return "macos";
#elif __linux
	return "linux";
#else
	return "";
#endif
}

String RealPath( String path ){
	std::vector<OS_CHAR> buf( PATH_MAX+1 );
	realpath( OS_STR( path ),&buf[0] );
	buf[buf.size()-1]=0;
	for( int i=0;i<PATH_MAX && buf[i];++i ){
		if( buf[i]=='\\' ) buf[i]='/';
		
	}
	return String( &buf[0] );
}

String AppPath(){
	if( _appPath.Length() ) return _appPath;
#if _WIN32
	OS_CHAR buf[PATH_MAX+1];
	GetModuleFileNameW( GetModuleHandleW(0),buf,PATH_MAX );
	buf[PATH_MAX]=0;
	_appPath=String( buf );
#elif __APPLE__
	char buf[PATH_MAX];
	uint32_t size=sizeof( buf );
	_NSGetExecutablePath( buf,&size );
	buf[PATH_MAX-1]=0;
	_appPath=String( buf );
#elif __linux
	char lnk[PATH_MAX],buf[PATH_MAX];
	pid_t pid=getpid();
	sprintf( lnk,"/proc/%i/exe",pid );
	int i=readlink( lnk,buf,PATH_MAX );
	if( i>0 && i<PATH_MAX ){
		buf[i]=0;
		_appPath=String( buf );
	}
#endif
	_appPath=RealPath( _appPath );
	return _appPath;
}

Array<String> AppArgs(){
	if( _appArgs.Length() ) return _appArgs;
	_appArgs=Array<String>( argc );
	for( int i=0;i<argc;++i ){
		_appArgs[i]=String( argv[i] );
	}
	return _appArgs;
}
	
int FileType( String path ){
	stat_t st;
	if( stat( OS_STR(path),&st ) ) return 0;
	switch( st.st_mode & S_IFMT ){
	case S_IFREG : return 1;
	case S_IFDIR : return 2;
	}
	return 0;
}

int FileSize( String path ){
	stat_t st;
	if( stat( OS_STR(path),&st ) ) return -1;
	return st.st_size;
}

int FileTime( String path ){
	stat_t st;
	if( stat( OS_STR(path),&st ) ) return -1;
	return st.st_mtime;
}

enum{ LATIN1,UTF8,UTF16BE,UTF16LE };

//detect unicode BOMs
static int uniEncoding( unsigned char *&p ){
	int c=*p++;
	int d=*p++;
	if( c==0xfe && d==0xff ) return UTF16BE;
	if( c==0xff && d==0xfe ) return UTF16LE;
	int e=*p++;
	if( c==0xef && d==0xbb && e==0xbf ) return UTF8;
	p-=3;
	return LATIN1;
}

static Char nextUniChar( unsigned char *&p,int encoding ){
	int c=*p++,d,e;
	switch( encoding ){
	case LATIN1:
		return c;
	case UTF8:
		if( c<128 ) return c;
		d=*p++;if( c<224 ) return (c-192)*64+(d-128);
		e=*p++;if( c<240 ) return (c-224)*4096+(d-128)*64+(e-128);
		return 0;	//ERROR!
	case UTF16BE:
		return (c<<8)|*p++;
	case UTF16LE:
		return *p++|c;
	}
}

String LoadString( String path ){

	if( FILE *fp=fopen( OS_STR(path),OS_STR("rb") ) ){
	
		std::vector<unsigned char> buf;
		
		int sz=0;
		for(;;){
			buf.resize( sz+1024 );
			int n=fread( &buf[sz],1,1024,fp );
			sz+=n;
			if( n!=1024 ) break;
		}
		buf.resize( sz );
		
		buf.push_back( 0 );
		buf.push_back( 0 );
		buf.push_back( 0 );
		
		unsigned char *p=&buf[0];
		int encoding=uniEncoding( p );
		
		if( encoding==LATIN1 ) return String( p,sz );
		
		std::vector<Char> chars;
		
		while( p<&buf[sz] ){
			chars.push_back( nextUniChar( p,encoding ) );
		}
		
		return String( &chars[0],chars.size() );
	/*
		String str;
		unsigned char buf[1024];
		while( int n=fread( buf,1,1024,fp ) ){
			str=str+String( buf,n );
		}
		fclose( fp );
		return str;
	*/
	}else{
		printf( "FOPEN 'rb' for LoadString '%s' failed\n",C_STR( path ) );
		fflush( stdout );
	}
	return T("");
}
	
int SaveString( String val,String path ){
	if( FILE *fp=fopen( OS_STR(path),OS_STR("wb") ) ){
		int n=fwrite( val.ToCString<unsigned char>(),1,val.Length(),fp );
		fclose( fp );
		return n==val.Length() ? 0 : -2;
	}else{
		printf( "FOPEN 'wb' for SaveString '%s' failed\n",C_STR( path ) );
		fflush( stdout );
	}
	return -1;
}

Array<String> LoadDir( String path ){
	std::vector<String> files;
#if _WIN32
	WIN32_FIND_DATAW filedata;
	HANDLE handle=FindFirstFileW( OS_STR(path+"/*"),&filedata );
	if( handle!=INVALID_HANDLE_VALUE ){
		do{
			String f=filedata.cFileName;
			if( f=="." || f==".." ) continue;
			files.push_back( f );
		}while( FindNextFileW( handle,&filedata ) );
		FindClose( handle );
	}else{
		printf( "FindFirstFileW for LoadDir(%s) failed\n",C_STR(path) );
		fflush( stdout );
	}
#else
	if( DIR *dir=opendir( OS_STR(path) ) ){
		while( dirent *ent=readdir( dir ) ){
			String f=ent->d_name;
			if( f=="." || f==".." ) continue;
			files.push_back( f );
		}
		closedir( dir );
	}else{
		printf( "opendir for LoadDir(%s) failed\n",C_STR(path) );
		fflush( stdout );
	}
#endif
	return Array<String>( &files[0],files.size() );
}
	
int CopyFile( String srcpath,String dstpath ){
	int err=-1;
	if( FILE *srcp=fopen( OS_STR( srcpath ),OS_STR( T("rb") ) ) ){
		err=-2;
		if( FILE *dstp=fopen( OS_STR( dstpath ),OS_STR( T("wb") ) ) ){
			err=0;
			char buf[1024];
			while( int n=fread( buf,1,1024,srcp ) ){
				if( fwrite( buf,1,n,dstp )!=n ){
					err=-3;
					break;
				}
			}
			fclose( dstp );
		}else{
			printf( "FOPEN 'wb' for CopyFile(%s,%s) failed\n",C_STR(srcpath),C_STR(dstpath) );
			fflush( stdout );
		}
		fclose( srcp );
	}else{
		printf( "FOPEN 'rb' for CopyFile(%s,%s) failed\n",C_STR(srcpath),C_STR(dstpath) );
		fflush( stdout );
	}
	return err==0;
}

int ChangeDir( String path ){
	return chdir( OS_STR(path) );
}

String CurrentDir(){
	std::vector<OS_CHAR> buf( PATH_MAX+1 );
	getcwd( &buf[0],buf.size() );
	buf[buf.size()-1]=0;
	return String( &buf[0] );
}

int CreateDir( String path ){
	mkdir( OS_STR( path ),0777 );
	return FileType(path)==2;
}

int DeleteDir( String path ){
	rmdir( OS_STR(path) );
	return FileType(path)==0;
}

int DeleteFile( String path ){
	remove( OS_STR(path) );
	return FileType(path)==0;
}

int SetEnv( String name,String value ){
//#if _WIN32
	return putenv( OS_STR( String(name)+T("=")+String(value) ) );
//#else
//	return putenv( OS_STR(name),OS_STR(value),1 );
//#endif
}

String GetEnv( String name ){
	if( OS_CHAR *p=getenv( OS_STR(name) ) ) return String( p );
	return T("");
}

int Execute( String cmd ){
#if _WIN32
	cmd=T("cmd /S /C \"")+cmd+T("\"");

	PROCESS_INFORMATION pi={0};
	STARTUPINFOW si={sizeof(si)};

	if( !CreateProcessW( 0,(WCHAR*)OS_STR(cmd),0,0,1,0,0,0,&si,&pi ) ) return -1;

	WaitForSingleObject( pi.hProcess,INFINITE );

	int res=GetExitCodeProcess( pi.hProcess,(DWORD*)&res ) ? res : -1;

	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	return res;
#else
	return system( OS_STR(cmd) );
#endif
}

void ExitApp( int retcode ){
	exit( retcode );
}

