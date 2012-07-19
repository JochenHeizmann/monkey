
abstract class DataBuffer{

	int Size(){ return 0; }
	
	Buffer GetBuffer(){ return null; }
}

class RamBuffer extends DataBuffer{
	
	ByteBuffer _data;
	
	RamBuffer( int length ){
		_data=ByteBuffer.allocateDirect( length );
		_data.order( ByteOrder.nativeOrder() );
	}

	int Size(){
		return _data.capacity();
	}
		
	Buffer GetBuffer(){
		return _data;
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
	
	static RamBuffer Create( int length ){
		return new RamBuffer( length );
	}
}

class IntArrayBuffer extends DataBuffer{

	IntBuffer _buffer;
	
	IntArrayBuffer( int[] arry,int offset ){
		_buffer=IntBuffer.wrap( arry,offset,arry.length-offset );
	}
	
	int Size(){
		return _buffer.capacity()*4;
	}
	
	Buffer GetBuffer(){
		return _buffer;
	}

	static IntArrayBuffer Create( int[] arry,int offset ){
		return new IntArrayBuffer( arry,offset );
	}
}

class FloatArrayBuffer extends DataBuffer{

	FloatBuffer _buffer;
	
	FloatArrayBuffer( float[] arry,int offset ){
		_buffer=FloatBuffer.wrap( arry,offset,arry.length-offset );
	}
	
	int Size(){
		return _buffer.capacity()*4;
	}
	
	Buffer GetBuffer(){
		return _buffer;
	}

	static FloatArrayBuffer Create( float[] arry,int offset ){
		return new FloatArrayBuffer( arry,offset );
	}
}
