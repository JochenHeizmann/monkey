
class BBDataBuffer{

	boolean _New( int length ){
		if( _data!=null ) return false;
		_data=ByteBuffer.allocate( length );
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
		
		System.arraycopy( bytes,0,_data.array(),0,length );
		
		return true;
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
}
