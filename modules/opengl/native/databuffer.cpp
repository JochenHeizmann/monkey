
class DataBuffer : public Object{

	public:
	
	int _length;
	unsigned char *_data;
	
	DataBuffer( int length ):_length( length ){
		_data=(unsigned char*)malloc( length );
	}
	
	~DataBuffer(){
		if( _data ) free( _data );
	}
	
	int Size(){
		return _length;
	}
	
	void Discard(){
		if( _data ){
			free( _data );
			_data=0;
			_length=0;
		}
	}
	
	void *ReadPointer(){
		return _data;
	}
	
	void *WritePointer(){
		return _data;
	}
	
	void PokeByte( int addr,int value ){
		*(_data+addr)=value;
	}
	
	void PokeShort( int addr,int value ){
		*(short*)(_data+addr)=value;
	}
	
	void PokeInt( int addr,int value ){
		*(int*)(_data+addr)=value;
	}
	
	void PokeFloat( int addr,float value ){
		*(float*)(_data+addr)=value;
	}
	
	int PeekByte( int addr ){
		return *(_data+addr);
	}
	
	int PeekShort( int addr ){
		return *(short*)(_data+addr);
	}
	
	int PeekInt( int addr ){
		return *(int*)(_data+addr);
	}
	
	float PeekFloat( int addr ){
		return *(float*)(_data+addr);
	}
	
	static DataBuffer *Create( int length ){
		return new DataBuffer( length );
	}
};
