
' Module trans.trans
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class Type

	Method ActualType:Type()
		Return Self
	End

	Method EqualsType( ty:Type )
		Return False
	End
	
	Method ExtendsType( ty:Type )
		Return EqualsType( ty )
	End
	
	Method Semant:Type()
		Return Self
	End

	Method GetClass:ClassDecl()
		Return Null
	End
	
	Method ToString$()
		Return "??Type??"
	End
	
	Global voidType:=New VoidType
	Global boolType:=New BoolType
	Global intType:=New IntType
	Global floatType:=New FloatType
	Global stringType:=New StringType
	Global emptyArrayType:ArrayType=New ArrayType( voidType )
	Global objectType:IdentType=New IdentType( "monkey.object",[] )
	Global nullObjectType:=New IdentType( "",[] )

End

Class VoidType Extends Type

	Method EqualsType( ty:Type )
		Return VoidType( ty )<>Null
	End
	
	Method ToString$()
		Return "Void"
	End
End

Class BoolType Extends Type

	Method EqualsType( ty:Type )
		Return BoolType( ty )<>Null
	End
	
	Method ExtendsType( ty:Type )
		Return IntType( ty )<>Null Or BoolType( ty )<>Null
	End
	
	Method ToString$()
		Return "Bool"
	End

End

Class NumericType Extends Type

End

Class IntType Extends NumericType
	
	Method EqualsType( ty:Type )
		Return IntType( ty )<>Null
	End
	
	Method ExtendsType( ty:Type )
		If ObjectType( ty )
			Local expr:Expr=New ConstExpr( Self,"" ).Semant()
			Local ctor:FuncDecl=ty.GetClass().FindFuncDecl( "new",[expr],True )
			Return ctor And ctor.IsCtor()
		Endif
		Return NumericType( ty )<>Null Or StringType( ty )<>Null
	End
	
	Method ToString$()
		Return "Int"
	End
End

Class FloatType Extends NumericType
	
	Method EqualsType( ty:Type )
		Return FloatType( ty )<>Null
	End
	
	Method ExtendsType( ty:Type )
		If ObjectType( ty )
			Local expr:Expr=New ConstExpr( Self,"" ).Semant()
			Local ctor:FuncDecl=ty.GetClass().FindFuncDecl( "new",[expr],True )
			Return ctor And ctor.IsCtor()
		Endif	
		Return NumericType( ty )<>Null Or StringType( ty )<>Null
	End
	
	Method ToString$()
		Return "Float"
	End

End

Class StringType Extends Type

	Method EqualsType( ty:Type )
		Return StringType( ty )<>Null
	End

	Method ExtendsType( ty:Type )	
		If ObjectType( ty )
			Local expr:Expr=New ConstExpr( Self,"" ).Semant()
			Local ctor:FuncDecl=ty.GetClass().FindFuncDecl( "new",[expr],True )
			Return ctor And ctor.IsCtor()
		Endif
		Return EqualsType( ty )
	End
	
	Method GetClass:ClassDecl()
		Return _env.FindClassDecl( "string",[] )
	End
	
	Method ToString$()
		Return "String"
	End
End

Class ArrayType Extends Type
	Field elemType:Type
	
	Method New( elemType:Type )
		Self.elemType=elemType
	End
	
	Method ActualType:Type()
		Local ty:=elemType.ActualType()
		If ty=elemType Return Self
		Return New ArrayType( ty )
	End
		
	Method EqualsType( ty:Type )
		Local arrayType:ArrayType=ArrayType( ty )
		Return arrayType And elemType.EqualsType( arrayType.elemType )
	End
	
	Method ExtendsType( ty:Type )
		Local arrayType:ArrayType=ArrayType( ty )
		Return arrayType And ( VoidType( elemType ) Or elemType.EqualsType( arrayType.elemType ) )
	End
	
	Method Semant:Type()
		Local ty:=elemType.Semant()
		If ty<>elemType Return New ArrayType( ty )
		Return Self
	End
	
	Method GetClass:ClassDecl()
		Return _env.FindClassDecl( "array",[] )
	End
	
	Method ToString$()
		Return elemType.ToString()+"[]"
	End
End

Class ObjectType Extends Type
	Field classDecl:ClassDecl
	
	Method New( classDecl:ClassDecl )
		Self.classDecl=classDecl
	End
	
	Method ActualType:Type()
		If classDecl.actual=classDecl Return Self
		Return New ObjectType( ClassDecl(classDecl.actual) )
	End
	
	Method EqualsType( ty:Type )
		Local objty:ObjectType=ObjectType( ty )
		Return objty And classDecl=objty.classDecl
	End
	
	Method ExtendsType( ty:Type )
		Local objty:ObjectType=ObjectType( ty )
		If objty Return classDecl.ExtendsClass( objty.classDecl )
		Local op$
		If BoolType( ty )
			op="ToBool"
		Else If IntType( ty ) 
			op="ToInt"
		Else If FloatType( ty )
			op="ToFloat"
		Else If StringType( ty )
			op="ToString"
		Else
			Return False
		Endif
		Local fdecl:FuncDecl=GetClass().FindFuncDecl( op,[],True )
		Return fdecl And fdecl.IsMethod() And fdecl.retType.EqualsType( ty )
	End
	
	Method GetClass:ClassDecl()
		Return classDecl
	End
	
	Method ToString$()
		Return classDecl.ToString()
	End
End

Class IdentType Extends Type
	Field ident$
	Field args:IdentType[]
	
	Method New( ident$,args:IdentType[] )
		Self.ident=ident
		Self.args=args
	End
	
	Method ActualType:Type()
		InternalErr
	End
	
	Method EqualsType( ty:Type )
		InternalErr
	End
	
	Method ExtendsType( ty:Type )
		InternalErr
	End
	
	Method Semant:Type()
		If ident Return New ObjectType( FindClass() )
		Return New ObjectType( ClassDecl.nullObjectClass )
	End
	
	Method FindClass:ClassDecl()
	
		Local argClasses:ClassDecl[args.Length]

		For Local i=0 Until args.Length
			argClasses[i]=args[i].FindClass()
		Next
		
		Local clsid$
		Local cdecl:ClassDecl
		Local i=ident.Find( "." )
		If i=-1
			clsid=ident
			cdecl=_env.FindClassDecl( clsid,argClasses )
		Else
			Local modid$=ident[..i]
			Local mdecl:ModuleDecl=_env.FindModuleDecl( modid )
			If Not mdecl Err "Module '"+modid+"' not found"
			clsid=ident[i+1..]
			cdecl=mdecl.FindClassDecl( clsid,argClasses )
		Endif
		If Not cdecl Err "Class '"+clsid+"' not found"
		Return cdecl
	End
	
	Method ToString$()
		Local t$
		For Local arg:=Eachin args
			If t t+=","
			t+=arg.ToString()
		Next
		If t Return "$"+ident+"<"+t.Replace("$","")+">"
		Return "$"+ident
	End
End
