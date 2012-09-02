
// ***** tcpsocket.h *****

#if _WIN32

#include <winsock.h>

#else

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>

#define closesocket close
#define ioctlsocket ioctl

#endif

class BBTCPStream : public BBStream{
public:

	BBTCPStream();
	~BBTCPStream();
	
	bool Connect( String addr,int port );
	bool IsConnected();
	int ReadAvail();
	
	int Eof();
	void Close();
	int Read( BBDataBuffer *buffer,int offset,int count );
	int Write( BBDataBuffer *buffer,int offset,int count );
	
private:
	int _sock;
	int _state;	//0=INIT, 1=CONNECTED, 2=CLOSED, -1=ERROR
};

// ***** socket.cpp *****

BBTCPStream::BBTCPStream():_sock(-1),_state(0){
#if _WIN32
	static bool started;
	if( !started ){
		WSADATA ws;
		WSAStartup( 0x101,&ws );
		started=true;
	}
#endif
}

BBTCPStream::~BBTCPStream(){
	if( _sock>=0 ) closesocket( _sock );
}

bool BBTCPStream::Connect( String addr,int port ){

	if( _state ) return false;

	_sock=socket( AF_INET,SOCK_STREAM,IPPROTO_TCP );
	if( _sock>=0 ){
		if( hostent *host=gethostbyname( addr.ToCString<char>() ) ){
			if( char *hostip=inet_ntoa(*(struct in_addr *)*host->h_addr_list) ){
				struct sockaddr_in sa;
				sa.sin_family=AF_INET;
				sa.sin_addr.s_addr=inet_addr( hostip );
				sa.sin_port=htons( port );
				if( connect( _sock,(const sockaddr*)&sa,sizeof(sa) )>=0 ){
					_state=1;
					return true;
				}
			}
		}
		closesocket( _sock );
		_sock=-1;
	}
	return false;
}

int BBTCPStream::ReadAvail(){

	if( _state!=1 ) return 0;
	
	u_long arg;
	if( ioctlsocket( _sock,FIONREAD,&arg )>=0 ) return arg;
	
	_state=-1;
	return 0;
}

bool BBTCPStream::IsConnected(){

	return _state==1;
	
/*	
	if( _state!=1 ) return false;

	fd_set r_set,w_set,e_set;
	FD_ZERO( &r_set );
	FD_ZERO( &w_set );
	FD_ZERO( &e_set );
	FD_SET( _sock,&r_set );
	
	struct timeval tv;
	tv.tv_sec=0;
	tv.tv_usec=0;
	
	int n=select( _sock+1,&r_set,&w_set,&e_set,&tv );
	
	if( n>=0 ){
		if( !n ) return true;
		u_long arg;
		if( ioctlsocket( _sock,FIONREAD,&arg )>=0 ){
			if( arg ) return true;
			_state=2;
			return false;
		}
	}

	_state=-1;
	
	return false;
*/
}

int BBTCPStream::Eof(){
	if( _state>=0 ) return _state==2;
	return -1;
}

void BBTCPStream::Close(){
	if( _sock<0 ) return;
	if( _state==1 ) _state=2;
	closesocket( _sock );
	_sock=-1;
}

int BBTCPStream::Read( BBDataBuffer *buffer,int offset,int count ){

	if( _state!=1 ) return 0;

	int n=recv( _sock,(char*)buffer->WritePointer(offset),count,0 );
	if( n>0 || (n==0 && count==0) ) return n;
	_state=(n==0) ? 2 : -1;
	return 0;
	
}

int BBTCPStream::Write( BBDataBuffer *buffer,int offset,int count ){
	if( _state!=1 ) return 0;
	int n=send( _sock,(const char*)buffer->ReadPointer(offset),count,0 );
	if( n>=0 ) return n;
	_state=-1;
	return 0;
}
