
class DataBuffer : public Object{

	public:
	
	virtual int Size(){ return 0; }
	
	virtual void *ReadPointer(){ return 0; }

	virtual void *WritePointer(){ return 0; }
};

class RamBuffer : public DataBuffer{

	public:
	
	int _length;
	unsigned char *_data;
	
	RamBuffer( int length ):_length( length ){
		_data=(unsigned char*)malloc( length );
	}
	
	~RamBuffer(){
		free( _data );
	}
	
	int Size(){
		return _length;
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
	
	static RamBuffer *Create( int length ){
		return new RamBuffer( length );
	}
};

class IntArrayBuffer : public DataBuffer{

	Array<int> _data;
	int _offset;
	
	public:
	
	IntArrayBuffer( Array<int> data,int offset ):_data( data ),_offset( offset ){
	}
	
	void mark(){
		gc_mark( _data );
	}
	
	int Size(){
		return (_data.Length()-_offset) * sizeof(Float);
	}

	void *ReadPointer(){
		return &_data[_offset];
	}
	
	static IntArrayBuffer *Create( Array<int> data,int offset ){
		return new IntArrayBuffer( data,offset );
	}	
};

class FloatArrayBuffer : public DataBuffer{

	Array<Float> _data;
	int _offset;
	
	public:
	
	FloatArrayBuffer( Array<Float> data,int offset ):_data( data ),_offset( offset ){
	}
	
	void mark(){
		gc_mark( _data );
	}
	
	int Size(){
		return (_data.Length()-_offset) * sizeof(Float);
	}

	void *ReadPointer(){
		return &_data[_offset];
	}
	
	static FloatArrayBuffer *Create( Array<Float> data,int offset ){
		return new FloatArrayBuffer( data,offset );
	}	
};
