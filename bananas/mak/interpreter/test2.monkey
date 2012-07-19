
#REFLECT="*"

Import os
Import mojo
Import reflection
'Import opengl.gles11

Function Box:Object( n:Int )
	Return New IntValue( n )
End

Function Box:Object( n:Float )
	Return New FloatValue( n )
End

Function Box:Object( n:String )
	Return New StringValue( n )
End

Function Box:Object( n:Object )
	Return n
End

Class C
	Method New()
		Print "New C!"
	End
End

Const MY_CONST=100

Class MyApp Extends App

	Field setUpdateRate:FunctionInfo
	Field loadImage:FunctionInfo
	Field drawImage:FunctionInfo
	
	Field img:Object
	
	Field X=100
	
	Method OnCreate()

		Local c:=GetClass( Self )
		If Not c Error "Can't find Self!"
		Print c.Name()
		Local f:=c.GetField( "X" )
		Print Value( f.Get( Self ) )
		
		Local t_int:=GetClass( "reflection.IntValue" )
		Local t_float:=GetClass( "reflection.FloatValue" )
		Local t_string:=GetClass( "reflection.StringValue" )
		Local t_image:=GetClass( "mojo.graphics.Image" )
		
		setUpdateRate=GetFunction( "mojo.app.SetUpdateRate",[t_int] )
		If Not setUpdateRate Error "SetUpdateRate not found"
		
		loadImage=GetFunction( "mojo.graphics.LoadImage",[t_string,t_int,t_int] )
		If Not loadImage Error "LoadImage not found"

		drawImage=GetFunction( "mojo.graphics.DrawImage",[t_image,t_float,t_float,t_int] )
		If Not drawImage Error "DrawImage not found"
		
		img=loadImage.Invoke( [Box("test.png"),Box(1),Box(0)] )

		setUpdateRate.Invoke( [Box(60)] )
	End
	
	Method OnUpdate()
	End
	
	Method OnRender()
		drawImage.Invoke( [Box(img),Box(0),Box(0),Box(0)] )
	End

End

Function Main()

	Local cnst:=GetConst( "MY_CONST" )
	Print "MY_CONST="+Value( cnst.Get() ).ToInt()
	
'	Local clas:=GetClass( "C" )
'	Local ctor:=clas.GetConstructor( [] )
'	Local obj:=ctor.Invoke( [] )

	New MyApp
	
End
