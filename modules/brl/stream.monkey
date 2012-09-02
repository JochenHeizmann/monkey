
Import databuffer

#If LANG="cpp"
Import "native/stream.cpp"
#Else If LANG="java"
Import "native/stream.java"
#Else If LANG="js"
Import "native/stream.js"
#Else If LANG="cs"
Import "native/stream.cs"
#Else If LANG="as"
Import "native/stream.as"
#Endif

Extern

Class BBStream

	Method Eof:Int()
	Method Close:Void()
	Method Length:Int()
	Method Position:Int()
	Method Seek:Int( position:Int )
	Method Read:Int( buf:BBDataBuffer,offset:Int,count:Int )
	Method Write:Int( buf:BBDataBuffer,offset:Int,count:Int )
	
End

Public

Class Stream

	Method New()
		If Not _tmp_buf _tmp_buf=New DataBuffer(BUF_SZ)
	End

	Method Eof:Int()
		Return _bb_stream.Eof()
	End
	
	Method Close:Void()
		_bb_stream.Close
	End
	
	Method Length:Int()
		Return _bb_stream.Length()
	End
	
	Method Position:Int()
		Return _bb_stream.Position()
	End
	
	Method Seek:Int( position:Int )
		Return _bb_stream.Seek( position )
	End
	
	Method Skip:Int( count:Int )
		Local n:=0
		While n<count
			Local t:=_bb_stream.Read( _tmp_buf,Min( count-n,BUF_SZ ),0 )
			If Not t And Eof() Throw New StreamReadError( Self )
			n+=t
		Wend
		Return n
	End
	
	Method Read:Int( buffer:DataBuffer,offset:Int,count:Int )
		Return _bb_stream.Read( buffer,offset,count )
	End
	
	Method Write:Int( buffer:DataBuffer,offset:Int,count:Int )
		Return _bb_stream.Write( buffer,offset,count )
	End
	
	Method ReadByte:Int()
		_Read 1
		Return _tmp_buf.PeekByte( 0 )
	End
	
	Method ReadShort:Int()
		_Read 2
		Return _tmp_buf.PeekShort( 0 )
	End
	
	Method ReadInt:Int()
		_Read 4
		Return _tmp_buf.PeekInt( 0 )
	End
	
	Method ReadFloat:Float()
		_Read 4
		Return _tmp_buf.PeekFloat( 0 )
	End
	
	Method ReadLine:String()
		Local buf:=New Stack<Int>
		While Not Eof()
			Local n:=Read( _tmp_buf,0,1 )
			If Not n Exit
			Local ch:=_tmp_buf.PeekByte( 0 )
			If Not ch Or ch=10 Exit
			If ch<>13 buf.Push ch
		Wend
		Return String.FromChars(buf.ToArray())
	End
	
	Method WriteByte:Void( value:Int )
		_tmp_buf.PokeByte 0,value
		_Write 1
	End
	
	Method WriteShort:Void( value:Int )
		_tmp_buf.PokeShort 0,value
		_Write 2
	End
	
	Method WriteInt:Void( value:Int )
		_tmp_buf.PokeInt 0,value
		_Write 4
	End
	
	Method WriteFloat:Void( value:Float )
		_tmp_buf.PokeFloat 0,value
		_Write 4
	End
	
	Method WriteLine:Void( str:String )
		For Local ch:=Eachin str
			WriteByte ch
		Next
		WriteByte 13
		WriteByte 10
	End

	'***** INTERNAL *****
	Method SetBBStream:Void( stream:BBStream )
		_bb_stream=stream
	End
		
	Private
	
	Const BUF_SZ=4096

	Field _bb_stream:BBStream
		
	Global _tmp_buf:DataBuffer
	
	Method _Read:Void( n:Int )
		Local i:=0
		Repeat
			i+=Read( _tmp_buf,i,n-i )
			If i=n Return
			If Eof() Throw New StreamReadError( Self )
		Forever
	End
	
	Method _Write:Void( n:Int )
		If Write( _tmp_buf,0,n )<>n Throw New StreamWriteError( Self )
	End
	
End

Class StreamError Extends Throwable

	Method New( stream:Stream )
		_stream=stream
	End
	
	Method GetStream:Stream()
		Return _stream
	End
		
	Method ToString:String() Abstract
	
	Private
	
	Field _stream:Stream
End

Class StreamReadError Extends StreamError

	Method New( stream:Stream )
		Super.New stream
	End

	Method ToString:String()
		Return "Error reading from stream"
	End
		
End

Class StreamWriteError Extends StreamError

	Method New( stream:Stream )
		Super.New stream
	End

	Method ToString:String()
		Return "Error writing to stream"
	End
	
End
