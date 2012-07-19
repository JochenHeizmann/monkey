
#If LANG="cpp" Or LANG="java"

#If LANG="cpp"
Import "native/databuffer.cpp"
#ElseIf LANG="java"
Import "native/databuffer.java"
#Endif

Extern

Class DataBuffer="RamBuffer"

	Method Size()

	Method PokeByte:Void( addr,value )
	Method PokeShort:Void( addr,value )
	Method PokeInt:Void( addr,value )
	Method PokeFloat:Void( addr,value# )
	
	Method PeekByte:Int( addr )
	Method PeekShort:Int( addr )
	Method PeekInt:Int( addr )
	Method PeekFloat:Float( addr )

#if LANG="cpp"
	Function Create:DataBuffer( size )="RamBuffer::Create"
#ElseIf LANG="java"
	Function Create:DataBuffer( size )="RamBuffer.Create"
#Endif

End

#Endif

