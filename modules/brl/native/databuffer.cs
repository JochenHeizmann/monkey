
public class BBDataBuffer{

    public byte[] _data;
    public int _length;
    
    public BBDataBuffer(){
    }

    public BBDataBuffer( int length ){
    	_data=new byte[length];
    	_length=length;
    }
    
    public BBDataBuffer( byte[] data ){
    	_data=data;
    	_length=data.Length;
    }
    
    public bool _New( int length ){
    	if( _data!=null ) return false;
    	_data=new byte[length];
    	_length=length;
    	return true;
    }
    
    public bool _Load( String path ){
    	if( _data!=null ) return false;
    	
    	_data=MonkeyData.loadBytes( path );
    	if( _data==null ) return false;
    	
    	_length=_data.Length;
    	return true;
    }
    
    public void Discard(){
    	if( _data!=null ){
    		_data=null;
    		_length=0;
    	}
    }
    
  	public int Length(){
  		return _length;
  	}

	public void PokeByte( int addr,int value ){
		_data[addr]=(byte)value;
	}

	public void PokeShort( int addr,int value ){
		Array.Copy( System.BitConverter.GetBytes( (short)value ),0,_data,addr,2 );
	}

	public void PokeInt( int addr,int value ){
		Array.Copy( System.BitConverter.GetBytes( value ),0,_data,addr,4 );
	}

	public void PokeFloat( int addr,float value ){
		Array.Copy( System.BitConverter.GetBytes( value ),0,_data,addr,4 );
	}

	public int PeekByte( int addr ){
		return (int)(sbyte)_data[addr];
	}

	public int PeekShort( int addr ){
		return (int)System.BitConverter.ToInt16( _data,addr );
	}

	public int PeekInt( int addr ){
		return System.BitConverter.ToInt32( _data,addr );
	}

	public float PeekFloat( int addr ){
		return (float)System.BitConverter.ToSingle( _data,addr );
	}
}