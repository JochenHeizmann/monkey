
' Module monkey.lang
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import "native/lang.${LANG}"

Extern

Function Print( message$ )="$print"
Function Error( message$ )="$error"

Class @String Extends Null="String"

	Method Length:Int() Property="$length"
	
	Method Compare:Int( str:String )="$compare"

	Method Find:Int( str:String,start:Int=0 )="$find"
	Method FindLast:Int( str:String )="$findlast"
	Method FindLast:Int( str:String,start:Int )="$findlast2"
	
	Method Trim:String()="$trim"
	Method Join:String( bits:String[] )="$join"
	Method Split:String[]( sep:String )="$split"
	Method Replace:String( substr:String,newstr:String )="$replace"
	Method ToLower:String()="$tolower"
	Method ToUpper:String()="$toupper"
	
	Method Contains:Bool( subString:String )="$contains"
	Method StartsWith:Bool( subString:String )="$startswith"
	Method EndsWith:Bool( subString:String )="$endswith"
	
	Function FromChar:String( charCode:Int )="$fromchar"

End

Class @Array Extends Null="Array"

	Method Length:Int() Property="$length"
	
	Method Resize:Int[]( length )="$resize"
	
End

Class @Object Extends Null="Object"

'	Method Equals?( obj:Object )
	
'	Method Compare( obj:Object )

End
