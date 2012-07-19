
'Little app to dump app decls in your app!

'#REFLECTION_FILTER="appdecls|mojo*"
Import reflection
Import mojo
Import os

Class Test
	Method New()
		Print "New Test object!"
	End
End

Function Attribs$( attrs )
	Local t$
	If attrs & ATTRIBUTE_EXTERN t+=" Extern"
	If attrs & ATTRIBUTE_PRIVATE t+=" Private"
	If attrs & ATTRIBUTE_ABSTRACT t+=" Abstract"
	If attrs & ATTRIBUTE_FINAL t+=" Final"
	If attrs & ATTRIBUTE_INTERFACE t+=" Interface"
	Return t
End

Function Ret$( t:ClassInfo )
	If t Return ":"+t.Name
	Return ":Void"
End

Function Args$( t:ClassInfo[] )
	Local p:=""
	For Local c:=Eachin t
		If p p+=","
		p+=c.Name
	Next
	Return "("+p+")"
End

Function AppDecls()
	For Local info:=Eachin GetConsts()
		Print "Const "+info.Name+Ret( info.Type )+Attribs( info.Attributes() )
	Next
	For Local info:=Eachin GetGlobals()
		Print "Global "+info.Name+Ret( info.Type )+Attribs( info.Attributes() )
	Next
	For Local info:=Eachin GetFunctions()
		Print "Function "+info.Name+Ret( info.ReturnType )+Args( info.ParameterTypes )+Attribs( info.Attributes() )
	Next
	For Local cinfo:=Eachin GetClasses()
		Print "Class "+cinfo.Name+Attribs( cinfo.Attributes() )
		For Local info:=Eachin cinfo.GetConstructors()
			Print "  Method New"+Args( info.ParameterTypes )+Attribs( info.Attributes() )
		Next
		For Local info:=Eachin cinfo.GetConsts()
			Print "  Const "+info.Name+Ret( info.Type )+Attribs( info.Attributes() )
		Next
		For Local info:=Eachin cinfo.GetGlobals()
			Print "  Global "+info.Name+Ret( info.Type )+Attribs( info.Attributes() )
		Next
		For Local info:=Eachin cinfo.GetFields()
			Print "  Field "+info.Name+Ret( info.Type )+Attribs( info.Attributes() )
		Next
		For Local info:=Eachin cinfo.GetMethods()
			Print "  Method "+info.Name+Ret( info.ReturnType )+Args( info.ParameterTypes )+Attribs( info.Attributes() )
		Next
		For Local info:=Eachin cinfo.GetFunctions()
			Print "  Method "+info.Name+Ret( info.ReturnType )+Args( info.ParameterTypes )+Attribs( info.Attributes() )
		Next
	Next
End

Function Main()

	AppDecls
	
	Local clas:=GetClass( "reflectiontest.Test" )
	Local inst:=clas.NewInstance()
	If Not Test( inst ) Error "Oops!"
	
End
