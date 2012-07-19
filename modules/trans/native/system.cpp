
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

typedef wchar_t OS_CHAR;
typedef struct _stat stat_t;

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

class bbSystem : public Object{
 
	String hostos;
	String appfile;
	Array<String> appargs;

public:

	char *C_STR( const String &t ){
		return t.ToCString<char>();
	}

	OS_CHAR *OS_STR( const String &t ){
		return t.ToCString<OS_CHAR>();
	}
	
#if _WIN32

	bbSystem(){
		hostos=String( "winnt" );
		OS_CHAR buf[PATH_MAX+1];
		GetModuleFileNameW( GetModuleHandleW(0),buf,PATH_MAX );
		buf[PATH_MAX]=0;
		appfile=Realpath( String( buf ) );
	}

	int setenv( const OS_CHAR *name,const OS_CHAR *value,int overwrite ){
		if( !overwrite && getenv( name ) ) return -1;
		return putenv( OS_STR( String(name)+T("=")+String(value) ) );
	}

	virtual int Alert( String msg,int kind ){
		int n=MessageBoxW( 0,OS_STR( msg ),L"Alert!",MB_OK );
		return 1;
	}
	
#elif __APPLE__

	bbSystem(){
		hostos=String( "macos" );
		char buf[PATH_MAX];
		uint32_t size=sizeof( buf );
		_NSGetExecutablePath( buf,&size );
		buf[PATH_MAX-1]=0;
		appfile=Realpath( String( buf ) );
	}
	
	virtual int Alert( String msg,int kind ){
		printf( "Alert! %s\n",C_STR( msg ) );
		fflush( stdout );
		return 1;
	}
	
#elif __linux

	bbSystem(){
		hostos=String( "linux" );
		char lnk[PATH_MAX],buf[PATH_MAX];
		pid_t pid=getpid();
		sprintf( lnk,"/proc/%i/exe",pid );
		int i=readlink( lnk,buf,PATH_MAX );
		if( i>0 && i<PATH_MAX ){
			buf[i]=0;
			appfile=Realpath( String( buf ) );
		}
	}
	
	virtual int Alert( String msg,int kind ){
		printf( "Alert! %s\n",C_STR( msg ) );
		fflush( stdout );
		return 1;
	}
	
#endif

	//***** UTIL *****
	
	virtual String HostOS(){
		return hostos;
	}
	
	virtual String AppPath(){
		return appfile;
	}
	
	virtual Array<String> AppArgs(){
		if( !appargs.Length() ){
			appargs=Array<String>( argc );
			for( int i=0;i<argc;++i ){
				appargs[i]=String( argv[i] );
			}
		}
		return appargs;
	}
	
	virtual int ExitApp( int retcode ){
		exit( retcode );
		return 0;
	}
	
	virtual int FileType( String path ){
		stat_t st;
		if( stat( OS_STR(path),&st ) ) return 0;
		switch( st.st_mode & S_IFMT ){
		case S_IFREG : return 1;
		case S_IFDIR : return 2;
		}
		return 0;
	}
	
	virtual int FileSize( String path ){
		stat_t st;
		if( stat( OS_STR(path),&st ) ) return -1;
		return st.st_size;
	}
	
	virtual int FileTime( String path ){
		stat_t st;
		if( stat( OS_STR(path),&st ) ) return -1;
		return st.st_mtime;
	}
	
	virtual String LoadString( String path ){
		if( FILE *fp=fopen( OS_STR(path),OS_STR( "rb" ) ) ){
			String str;
			unsigned char buf[1024];
			while( int n=fread( buf,1,1024,fp ) ){
				str=str+String( buf,n );
			}
			fclose( fp );
			return str;
		}else{
			printf( "FOPEN 'rb' for LoadString '%s' failed\n",C_STR( path ) );
			fflush( stdout );
		}
		return T("");
	}
		
	virtual int SaveString( String val,String path ){
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
	
	virtual Array<String> LoadDir( String path ){
		std::vector<String> files;
#if _WIN32
		WIN32_FIND_DATAW filedata;
		HANDLE handle=FindFirstFileW( OS_STR(path+"/*"),&filedata );
		if( handle!=INVALID_HANDLE_VALUE ){
			do{
				files.push_back( filedata.cFileName );
			}while( FindNextFileW( handle,&filedata ) );
			FindClose( handle );
		}else{
			printf( "FindFirstFileW for LoadDir(%s) failed\n",C_STR(path) );
			fflush( stdout );
		}
#else
		if( DIR *dir=opendir( OS_STR(path) ) ){
			while( dirent *ent=readdir( dir ) ){
				files.push_back( String( ent->d_name ) );
			}
			closedir( dir );
		}else{
			printf( "opendir for LoadDir(%s) failed\n",C_STR(path) );
			fflush( stdout );
		}
#endif
		return Array<String>( &files[0],files.size() );
	}
	
	virtual int CopyFile( String srcpath,String dstpath ){
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
		return err;
	}

	//***** POSIX FileSystem stuff *****

	virtual String Realpath( String path ){
		std::vector<OS_CHAR> buf( PATH_MAX+1 );
		realpath( OS_STR( path ),&buf[0] );
		buf[buf.size()-1]=0;
		for( int i=0;i<PATH_MAX && buf[i];++i ){
			if( buf[i]=='\\' ) buf[i]='/';
			
		}
		return String( &buf[0] );
	}
	
	virtual int Chdir( String path ){
		return chdir( OS_STR(path) );
	}
	
	virtual String Getcwd(){
		std::vector<OS_CHAR> buf( PATH_MAX+1 );
		getcwd( &buf[0],buf.size() );
		buf[buf.size()-1]=0;
		return String( &buf[0] );
	}

	virtual int Mkdir( String path ){
		return mkdir( OS_STR( path ),0777 );
	}
	
	virtual int Rmdir( String path ){
		return rmdir( OS_STR(path) );
	}
	
	virtual int Remove( String path ){
		return remove( OS_STR(path) );
	}
	
	virtual int Setenv( String name,String value ){
		return setenv( OS_STR(name),OS_STR(value),1 );
	}

	virtual String Getenv( String name ){
		if( OS_CHAR *p=getenv( OS_STR(name) ) ) return String( p );
		return T("");
	}
	
#if _WIN32
	//
	//OK, we have to do this ourselves because mingw's (or someones...) system() is borked.
	//It doesn't handle cmds with more than one quoted arg at all well...
	//
	virtual int System( String cmd ){
		
		cmd=T("cmd /S /C \"")+cmd+T("\"");

		PROCESS_INFORMATION pi={0};
		STARTUPINFOW si={sizeof(si)};

		if( !CreateProcessW( 0,(WCHAR*)OS_STR(cmd),0,0,1,0,0,0,&si,&pi ) ){
			return -1;
		}

		WaitForSingleObject( pi.hProcess,INFINITE );

		int res=GetExitCodeProcess( pi.hProcess,(DWORD*)&res ) ? res : -1;

		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );

		return res;
	}
#else
	virtual int System( String cmd ){
		return system( OS_STR(cmd) );
	}
#endif
};
