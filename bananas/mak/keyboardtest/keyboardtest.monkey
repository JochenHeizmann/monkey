
Import mojo

Class C
	Field t
End

Class MyApp Extends App

	Field str$,old$
	Field enabled?

	Method OnCreate()
		SetUpdateRate 15
	End
	
	Method OnUpdate()
		If enabled
			Local char=GetChar()
			If char 
				Print "char="+char
				If char>=32
					str+=String.FromChar( char )
				Else
					Select char
					Case 8
						str=str[..-1]
					Case 13
						enabled=False
						DisableKeyboard
					Case 27
						str=old
						enabled=False
						DisableKeyboard
					End
				Endif
			Endif
		Else
			If KeyHit( KEY_LMB )
				old=str
				str=""
				enabled=True
				EnableKeyboard
			Endif
		Endif
	End
	
	Method OnRender()
		Cls
		Scale 2,2
		DrawText "Text: "+str,0,0
		If Not enabled DrawText "Click anywhere to type text...",0,20
	End

End

Function Main()
	New MyApp
End

