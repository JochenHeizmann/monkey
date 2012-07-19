
#REFLECT="mojo"
Import reflection

Import os
Import mojo
Import interpreter

Function Main()

	ChangeDir "../.."
#if TARGET="glfw"
	ChangeDir "../.."
#end

	Local t:=GetGlobal( "mojo.Test" )
	
	If t Print "Test="+t.GetInt() Else Print "No Test!"

	Print CurrentDir()

	Local source:=os.LoadString( "test.txt" )
	
	Run source

End
