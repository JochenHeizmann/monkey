
Import stream

#If LANG="cpp"
Import "native/tcpstream.cpp"
#Elseif LANG="java"
Import "native/tcpstream.java"
#Endif

Extern

Class BBTCPStream Extends BBStream

	Method Connect:Bool( address:String,port:Int )
	
	Method IsConnected:Bool()
	
	Method ReadAvail:Int()

End

Public

Class TCPStream Extends Stream

	Method New()
		_stream=New BBTCPStream
		Super.SetBBStream _stream
	End

	Method Connect:Bool( address:String,port:Int )
		Return _stream.Connect( address,port )
	End
	
	Method IsConnected:Bool()
		Return _stream.IsConnected()
	End
	
	Method ReadAvail:Int()
		Return _stream.ReadAvail()
	End
	
	'***** INTERNAL *****
	Method New( stream:BBTCPStream )
		_stream=stream
	End
	
	Private
	
	Field _stream:BBTCPStream

End
