
' Module monkey.math
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Const PI#=3.14159265
Const TWOPI#=6.28318531
Const HALFPI#=1.57079633

Extern

'These versions in degrees...
Function Sin#( n# )="$sin"
Function Cos#( n# )="$cos"
Function Tan#( n# )="$tan"

Function ASin#( n# )="$asin"
Function ACos#( n# )="$acos"
Function ATan#( n# )="$atan"
Function ATan2#( x#,y# )="$atan2"

'TODO: radian version
'Function Sinr#( n# )="$sin"
'Function Cosr#( n# )="$cos"
'Function Tanr#( n# )="$tan"

'Function ASinr#( n# )="$asin"
'Function ACosr#( n# )="$acos"
'Function ATanr#( n# )="$atan"
'Function ATan2r#( x#,y# )="$atan2"

Function Sqrt#( n# )="$sqrt"
Function Floor#( n# )="$floor"
Function Ceil#( n# )="$ceil"
Function Log#( n# )="$log"

Function Pow#( x#,y# )="$pow"

Public

Function Sgn( x )
	If x<0 Return -1
	Return x>0
End

Function Abs( x )
	If x>=0 Return x
	Return -x
End

Function Min( x,y )
	If x<y Return x
	Return y
End

Function Max( x,y )
	If x>y Return x
	Return y
End

Function Clamp( n,min,max )
	If n<min Return min
	If n>max Return max
	Return n
End

Function Sgn#( x# )
	If x<0 Return -1
	If x>0 Return 1
	Return 0
End

Function Abs#( x# )
	If x>=0 Return x
	Return -x
End

Function Min#( x#,y# )
	If x<y Return x
	Return y
End

Function Max#( x#,y# )
	If x>y Return x
	Return y
End

Function Clamp#( n#,min#,max# )
	If n<min Return min
	If n>max Return max
	Return n
End
