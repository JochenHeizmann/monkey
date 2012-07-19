
'A simple audio test app

Import mojo

Class MyApp Extends App

	Field tkey,fmt$,mfmt$
	Field shoot:Sound,shoot_chan,shoot_loop
	Field tinkle:Sound,tinkle_chan=4,tinkle_loop
	Field music$,music_on
	
	Method OnCreate()
	
#If TARGET="glfw"
		'GLFW supports WAV only
		fmt="wav"
#Elseif TARGET="html5"
		'Less than awesomely, there appears to be no 'common' format for html5!
		'Opera/Chrome appear to handle everything, but...
		'IE wont play WAV/OGG
		'FF wont play MP3/M4A
		'Let's support OGG!
		fmt="ogg"		'use M4a for IE...
#Elseif TARGET="flash"
		'Flash supports MP3, M4A online, but only MP3 embedded.
		fmt="mp3"
#Elseif TARGET="android"
		'Android supports WAV, OGG, MP3, M4A (M4A only appears to work for music though)
		'There are rumours of problems with WAV sounds so let's use OGG.
		fmt="ogg"
#Elseif TARGET="xna"
		'XNA supports WAV, MP3, WMA
		'Probably OK to use mp3 here, as audio is converted by XNA so you're not actually 
		'redistributing/decoding mp3s so probably don't need a license. But that's a lot
		'of 'probablys'...
		fmt="wav"
		mfmt="wma"
#Elseif TARGET="ios"
		'iOS supports WAV, MP3, M4A
		fmt="m4a"
#End
		If Not mfmt mfmt=fmt

		LoadStuff
				
		SetUpdateRate 15
	End
	
	Method LoadStuff()
		shoot=LoadSound( "shoot."+fmt )
		tinkle=LoadSound( "tinkle."+fmt )
		music="happy."+mfmt
	End
	
	Method OnUpdate()
	
		Local tx#=TouchX(0)*(320.0/DeviceWidth)
		Local ty#=TouchY(0)*(480.0/DeviceHeight)
	
		Local key
		If TouchHit(0)
			Local y=ty/24
			If y>=0 And y<8 key=y+1
		Endif
	
		For Local i=KEY_1 To KEY_8
			If KeyHit(i)
				key=i-KEY_1+1
				Exit
			Endif
		Next
		
		If key tkey=key-1
		
		Select key
		Case 1
			PlaySound shoot,0
		Case 2
			PlaySound shoot,shoot_chan
			shoot_chan+=1
			If shoot_chan=3 shoot_chan=0
		Case 3
			PlaySound tinkle,3
		Case 4
			PlaySound tinkle,tinkle_chan
			tinkle_chan+=1
			If tinkle_chan=6 tinkle_chan=3
		Case 5
			If shoot_loop
				StopChannel 6
			Else
				PlaySound shoot,6,True
			Endif
			shoot_loop=Not shoot_loop
		Case 6
			If tinkle_loop
				StopChannel 7
			Else
				PlaySound tinkle,7,True
			Endif
			tinkle_loop=Not tinkle_loop
		Case 7
			If music_on
				StopMusic
				music_on=False
			Else
				PlayMusic music,1
				music_on=True
			Endif
		Case 8
			'shoot.Discard		'should work with/without...
			'tinkle.Discard
			LoadStuff
		End
	End
	
	Method OnRender()

		Scale DeviceWidth/320.0,DeviceHeight/480.0
		
		Cls
		
		SetColor 0,0,255
		DrawRect 0,tkey*24,320,24
		
		Translate 5,5
		
		SetColor 255,255,255		
		DrawText "1) Play 'shoot' through channel 0",0,0
		DrawText "2) Play 'shoot' through channel 0...2",0,24
		DrawText "3) Play 'tinkle' through channel 3",0,24*2
		DrawText "4) Play 'tinkle' through channel 3...5",0,24*3
		DrawText "5) Loop/Stop 'shoot' through channel 6",0,24*4
		DrawText "6) Loop/Stop 'tinkle' through channel 7",0,24*5
		DrawText "7) Loop/Stop music",0,24*6
		DrawText "8) Reload sounds",0,24*7
		
		Local y=24*9
		For Local i=0 Until 8
			DrawText "ChannelState("+i+")="+ChannelState(i),0,y
			y+=24
		Next
		DrawText "MusicState="+MusicState(),0,y
		
	End
End

Function Main()

	New MyApp

End
