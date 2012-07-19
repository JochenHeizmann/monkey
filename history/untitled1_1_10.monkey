Import mojo

Class MyApp Extends App

	Method OnCreate()
		SetUpdateRate 30
	End
	
	Method OnUpdate()
	End
	
	Method OnRender()
		Cls
		DrawText "Hold down some keys"
		Local y=16
		For Local i=1 To 255
			If KeyDown( i )
				DrawText "Key "+i+" is down.",0,
				y+=16
			Endif
		Next
	End
End

Function Main()
	New MyApp
End
