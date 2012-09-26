
// ***** databuffer.h *****

class BBDataBuffer : public Object{

public:
	
	BBDataBuffer();
	~BBDataBuffer();
	
	void Discard();
	
	int Length();
	
	const void *ReadPointer( int offset );
	void *WritePointer( int offset );
	
	void PokeByte( int addr,int value );
	void PokeShort( int addr,int value );
	void PokeInt( int addr,int value );
	void PokeFloat( int addr,float value );
	
	int PeekByte( int addr );
	int PeekShort( int addr );
	int PeekInt( int addr );
	float PeekFloat( int addr );
	
	bool LoadBuffer( String path );
	bool CreateBuffer( int length );
	
	bool _New( int length );
	bool _Load( String path );

private:
	signed char *_data;
	int _length;
};

// ***** databuffer.cpp *****

//Forward refs to data functions.
FILE *fopenFile( String path,String mode );

BBDataBuffer::BBDataBuffer():_data(0),_length(0){
}

BBDataBuffer::~BBDataBuffer(){
	if( _data ) free( _data );
}

bool BBDataBuffer::_New( int length ){
	if( _data ) return false;
	_data=(signed char*)malloc( length );
	_length=length;
	return true;
}

bool BBDataBuffer::_Load( String path ){
	if( _data ) return false;
	if( FILE *f=fopenFile( path,"rb" ) ){
		const int BUF_SZ=4096;
		std::vector<void*> tmps;
		for(;;){
			void *p=malloc( BUF_SZ );
			int n=fread( p,1,BUF_SZ,f );
			tmps.push_back( p );
			_length+=n;
			if( n!=BUF_SZ ) break;
		}
		fclose( f );
		_data=(signed char*)malloc( _length );
		signed char *p=_data;
		int sz=_length;
		for( int i=0;i<tmps.size();++i ){
			int n=sz>BUF_SZ ? BUF_SZ : sz;
			memcpy( p,tmps[i],n );
			free( tmps[i] );
			sz-=n;
		}
		return true;
	}
	return false;
}

void BBDataBuffer::Discard(){
	if( !_data ) return;
	free( _data );
	_data=0;
	_length=0;
}

int BBDataBuffer::Length(){
	return _length;
}

const void *BBDataBuffer::ReadPointer( int offset ){
	return _data+offset;
}

void *BBDataBuffer::WritePointer( int offset ){
	return _data+offset;
}

void BBDataBuffer::PokeByte( int addr,int value ){
	*(_data+addr)=value;
}

void BBDataBuffer::PokeShort( int addr,int value ){
	*(short*)(_data+addr)=value;
}

void BBDataBuffer::PokeInt( int addr,int value ){
	*(int*)(_data+addr)=value;
}

void BBDataBuffer::PokeFloat( int addr,float value ){
	*(float*)(_data+addr)=value;
}

int BBDataBuffer::PeekByte( int addr ){
	return *(_data+addr);
}

int BBDataBuffer::PeekShort( int addr ){
	return *(short*)(_data+addr);
}

int BBDataBuffer::PeekInt( int addr ){
	return *(int*)(_data+addr);
}

float BBDataBuffer::PeekFloat( int addr ){
	return *(float*)(_data+addr);
}
