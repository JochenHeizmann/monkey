
Import mojo

Class MyApp Extends App

	Field creates

	Field suspends,suspend_ms,resume_ms
	
	Field tinkle:Sound,tinkle_loop
	
	Method OnCreate()

		Local state$=LoadState()
		If state
			creates=Int( state )+1
		Else
			creates=1
		Endif

		SaveState creates

#If TARGET="flash"
		tinkle=LoadSound( "tinkle.mp3" )
#Else If TARGET="android"
		tinkle=LoadSound( "tinkle.ogg" )
#Else
		tinkle=LoadSound( "tinkle.wav" )
#EndIf
		SetUpdateRate 15
	End
	
	Method OnUpdate()

		If TouchHit(0)
			Select (Int(TouchY(0))-(14*10-14))/28
			Case 0
				tinkle_loop=Not tinkle_loop
				If tinkle_loop
					PlaySound tinkle,0,1
				Else
					StopChannel 0
				Endif
			Case 1
				Error "Runtime Error"
			Case 2
				Error ""
			End
		Endif
	End
	
	Field tx#=0,ty#=0
	
	Method OnRender()
	
		If KeyDown( KEY_RIGHT )
			tx+=.0125
		Else If KeyDown( KEY_LEFT )
			tx-=.0125
		Endif
		If KeyDown( KEY_UP )
			ty-=.0125
		Else If KeyDown( KEY_DOWN )
			ty+=.0125
		Endif
		Translate tx,ty
	
		Cls
		DrawText "Audio:          "+tinkle_loop,0,0
		DrawText "Millisecs:      "+Millisecs,0,14*1
		DrawText "Creates:        "+creates,0,14*2
		DrawText "Suspends:       "+suspends,0,14*3
		DrawText "Last suspended: "+suspend_ms,0,14*4
		DrawText "Last resumed:   "+resume_ms,0,14*5
		DrawText "tx:             "+tx,0,14*6
		DrawText "ty:             "+ty,0,14*7

		DrawText "[ *****  Click to Toggle audio  ***** ]",DeviceWidth/2,14*10,.5,.5
		DrawText "[ ***** Click for Runtime error ***** ]",DeviceWidth/2,14*12,.5,.5
		DrawText "[ *****  Click for Null error   ***** ]",DeviceWidth/2,14*14,.5,.5
		
		SetColor 255,0,0
		DrawRect 0,0,DeviceWidth,1
		DrawRect DeviceWidth-1,0,1,DeviceHeight
		DrawRect 0,DeviceHeight-1,DeviceWidth,1
		DrawRect 0,0,1,DeviceHeight-1
	End
	
	Method OnSuspend()
		suspends+=1
		suspend_ms=Millisecs
	End
	
	Method OnResume()
		resume_ms=Millisecs
	End
End

Function Main()
	New MyApp
End
