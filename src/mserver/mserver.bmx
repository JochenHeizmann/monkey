
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

Global serverId
Global serverMap:TMap=New TMap				'maps documentDirs to serverdatas

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
	
	WriteLine stream,"NEW:"+ExtractDir(p)
	
	Local resp$=ReadLine( stream )
	
	CloseSocket client
	
	If resp.StartsWith( "OK:" )
		Local port=Int(resp[3..])
		OpenURL "http://localhost:"+port+"/"+StripDir(p)
		exit_ 0
	EndIf
	
	Error "Failed to create new server."
Else
	Error "Usage: mserver [-run file]"
EndIf

Function StartGUI()
	window=CreateWindow( "MServer - Minimal Monkey HTTP Server",0,0,640,160,Null,WINDOW_TITLEBAR|WINDOW_RESIZABLE )
	textarea=CreateTextArea( 0,0,ClientWidth(window),ClientHeight(window),window,TEXTAREA_READONLY )
	SetGadgetLayout textarea,EDGE_ALIGNED,EDGE_ALIGNED,EDGE_ALIGNED,EDGE_ALIGNED
End Function

Type THTTPServerData
	Field thread:TThread
	Field socket:TSocket
	Field dir$
	Field id
End Type

Function Parse$( toke$,str$ )
	str=str.Trim()
	If str.ToLower().StartsWith( toke ) Return str[toke.Length..]
End Function

Function HTTPServerThread:Object( threadData:Object )

	Local data:THTTPServerData=THTTPServerData( threadData )
	
	Local p$=data.id+"> "
	
	SocketListen data.socket

	Print p+"HTTP Server active and listening on port "+SocketLocalPort(data.socket)

	Local client:TSocket
	Local stream:TSocketStream
	
	Repeat

		If client And Not stream
			CloseSocket client
			client=Null
		EndIf

		If Not client		
			client=SocketAccept( data.socket,60*1000 )
			If Not client Continue
			If SocketRemoteIP( client )<>localhostIp
				Print p+"***** Warning! *****"
				Print p+"Connection attempt by non-Local host!"
				Print p+"Remote IP="+SocketRemoteIP( client )+", RemotePort="+SocketRemotePort( client )
				Continue
			EndIf
			stream=CreateSocketStream( client,False )
		EndIf

		Local req$=ReadLine( stream )
		
'		Print ""
'		Print p+req
		
		Local range_start=-1,range_end=-1,keep_alive,req_host$
		
		Local hdrs:TList=New TList
		Repeat
			Local hdr$=ReadLine( stream )
			If Not hdr Exit
			hdrs.AddLast hdr
			
'			Print p+hdr
			
			Local host$=Parse( "host:",hdr )
			If host
				req_host=host.Trim()
				Continue
			EndIf
			
			Local range$=Parse( "range:",hdr )
			If range
				Local bytes$=Parse( "bytes=",range )
				Local bits$[]=bytes.Split( "-" )
				If bits.length=2
					range_start=Int( bits[0] )
					If bits[1] range_end=Int( bits[1] )
				EndIf
				Continue
			EndIf
			
			Local conn$=Parse( "connection:",hdr )
			If conn
				If conn="keep-alive" keep_alive=True
				Continue
			EndIf
		Forever
		
		Local bits$[]=req.Split( " " )
		Local cmd$=bits[0].ToLower()
		
		If cmd="get" And bits.length>=2 And bits[1].StartsWith("/")
		
			Local f$=RealPath( data.dir+bits[1] )
			
			If FileType( f )=FILETYPE_FILE
			
				If range_start=-1
			
					Local data:Byte[]=LoadByteArray(f)
					
					Print p+"GET "+f
				
					WriteLine stream,"HTTP/1.1 200 OK"
					WriteLine stream,"Cache-Control: max-age=31556926"
					WriteLine stream,"Date: "+HTTPNow()	'should really by GMT!
					WriteLine stream,"Last-Modified: "+HTTPDate( 1,1,2000,"00:00:00" )
					WriteLine stream,"Content-Length: "+data.length
					WriteLine stream,"Connection: close"
					WriteLine stream,""
					
					stream.WriteBytes data,data.length
				Else
					Local data:Byte[]=LoadByteArray(f)
					
					Local length=data.length
					
					If range_end=-1 Or range_end>=length
						range_end=length-1
					EndIf
					
					data=data[range_start..range_end+1]
					
					Print p+"GET "+f+" (bytes "+range_start+"-"+range_end+")"
					
					WriteLine stream,"HTTP/1.1 206 Partial content"
					WriteLine stream,"Cache-Control: max-age=31556926"
					WriteLine stream,"Date: "+HTTPNow()
					WriteLine stream,"Last-Modified: "+HTTPDate( 1,1,2000,"00:00:00" )
					WriteLine stream,"Content-Length: "+data.length
					WriteLine stream,"Content-Range: bytes "+range_start+"-"+range_end+"/"+length
					WriteLine stream,"Connection: close"
					WriteLine stream,""
					
					stream.WriteBytes data,data.length
				EndIf
			
			Else
				Print p+"404 Not Found: "+f
				
				Local data$="404 Not Found"
				
				WriteLine stream,"HTTP/1.1 404 Not Found"
				WriteLine stream,"Content-Length: "+data.length
				WriteLine stream,"Connection: close"
				WriteLine stream,""
				stream.WriteBytes data,data.length
			EndIf
			
		Else
			Print p+"400 Bad Request: "+req
			
			Local data$="400 Bad Request"
			
			WriteLine stream,"HTTP/1.1 400 Not Found"
			WriteLine stream,"Content-Length: "+data.length
			WriteLine stream,"Connection: close"
			WriteLine stream,""
			stream.WriteBytes data,data.length
		EndIf
		
'		If Not keep_alive stream=Null
		stream=Null
		
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
	
	Local client:TSocket
	
	Repeat
	
		If client
			CloseSocket client
			client=Null
		EndIf
		
		While Not client
		
			While PeekEvent()
				Select PollEvent()
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
			
			client=SocketAccept( server,100 )
	
			If client And SocketRemoteIP( client )<>localhostIp
				Print "***** Warning! *****"
				Print "Connection attempt by non-Local host!"
				Print "Remote IP="+SocketRemoteIP( client )+", RemotePort="+SocketRemotePort( client )
				CloseSocket client
				client=Null
			EndIf
		Wend
		
		Local stream:TSocketStream=CreateSocketStream( client,False )

		Local req$=ReadLine( stream )
		
		If req.StartsWith( "NEW:" )
		
			Local dir$=req[4..]
			
			Local data:THTTPServerData=THTTPServerData( serverMap.ValueForKey(dir) )
			
			If Not data
				Local socket:TSocket=CreateTCPSocket()
				If BindSocket( socket,0 )
					serverId:+1
					data=New THTTPServerData
					data.socket=socket
					data.dir=dir
					data.id=serverId
					data.thread=CreateThread( HTTPServerThread,data )
					serverMap.Insert dir,data
				Else
					CloseSocket socket
				EndIf
			EndIf
			
			If data
				WriteLine stream,"OK:"+SocketLocalPort( data.socket )
			Else
				WriteLine stream,"ERR"
			EndIf
		EndIf
		
		WriteLine stream,"ERR"
		
	Forever

End Function

'With help from some wedoe code in the bmx code arcs - thanks wedoe!
'
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
