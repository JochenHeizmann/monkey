
class BBDataBuffer{

	boolean _New( int length ){
		if( _data!=null ) return false;
//		_data=ByteBuffer.allocate( length );
		_data=ByteBuffer.allocateDirect( length );
		_data.order( ByteOrder.nativeOrder() );
		_length=length;
		return true;
	}
	
	boolean _Load( String path ){
		if( _data!=null ) return false;
	
		byte[] bytes=MonkeyData.loadBytes( path );
		if( bytes==null ) return false;
		
		int length=bytes.length;
		
		if( !_New( length ) ) return false;
		
		byte[] lock=Lock( 0,length,false,true );
		System.arraycopy( bytes,0,lock,LockedOffset(),length );
		Unlock();
		
		return true;
	}
	
	byte[] Lock( int offset,int count,boolean read,boolean write ){
		if( _locked!=null ) return null;
		if( _data.isDirect() ){
			_locked=new byte[count];
			_lockedOffset=0;
			if( read ){
				_data.mark();
				_data.position( offset );
				_data.get( _locked,0,count );
				_data.reset();
			}
		}else{
			_locked=_data.array();
			_lockedOffset=_data.arrayOffset()+offset;
		}
		_lockOffset=offset;
		_lockCount=count;
		_writeLock=write;
		return _locked;
	}
	
	int LockedOffset(){
		return _lockedOffset;
	}
	
	void Unlock(){
		if( _locked==null ) return;
		if( _writeLock && _data.isDirect() ){
			_data.mark();
			_data.position( _lockOffset );
			_data.put( _locked,0,_lockCount );
			_data.reset();
		}
		_lockedOffset=0;
		_locked=null;
	}
	
	int Length(){
		return _length;
	}
	
	void Discard(){
		if( _data==null ) return;
		_data=null;
		_length=0;
	}
		
	void PokeByte( int addr,int value ){
		_data.put( addr,(byte)value );
	}
	
	void PokeShort( int addr,int value ){
		_data.putShort( addr,(short)value );
	}
	
	void PokeInt( int addr,int value ){
		_data.putInt( addr,value );
	}
	
	void PokeFloat( int addr,float value ){
		_data.putFloat( addr,value );
	}
	
	int PeekByte( int addr ){
		return _data.get( addr );
	}
	
	int PeekShort( int addr ){
		return _data.getShort( addr );
	}
	
	int PeekInt( int addr ){
		return _data.getInt( addr );
	}
	
	float PeekFloat( int addr ){
		return _data.getFloat( addr );
	}

	ByteBuffer GetByteBuffer(){
		return _data;
	}
	
	ByteBuffer _data;
	int _length;
	
	byte[] _locked;
	int _lockedOffset;
	
	int _lockOffset;
	int _lockCount;
	boolean _writeLock;
}
