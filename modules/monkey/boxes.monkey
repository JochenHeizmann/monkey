
' Module monkey.boxes
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Class IntObject
	Field value:Int
	
	Method New( value:Int )
		Self.value=value
	End
	
	Method New( value:Float )
	   Self.value=value
	End
	
	Method ToInt:Int()
		Return value
	End	
	
	Method ToFloat:Float()
	   Return value
	End
	
	Method ToString:String()
	   Return value
	End
End

Class FloatObject
	Field value:Float
	
	Method New( value:Int )
	   Self.value=value
	End
	
	Method New( value:Float )
		Self.value=value
	End
	
	Method ToInt:Int()
	   Return value
	End
	
	Method ToFloat:Float()
		Return value
	End
	
	Method ToString:String()
	   Return value
	End
End

Class StringObject
	Field value:String
	
	Method New( value:Int )
	   Self.value=value
	End
	
	Method New( value:Float )
	   Self.value=value
	End
	
	Method New( value:String )
		Self.value=value
	End
	
	Method ToString:String()
		Return value
	End
End
