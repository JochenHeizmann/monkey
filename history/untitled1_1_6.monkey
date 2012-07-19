Import mojo

Class MyApp Extends App

	Field text$

	Method OnCreate()
		SetUpdateRate 30
	End
	
	Method OnUpdate()
		Local char=GetChar()
		If char>=32
			text+=String.FromChar( char )
		Endif
	End
	
	Method OnRender()
		Cls
		DrawText text,0,0
	End
End

Function Main()
	New MyApp
End
