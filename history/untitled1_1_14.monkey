Import mojo

Class MyApp Extends App

	Method OnCreate()
		SetUpdateRate 30
	End
	
	Method OnUpdate()
	End
	
	Method OnRender()
		Cls
		DrawText "MouseX="+MouseX+", MouseY="+MouseY
		DrawCircle MouseX,MouseY,10
	End
End

Function Main()
	New MyApp
End
