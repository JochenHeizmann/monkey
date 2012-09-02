
Import stream

Class DataStream Extends Stream

	Method New( buf:DataBuffer )
		_buf=buf
		_len=buf.Length
	End
	
	Method Length:Int()
		Return _len
	End
	
	Method Position:Int()
		Return _pos
	End
	
	Method Seek:Int( position:Int )
		_pos=Clamp( position,0,_len-1 )
		Return _pos
	End

	Method Eof:Int()
		Return _pos=_len
	End
	
	Method Close:Void()
		If _buf
			_buf=Null
			_pos=0
			_len=0
		Endif
	End
	
	Method Skip:Int( count:Int )
		If _pos+count>_len count=_len-_pos
		_pos+=count
		Return count
	End
	
	Method Read:Int( buf:DataBuffer,offset:Int,count:Int )
		If _pos+count>_len count=_len-_pos
		For Local i:=0 Until count
			buf.PokeByte offset+i,_buf.PeekByte( _pos+i )
		Next
		_pos+=count
		Return count
	End
	
	Method Write:Int( buf:DataBuffer,offset:Int,count:Int )
		If _pos+count>_len count=_len-_pos
		For Local i:=0 Until count
			_buf.PokeByte _pos+i,buf.PeekByte( offset+i )
		Next
		_pos+=count
		Return count
	End
	
	Private
	
	Field _buf:DataBuffer
	Field _pos:Int
	Field _len:Int
	
End
