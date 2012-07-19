
Import mojo

Class MyApp Extends App

	Field suspend_ms,resume_ms
	
	Method OnCreate()
		SetUpdateRate 15
	End

	Method OnRender()
		Cls
		DrawText "Milisecs:       "+Millisecs,0,0
		DrawText "Last suspended: "+suspend_ms,0,14
		DrawText "Last resumed:   "+resume_ms,0,28
	End
	
	Method OnSuspend()
		suspend_ms=Millisecs
	End
	
	Method OnResume()
		resume_ms=Millisecs
	End
End

Function Main()
	New MyApp
End
