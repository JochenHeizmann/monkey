
class BBTCPStream extends BBStream{
	
	java.net.Socket _sock;
	int _state;				//0=INIT, 1=CONNECTED, 2=CLOSED, -1=ERROR

	boolean Connect( String addr,int port ){
	
		if( _state!=0 ) return false;
		
		try{
			_sock=new java.net.Socket( addr,port );
			if( _sock.isConnected() ){
				_state=1;
				return true;
			}
		}catch( IOException ex ){
		}
		
		_state=1;
		_sock=null;
		return false;
	}
	
	boolean IsConnected(){
		return _state==1;
	}
	
	int Eof(){
		if( _state>=0 ) return (_state==2) ? 1 : 0;
		return -1;
	}
	
	void Close(){

		if( _sock==null ) return;
		
		try{
			_sock.close();
			if( _state==1 ) _state=2;
		}catch( IOException ex ){
			_state=-1;
		}
		_sock=null;
	}
	
	int Read( BBDataBuffer buffer,int offset,int count ){

		if( _state!=1 ) return 0;
		
		try{
			byte[] lock=buffer.Lock( offset,count,false,true );
			int n=_sock.getInputStream().read( lock,buffer.LockedOffset(),count );
			buffer.Unlock();
			if( n>=0 ) return n;
			_state=2;
			return 0;
		}catch( IOException ex ){
		}
		_state=-1;
		return 0;
	}
	
	int Write( BBDataBuffer buffer,int offset,int count ){

		if( _state!=1 ) return 0;
		
		try{
			byte[] lock=buffer.Lock( offset,count,true,false );
			_sock.getOutputStream().write( lock,buffer.LockedOffset(),count );
			buffer.Unlock();
			return count;
		}catch( IOException ex ){
		}
		_state=-1;
		return 0;
	}
}
