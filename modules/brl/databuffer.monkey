
#If LANG="cpp"
Import "native/databuffer.cpp"
#Else If LANG="java"
Import "native/databuffer.java"
#Else If LANG="js"
Import "native/databuffer.js"
#Else If LANG="cs"
Import "native/databuffer.cs"
#Else If LANG="as"
Import "native/databuffer.as"
#Endif

Extern

Class BBDataBuffer

	Method Discard:Void()

	Method Length:Int() Property
	
	Method PokeByte:Void( addr:Int,value:Int )
	Method PokeShort:Void( addr:Int,value:Int )
	Method PokeInt:Void( addr:Int,value:Int )
	Method PokeFloat:Void( addr:Int,value:Float )
	
	Method PeekByte:Int( addr:Int )
	Method PeekShort:Int( addr:Int )
	Method PeekInt:Int( addr:Int )
	Method PeekFloat:Float( addr:Int )
	
Private
	
	Method _New:Bool( length:Int )
	Method _Load:Bool( path:String )

End

Public

Class DataBuffer Extends BBDataBuffer

	Method New( length:Int )
		If Not _New( length ) Error "Allocate DataBuffer failed"
	End
	
	Function Load:DataBuffer( path:String )
		Local buf:=New DataBuffer
		If buf._Load( path ) Return buf
		Return Null
	End

End
