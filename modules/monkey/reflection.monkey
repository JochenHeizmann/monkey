
'Values...

Class Value
	Method ToInt()
	Method ToFloat#()
	Method ToSring$()
	Method ToObject:Object()
	Method ToArray:Value[]()
End

Class IntValue Extends Value
	Method New( value )
End

Class FloatValue Extends Value
	Method New( value# )
End

Class StringValue Extends Value
	Method New( value$ )
End

Class ObjectValue Extends Value
	Method New( value:Object,clas:Class )
End

Class ArrayValue Extends Value
	Method New( value:Value[] )
End

'Decls...

Class Decl
	Method Ident$()
	Method Type$()
	Method Attrs()
	Method Meta$()
	Method Scope:Scope()
End	

Class Const Extends Decl
	Method Get:Value()
End

Class Field Extends Decl
	Method Set( value:Value,instance:Value )
	Method Get:Value( instance:Value )
End

Class Global Extends Decl
	Method Set( value:Value )
	Method Get:Value()
End

Class Scope Extends Decl
End

Class Method Extends Scope
	Method Args:Decl[]()
	Method Invoke:Value( args:Value[],instance:Value )
End

Class Function Extends Scope
	Method Args:Decl[]()
	Method Invoke:Value( args:Value[] )
End

Class Class Extends Scope
	Method IsInterface()
	Method Super:Class()
	Method Implements:Class[]()
	Method Consts:Const[]
	Method Fields:Field[]
	Method Methods:Method[]
	Method Globals:Global[]
	Method Functions:Function[]
	Method NewInstance:ObjectValue()
End

Class Module Extends Scope
	Method Consts:Const[]
	Method Classes:Class[]
	Method Globals:Const[]
	Method Functions:Function[]
End

Function Modules:Module[]
	Return _modules
End

'---- demo code -----

'entity.monkey

Import mojo

Class Entity
	Field x#,y#,z#
	
	Method Update( time# )
	End
	
	Method Render( context:Object )
	End
	
	Method EnumLights:Entity[]()
	End
	
	Method CullLights( lights:Entity[] )
	End
	
End

Class Light Extends Entity
End

Function UpdateWorld()
End

Function RenderWorld()
End

'----- resultant glue code -----

Import mojo

Class p_x1 Extends Field
	Method New()
		Super.New "x","Float"
	End
	Method Set( value:Value,instance:Value )
		Entity(instance.ToObect()).x=value.ToFloat()
	End
	Method Get:Value( instance:Value )
		Return New FloatValue( Entity(instance.ToObject()).x )
	End
End

Class p_y1 Extends Field
	Method New()
		Super.New "y","Float"
	End
	Method Set( value:Value,instance:Value )
		Entity(instance.ToObect()).y=value.ToFloat()
	End
	Method Get:Value( instance:Value )
		Return New FloatValue( Entity(instance.ToObject()).y )
	End
End

Class p_z1 Extends Field
	Method New()
		Super.New "z","Float"
	End
	Method Set( value:Value,instance:Value )
		Entity(instance.ToObect()).z=value.ToFloat()
	End
	Method Get:Value( instance:Value )
		Return New FloatValue( Entity(instance.ToObject()).z )
	End
End

Class m_Update1 Extends Method
	Method New()
		Super.New "Update","Int"
		args=[New Decl("time","float")]
	End
	Method Invoke:Value( args:Value[],instance:Value )
		Return New IntValue( Entity(instance.ToObject()).Update(args[0].ToFloat()) )
	End
End

Class m_Render1 Extends Method
	Method New()
		Super.New "Render","Int"
		args=[New Decl("context","Object")]
	End
	Method Invoke:Value( args:Value[],instance:Value )
		Return New IntValue( Entity(instance.ToObject()).Render(args[0].ToObject()) )
	End
End

Class _m_EnumLights1 Extends Method
	Method New()
		Super.New "EnumLights","Int"
	End
	Method Invoke:Value( args:Value[],instance:Value )
		Local t:=Entity(instance).EnumLights()
		Local r:=New Value[t.Length]
		For Local i=0 Until t.Length
			r[i]=New ObjectValue( t[i],c_Light1 )
		End
		Return New ArrayValue( r )
	End
	

Class c_Entity1 Extends Class
	Method New()
		Super.New "Entity","Class"
		fields=[New p_x1,New p_y1,New p_z1]
		methods=[New m_update1,New m_Update2]
	End
	Method NewInstance:ObjectValue()
		Return New ObjectValue( New Entity,Self )
	End
End

Class f_UpdateWorld Extends Function
	Method New()
		Super.New "UpdateWorld","Int"
		_args=[]
	End
	Method Invoke:Value( args:Value[] )
		Return New IntValue( UpdateWorld() )
	End
End

Class f_RenderWorld Extends Function
	Method New()
		Super.New "RenderWorld","Int"
		_args=[]
	End
	Method Invoke:Value( args:Value[] )
		Return New IntValue( RenderWorld() )
	End
End

Class q_entity1 Extends Module
	Method New()
		Super.New "entity","Module"
		_classes=[New c_Entity1]
		_functions=[New f_UpdateWorld1,New f_RenderWorld1]
	End
End

_modules=[New m_entity1]
