
class BBStream{

	public int Eof(){
		return 0;
	}
	
	public void Close(){
	}
	
	public int Length(){
		return 0;
	}
	
	public int Position(){
		return 0;
	}
	
	public int Seek( int position ){
		return 0;
	}
	
	public int Read( BBDataBuffer buffer,int offset,int count ){
		return 0;
	}
	
	public int Write( BBDataBuffer buffer,int offset,int count ){
		return 0;
	}
}
