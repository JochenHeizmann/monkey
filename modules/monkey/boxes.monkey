
' Module monkey.boxes
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Class BoolObject
	Field value:Bool
	
	Method New( value:Bool )
		Self.value=value
	End
	
	Method ToBool:Bool()
		Return value
	End
	
	Method Equals?( box:BoolObject )
		Return value=box.value
	End
End

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
	
	Method Equals?( box:IntObject )
		Return value=box.value
	End
	
	Method Compare( box:IntObject )
		Return value-box.value
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
	
	Method Equals?( box:FloatObject )
		Return value=box.value
	End
	
	Method Compare( box:FloatObject )
		If value<box.value Return -1
		Return value>box.value		
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
	
	Method Equals?( box:StringObject )
		Return value=box.value
	End
	
	Method Compare( box:StringObject )
		Return value.Compare( box.value )
	End
End

Class ArrayObject<T>
	Field value:T[]
	
	Method New( value:T[] )
		Self.value=value
	End
End
