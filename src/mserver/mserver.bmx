
Strict

Framework brl.blitz

Import brl.socketstream
Import brl.threads
Import brl.eventqueue

Import maxgui.drivers

Global mserverPort=50607
Global localhostIp=HostIp( "localhost" )

Global window:TGadget
Global textarea:TGadget

Global addText:TList=New TList
Global textMutex:TMutex=CreateMutex()

Function Error( t$ )
	WriteStdout t+"~n"
	exit_ -1
End Function

Function Print( t$ )
	If textarea
		LockMutex textMutex
		addText.AddLast t
		UnlockMutex textMutex
	Else
		WriteStdout t+"~n"
	EndIf
End Function

Function ConnectToMServer:TSocket()

	Local client:TSocket=CreateTCPSocket()
	If Not ConnectSocket( client,localhostIp,mserverPort )
?Win32
		OpenURL AppFile
?Macos
		Local f$=AppFile
		Local p=f.Find(".app/Contents/")
		If p<>-1 f=f[..p+4]
		system_ "open -n ~q"+f+"~q"
?
		Local i
		For i=0 Until 50
			CloseSocket client
			client=CreateTCPSocket()
			If ConnectSocket( client,localhostIp,mserverPort ) Exit
			Delay 100
		Next
		
		If i=50
			Error "Can't connect to MServer"
		EndIf
		
	EndIf
	Return client
End Function

ChangeDir LaunchDir

If AppArgs.length=1

	MServer

Else If AppArgs.length=2 

	Local p$=RealPath( AppArgs[1] )
	If FileType( p )<>FILETYPE_FILE Error "Invalid file '"+p+"'"
	
	Local client:TSocket=ConnectToMServer()
	
	Local stream:TSocketStream=CreateSocketStream( client,False )
	
	WriteLine stream,"NEW "+HTTPEncode( ExtractDir(p) )
	
	Local resp$=ReadLine( stream )
	
	CloseSocket client
	
	If resp.StartsWith( "OK " )
		Local port=Int(resp[3..])
		OpenURL "http://localhost:"+port+"/"+StripDir(p)
		exit_ 0
	EndIf
	
	Error "Failed to create new server."
Else
	Error "Usage: mserver [filePath]"
EndIf

Function StartGUI()
	window=CreateWindow( "MServer - Minimal Monkey HTTP Server",0,0,640,160,Null,WINDOW_TITLEBAR|WINDOW_RESIZABLE )
	textarea=CreateTextArea( 0,0,ClientWidth(window),ClientHeight(window),window,TEXTAREA_READONLY )
	SetGadgetLayout textarea,EDGE_ALIGNED,EDGE_ALIGNED,EDGE_ALIGNED,EDGE_ALIGNED
End Function

Type TServer
	Field id
	Field socket:TSocket
	Field dir$
	Field thread:TThread
End Type

Type TClient
	Field id
	Field socket:TSocket
	Field server:TServer
End Type

Type TToker
	Field text$
	
	Method SetText( text$ )
		Self.text=text
	End Method
	
	Method CParse( toke$ )
		text=text.Trim()
		If text.ToLower().StartsWith( toke )
			text=text[toke.length..]
			Return True
		EndIf
	End Method
	
	Method ParsePath$()
		text=text.Trim()
		Local i=text.Find(" ")
		If i=-1 i=text.length
		Local p$=text[..i]
		text=text[i..]
		Return p
	End Method

End Type

Function ClientThread:Object( data:Object )

	Local client:TClient=TClient( data )
	
	Local stream:TSocketStream=CreateSocketStream( client.socket,False )

	Local p$=client.server.id+":"+client.id+"> "
	
	Local toker:TToker=New TToker
	
	Repeat
	
		Local req$=ReadLine( stream )
		If Not req Exit
		
'		Print ""
'		Print p+req
		
		Local range_start=-1,range_end=-1,keep_alive,req_host$,err
		
		Repeat
			Local hdr$=ReadLine( stream )
			If Not hdr Exit
			
'			Print p+hdr

			toker.SetText hdr
			
			If toker.CParse( "host:" )
				req_host=toker.text
				Continue
			EndIf
			
			If toker.CParse( "range:" )
				If toker.CParse( "bytes=" )
					Local bits$[]=toker.text.Split( "-" )
					If bits.length=2
						range_start=Int( bits[0] )
						If bits[1] range_end=Int( bits[1] )
						Continue
					EndIf
				EndIf
				Print p+"***** Error parsing 'range' header ***** : "+hdr
				err=True
				Exit
			EndIf
			
			If toker.CParse( "connection:" )
				If toker.CParse( "keep-alive" )
					keep_alive=True
					Continue
				EndIf
				Print p+"***** Error parsing 'connection' header ***** : "+hdr
				err=True
				Exit
			EndIf
		Forever
		If err Exit
		
		
		Local get$	'file to get
		
		
		toker.SetText req
		If toker.CParse( "get" )
			Local t$=HTTPDecode(toker.ParsePath())
			If t
				get=RealPath( client.server.dir+t )
				If Not get.StartsWith( client.server.dir+"/" )
					Print p+"***** Invalid GET path ***** : "+t
					Exit
				EndIf
			EndIf
		EndIf
		
		
		If get And FileType( get )=FILETYPE_FILE
		
			Local data:Byte[]=LoadByteArray( get )
			Local length=data.length
			
			If range_start=-1
				Print p+"GET "+get
				WriteLine stream,"HTTP/1.1 200 OK"
			Else
				If range_end=-1 Or range_end>=length
					range_end=length-1
				EndIf
				data=data[range_start..range_end+1]
				
				Print p+"GET "+get+" (bytes "+range_start+"-"+range_end+")"
				WriteLine stream,"HTTP/1.1 206 Partial content"
			EndIf
			
			WriteLine stream,"Cache-Control: max-age=31556926"
			WriteLine stream,"Date: "+HTTPNow()	'should really be GMT!
			WriteLine stream,"Last-Modified: "+HTTPDate( 1,1,2000,"00:00:00" )
			WriteLine stream,"Content-Length: "+data.length
			If range_start<>-1 WriteLine stream,"Content-Range: bytes "+range_start+"-"+range_end+"/"+length
			WriteLine stream,""
			stream.WriteBytes data,data.length
		
		Else If get
	
			Print p+"404 Not Found: "+get
				
			Local data$="404 Not Found"
			WriteLine stream,"HTTP/1.1 404 Not Found"
			WriteLine stream,"Content-Length: "+data.length
			WriteLine stream,""
			stream.WriteBytes data,data.length
		
		Else
			Print p+"400 Bad Request: "+req

			Local data$="400 Bad Request"
			WriteLine stream,"HTTP/1.1 400 Not Found"
			WriteLine stream,"Content-Length: "+data.length
			WriteLine stream,""
			stream.WriteBytes data,data.length
		EndIf

		If Not keep_alive Exit
		
	Forever
	
	Print p+"BYE"

	CloseSocket client.socket
	
End Function

Function ServerThread:Object( data:Object )

	Local server:TServer=TServer( data )
	
	Local p$=server.id+"> "
	
	SocketListen server.socket

	Print p+"HTTP Server active and listening on port "+SocketLocalPort( server.socket )
	
	Local clientId
	
	Repeat
	
		Local socket:TSocket=SocketAccept( server.socket,60*1000 )
		If Not socket Continue
		
		If SocketRemoteIP( socket )<>localhostIp
			Print p+"***** Warning! *****"
			Print p+"Connection attempt by non-Local host!"
			Print p+"Remote IP="+SocketRemoteIP( socket )+", RemotePort="+SocketRemotePort( socket )
			Continue
		EndIf
		
		Local client:TClient=New TClient
		clientId:+1
		client.id=clientId
		client.socket=socket
		client.server=server
		
		Local thread:TThread=CreateThread( ClientThread,client )
		
	Forever
	
End Function

Function MServer()

	Local server:TSocket=CreateTCPSocket()
	If Not BindSocket( server,mserverPort )
		Error "BindSocket failed"
	EndIf
	
	SocketListen server

	StartGUI
		
	Print "MServer active and listening on port "+mserverPort
	
	Local serverId
	Local serverMap:TMap=New TMap				'maps dirs to servers
	
	Local toker:TToker=New TToker
	
	Repeat
	
		While PollEvent()
			Select EventID()
			Case EVENT_WINDOWCLOSE
				exit_ 0
			End Select
		Wend
		
		LockMutex textMutex
		For Local t$=EachIn addText
			AddTextAreaText textArea,t+"~n"
		Next
		addtext.Clear
		UnlockMutex textMutex
		
		
		Local client:TSocket=SocketAccept( server,100 )
		
		
		If client And SocketRemoteIP( client )<>localhostIp
			Print "***** Warning! *****"
			Print "Connection attempt by non-Local host!"
			Print "Remote IP="+SocketRemoteIP( client )+", RemotePort="+SocketRemotePort( client )
			CloseSocket client
			client=Null
		EndIf
		If Not client Continue
		
		
		Local stream:TSocketStream=CreateSocketStream( client,False )
		Local req$=ReadLine( stream )

		toker.SetText req
		
		If toker.CParse( "new " )

			Local dir$=HTTPDecode( toker.ParsePath() )
			
			Local server:TServer=TServer( serverMap.ValueForKey( dir ) )
			
			If Not server
				Local socket:TSocket=CreateTCPSocket()
				If BindSocket( socket,0 )
					server=New TServer
					serverId:+1
					server.id=serverId
					server.socket=socket
					server.dir=dir
					server.thread=CreateThread( ServerThread,server )
					serverMap.Insert dir,server
				EndIf
			EndIf
			
			If server
				WriteLine stream,"OK "+SocketLocalPort( server.socket )
			Else
				WriteLine stream,"ERR"
			EndIf
		Else
			WriteLine stream,"ERR"
		EndIf
		
		CloseSocket client
		
	Forever

End Function

Function HTTPEncode$( t$ )
	t=t.Replace( "%","%25" )
	t=t.Replace( " ","%20" )
	Return t
End Function

Function HTTPDecode$( t$ )
	t=t.Replace( "%20"," " )
	t=t.Replace( "%25","%" )
	Return t
End Function

'With help from some wedoe code in the bmx code arcs - thanks wedoe!
Function HTTPDate$( day,mon,year,time$ )

	Local t_day$=day,w_day$,t_mon$
	
	If day<10 t_day="0"+t_day

	Local d,a,m,y,tp$

	a=14-mon
	a=a/12
	y=year-a
	m=mon+(12*a)-2

	d=(day+y+(y/4)-(y/100)+(y/400)+((31*m)/12)) Mod 7
	
	Select d
	Case 0 w_day="Sun"
	Case 1 w_day="Mon"
	Case 2 w_day="Tue"
	Case 3 w_day="Wed"
	Case 4 w_day="Thu"
	Case 5 w_day="Fri"
	Case 6 w_day="Sat" 
	End Select
	
	Select mon
	Case 1 t_mon="Jan"
	Case 2 t_mon="Feb"
	Case 3 t_mon="Mar"
	Case 4 t_mon="Apr"
	Case 5 t_mon="May"
	Case 6 t_mon="Jun"
	Case 7 t_mon="Jul"
	Case 8 t_mon="Aug"
	Case 9 t_mon="Sep"
	Case 10 t_mon="Oct"
	Case 11 t_mon="Nov"
	Case 12 t_mon="Dec"
	Default Return "?"
	End Select
	
	Return w_day+", "+t_day+" "+t_mon+" "+year+" "+time+" GMT"
	
End Function

Function HTTPNow$()

	Local bits$[]=CurrentDate().Split( " " )
	If bits.length<>3 Return "?"
	
	Local day=Int( bits[0] ),mon
	
	Select bits[1]
	Case "Jan" mon=1
	Case "Feb" mon=2
	Case "Mar" mon=3
	Case "Apr" mon=4
	Case "May" mon=5
	Case "Jun" mon=6
	Case "Jul" mon=7
	Case "Aug" mon=8
	Case "Sep" mon=9
	Case "Oct" mon=10
	Case "Nov" mon=11
	Case "Dec" mon=12
	Return "?"
	End Select
	
	Local year=Int( bits[2] )
	
	Return HTTPDate( day,mon,year,CurrentTime() )

End Function
