
#If LANG<>"cpp" And LANG<>"java"
#Error "This target does not support TCP streams"
#Endif

Import mojo

Import brl.tcpstream

Class MyApp Extends App

	Field sx:Int,sy:Int
	Field mx:Int,my:Int

	Field lines:=New StringStack
	
	Method OnCreate()
		Local stream:=New TCPStream
		
		If Not stream.Connect( "www.monkeycoder.co.nz",80 ) Error "Can't connect to monkeycoder!"
		
		stream.WriteLine "GET / HTTP/1.0"
		stream.WriteLine "Host: www.blitzbasic.com"
		stream.WriteLine ""
		Local n:=0
		While Not stream.Eof()
			lines.Push stream.ReadLine()
		Wend
		stream.Close
		
		SetUpdateRate 60

	End
	
	Method OnUpdate()
		If MouseDown( 0 ) And Not MouseHit( 0 )
			sx+=MouseX-mx
			sy+=MouseY-my
		Endif
		mx=MouseX
		my=MouseY
	End

	Method OnRender()
		Cls
		Translate 0,sy
		For Local i:=0 Until lines.Length
			DrawText lines.Get(i),0,i*12
		Next
	End	
End

Function Main()
	New MyApp
End
