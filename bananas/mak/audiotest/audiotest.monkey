
'A simple audio test app
'
'Alas, audio formats are STILL a complete and utter munted mess, hence the #if stuff
'below - fix me!
Import mojo

Class MyApp Extends App

	Field tkey
	Field shoot:Sound,shoot_chan,shoot_loop
	Field tinkle:Sound,tinkle_chan=4,tinkle_loop
	
	Method OnCreate()
	
#If TARGET="flash"
		shoot=LoadSound( "shoot.mp3" )
		tinkle=LoadSound( "tinkle.mp3" )
#Else If TARGET="android"
		shoot=LoadSound( "shoot.ogg" )
		tinkle=LoadSound( "tinkle.ogg" )
#Else If TARGET="ios"
		shoot=LoadSound( "shoot.mp3" )
		tinkle=LoadSound( "tinkle.mp3" )
#Else
		shoot=LoadSound( "shoot.wav" )
		tinkle=LoadSound( "tinkle.wav" )
#Endif

		SetUpdateRate 15
	End
	
	Method OnUpdate()

		Local key
		If TouchHit(0)
			Local y=TouchY(0)/24
			If y>=0 And y<6 key=y+1
		Endif
	
		For Local i=KEY_1 To KEY_6
			If KeyHit(i)
				key=i-KEY_1+1
				Exit
			Endif
		Next
		
		If key tkey=key-1
		
		For Local i=0 Until 32
			SetChannelVolume i,1
		Next
		
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
		End
	End
	
	Method OnRender()
		Cls
		
		SetColor 0,0,255
		DrawRect 0,tkey*24,DeviceWidth,24
		
		Translate 5,5

		DrawText "1) Play 'shoot' through channel 0",0,0
		DrawText "2) Play 'shoot' through channel 0...2",0,24
		DrawText "3) Play 'tinkle' through channel 3",0,24*2
		DrawText "4) Play 'tinkle' through channel 3...5",0,24*3
		DrawText "5) Loop/Stop 'shoot' through channel 6",0,24*4
		DrawText "6) Loop/Stop 'tinkle' through channel 7",0,24*5
		
		Local y=24*7
		For Local i=0 Until 8
			DrawText "ChannelState("+i+")="+ChannelState(i),0,y
			y+=24
		Next
		
	End
End

Function Main()

	New MyApp

End