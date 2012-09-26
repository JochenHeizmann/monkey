
class BBFileStream : BBStream{

	public bool Open( String path,String mode ){
		if( _stream!=null ) return false;
		
		FileMode fmode=0;
		
		if( mode=="r" ){
			fmode=FileMode.Open;
		}else if( mode=="w" ){
			fmode=FileMode.Create;
		}else if( mode=="u" ){
			fmode=FileMode.OpenOrCreate;
		}else{
			return false;
		}
		
		_stream=MonkeyData.OpenFile( path,fmode );
		if( _stream==null ) return false;
		
		_position=_stream.Position;
		_length=_stream.Length;
		return true;
	}

	public void Close(){
		if( _stream==null ) return;
		
		_stream.Close();
		_stream=null;
		_position=0;
		_length=0;
	}
	
	public int Eof(){
		if( _stream==null ) return -1;
		
		return (_position==_length) ? 1 : 0;
	}
	
	public int Length(){
		return (int)_length;
	}
	
	public int Offset(){
		return (int)_position;
	}
	
	public int Seek( int offset ){
		try{
			_stream.Seek( offset,SeekOrigin.Begin );
			_position=_stream.Position;
		}catch( IOException ex ){
		}
		return (int)_position;
	}
		
	public int Read( BBDataBuffer buffer,int offset,int count ){
		if( _stream==null ) return 0;

		try{
			int n=_stream.Read( buffer._data,offset,count );
			_position+=n;
			return n;
		}catch( IOException ex ){
		}
		return 0;
	}
	
	public int Write( BBDataBuffer buffer,int offset,int count ){
		if( _stream==null ) return 0;
		
		try{
			_stream.Write( buffer._data,offset,count );
			_position+=count;
			if( _position>_length ) _length=_position;
			return count;
		}catch( IOException ex ){
		}
		return 0;
	}

	FileStream _stream;
	long _position,_length;
}
