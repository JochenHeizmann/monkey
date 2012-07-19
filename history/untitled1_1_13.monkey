Import mojo

Class MyApp Extends App

	Method OnCreate()
		SetUpdateRate 30
	End
	
	Method OnUpdate()
	End
	
	Method OnRender()
		Cls
		DrawText "Hold down some keys",0,0
		Local y=14
		For Local i=1 To 255
			If KeyDown( i )
				DrawText "Key "+i+" is down.",0,y
				y+=14
			Endif
		Next
	End
End

Function Main()
	New MyApp
End
