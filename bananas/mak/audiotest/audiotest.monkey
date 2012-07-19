
'A simple audio test app

Import mojo

Class MyApp Extends App

	Field tkey
	Field shoot:Sound,shoot_chan,shoot_loop
	Field tinkle:Sound,tinkle_chan=4,tinkle_loop
	Field music$,music_on
	
	Method OnCreate()
	
		Local fmt$,mfmt$
	
#If TARGET="glfw"
		'GLFW supports WAV only
		fmt="wav"
#Elseif TARGET="html5"
		'Seems to be no 100% reliable format for html5!
		'IE wont play WAV(!)
		'FF wont play MP3/M4A anymore
		'Opera/Chrome appear to handle everything
		'Safari..?
		'Let's support OGG!
		fmt="ogg"
#Elseif TARGET="flash"
		'Flash supports MP3, M4A online, but only MP3 embedded.
		fmt="mp3"
#Elseif TARGET="android"
		'Android supports WAV, OGG, MP3, M4A (M4A only appears to work for music though)
		'There are rumours of problems with WAV sounds so let's use OGG.
		fmt="ogg"
#Elseif TARGET="xna"
		'XNA supports WAV, MP3
		'Probably OK to use mp3 here, as audio is converted by XNA so you're not actually 
		'redistributing/decoding mp3s so probably don't need a license. But that's a lot
		'of 'probablys'...
		fmt="wav"
#Elseif TARGET="ios"
		'iOS supports WAV, MP3, M4A
		fmt="m4a"
#End
		If Not mfmt mfmt=fmt
		
		shoot=LoadSound( "shoot."+fmt )
		tinkle=LoadSound( "tinkle."+fmt )
		
		music="happy."+mfmt
		
'		LoadSound music
'		music="shoot."+mfmt
		
		SetUpdateRate 15
	End
	
	Method OnUpdate()
	
		Local key
		If TouchHit(0)
			Local y=TouchY(0)/24
			If y>=0 And y<7 key=y+1
		Endif
	
		For Local i=KEY_1 To KEY_7
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
		End
	End
	
	Method OnRender()
		Cls
		
		SetColor 0,0,255
		DrawRect 0,tkey*24,DeviceWidth,24
		
		Translate 5,5
		
		SetColor 255,255,255		
		DrawText "1) Play 'shoot' through channel 0",0,0
		DrawText "2) Play 'shoot' through channel 0...2",0,24
		DrawText "3) Play 'tinkle' through channel 3",0,24*2
		DrawText "4) Play 'tinkle' through channel 3...5",0,24*3
		DrawText "5) Loop/Stop 'shoot' through channel 6",0,24*4
		DrawText "6) Loop/Stop 'tinkle' through channel 7",0,24*5
		DrawText "7) Loop/Stop music",0,24*6
		
		Local y=24*7
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
