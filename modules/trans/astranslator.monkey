
' Module trans.astranslator
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class AsTranslator Extends Translator

	Method TransType$( ty:Type )
		ty=ty.Actual()
		If VoidType( ty ) Return "void"
		If BoolType( ty ) Return "Boolean"
		If IntType( ty ) Return "int"
		If FloatType( ty ) Return "Number"
		If StringType( ty ) Return "String"
		If ArrayType( ty ) Return "Array"
		If ObjectType( ty ) Return ObjectType( ty ).classDecl.munged
		InternalErr
	End
	
	Method TransValue$( ty:Type,value$ )
		If value
			If IntType( ty ) And value.StartsWith( "$" ) Return "0x"+value[1..]
			If BoolType( ty ) Return "true"
			If NumericType( ty ) Return value
			If StringType( ty ) Return Enquote( value )
		Else
			If BoolType( ty ) Return "false"
			If NumericType( ty ) Return "0"
			If StringType( ty ) Return "~q~q"
			If ArrayType( ty ) Return "[]"
			If ObjectType( ty ) Return "null"
		EndIf
		InternalErr
	End

	Method TransValDecl$( decl:ValDecl )
		Return decl.munged+":"+TransType( decl.ty )
	End
	
	Method TransArgs$( args:Expr[] )
		Local t$
		For Local arg:=EachIn args
			If t t+=","
			t+=arg.Trans()
		Next
		Return Bra(t)
	End
	
	'***** Utility *****

	Method TransLocalDecl$( munged$,init:Expr )
		Return "var "+munged+":"+TransType( init.exprType )+"="+init.Trans()
	End

	Method EmitPushErr()
		Emit "push_err();"
	End
	
	Method EmitSetErr( info$ )
		Emit "err_info=~q"+info.Replace( "\","/" )+"~q;"
	End
	
	Method EmitPopErr()
		Emit "pop_err();"
	End
	
	
	'***** Declarations *****

	Method TransStatic$( decl:Decl )
		If decl.IsExtern()
			Return decl.munged
		Else If _env And decl.scope And decl.scope=_env.ClassScope
			Return decl.munged
		Else If ClassDecl( decl.scope )
			Return decl.scope.munged+"."+decl.munged
		Else If ModuleDecl( decl.scope )
			Return decl.munged
		EndIf
		InternalErr
	End
	
	Method TransGlobal$( decl:GlobalDecl )
		Return TransStatic( decl )
	End
	
	Method TransField$( decl:FieldDecl,lhs:Expr )
		Local t_lhs$="this"
		If lhs
			t_lhs=TransSubExpr( lhs )
			If ENV_CONFIG="debug" t_lhs="dbg_object"+Bra(t_lhs)
		Endif
		Return t_lhs+"."+decl.munged
	End
		
	Method TransFunc$( decl:FuncDecl,args:Expr[],lhs:Expr )
		If decl.IsMethod()
			Local t_lhs$="this"
			If lhs
				t_lhs=TransSubExpr( lhs )
'				If ENV_CONFIG="debug" t_lhs="dbg_object"+Bra(t_lhs)
			Endif
			Return t_lhs+"."+decl.munged+TransArgs( args )
		EndIf
		Return TransStatic( decl )+TransArgs( args )
	End
	
	Method TransSuperFunc$( decl:FuncDecl,args:Expr[] )
		Return "super."+decl.munged+TransArgs( args )
	End
	
	'***** Expressions *****

	Method TransConstExpr$( expr:ConstExpr )
		Return TransValue( expr.exprType,expr.value )
	End

	Method TransNewObjectExpr$( expr:NewObjectExpr )
		Local ctor:=FuncDecl( expr.ctor.actual )
		Local cdecl:=ClassDecl( expr.classDecl.actual )
		'
		Return "(new "+cdecl.munged+")."+ctor.munged+TransArgs( expr.args )
	End
	
	Method TransNewArrayExpr$( expr:NewArrayExpr )
		Local texpr$=expr.expr.Trans()
		Local ty:=expr.ty.elemType
		If BoolType( ty ) Return "new_bool_array("+texpr+")"
		If NumericType( ty ) Return "new_number_array("+texpr+")"
		If StringType( ty ) Return "new_string_array("+texpr+")"
		If ObjectType( ty ) Return "new_object_array("+texpr+")"
		If ArrayType( ty ) Return "new_array_array("+texpr+")"
		InternalErr
	End
	
	Method TransSelfExpr$( expr:SelfExpr )
		Return "this"
	End
	
	Method TransCastExpr$( expr:CastExpr )
		Local dst:=expr.exprType
		Local src:=expr.expr.exprType
		Local texpr$=Bra( expr.expr.Trans() )
		
		If BoolType( dst )
			If BoolType( src ) Return texpr
			If IntType( src ) Return Bra( texpr+"!=0" )
			If FloatType( src ) Return Bra( texpr+"!=0.0" )
			If StringType( src ) Return Bra( texpr+".length!=0" )
			If ArrayType( src ) Return Bra( texpr+".length!=0" )
			If ObjectType( src ) Return Bra( texpr+"!=null" )
		Else If IntType( dst )
			If BoolType( src ) Return Bra( texpr+"?1:0" )
			If IntType( src ) Return texpr
			If FloatType( src ) Return Bra( texpr+"|0" )
			If StringType( src ) Return "parseInt"+texpr
		Else If FloatType( dst )
			If NumericType( src ) Return texpr
			If StringType( src ) 	Return "parseFloat"+texpr
		Else If StringType( dst )
			If NumericType( src ) Return "String"+texpr
			If StringType( src )  Return texpr
		EndIf

		'upcast
		If src.ExtendsType( dst )
			Return texpr
		EndIf

		'downcast		
		If dst.ExtendsType( src )
			Return Bra( texpr + " as "+TransType( dst ) )
		EndIf
		
		InternalErr
	End
	
	Method TransUnaryExpr$( expr:UnaryExpr )
		Local pri=ExprPri( expr )
		Local t_expr$=TransSubExpr( expr.expr,pri )
		Return TransUnaryOp( expr.op )+t_expr
	End
	
	Method TransBinaryExpr$( expr:BinaryExpr )
		Local pri=ExprPri( expr )
		Local t_lhs$=TransSubExpr( expr.lhs,pri )
		Local t_rhs$=TransSubExpr( expr.rhs,pri-1 )
		Local t_expr$=t_lhs+TransBinaryOp( expr.op )+t_rhs
		If expr.op="/" And IntType( expr.exprType ) t_expr=Bra( Bra(t_expr)+"|0" )
		Return t_expr
	End
	
	Method TransIndexExpr$( expr:IndexExpr )
		Local t_expr:=TransSubExpr( expr.expr )
		If StringType( expr.expr.exprType ) 
			Local t_index:=expr.index.Trans()
			Return t_expr+".charCodeAt("+t_index+")"
		Else If ENV_CONFIG="debug"
			Local t_index:=TransExprNS( expr.index )
			Return "dbg_array("+t_expr+","+t_index+")["+t_index+"]"
		Else
			Local t_index:=expr.index.Trans()
			Return t_expr+"["+t_index+"]"
		Endif
	End
	
	Method TransSliceExpr$( expr:SliceExpr )
		Local t_expr$=TransSubExpr( expr.expr )
		Local t_args$="0"
		If expr.from t_args=expr.from.Trans()
		If expr.term t_args+=","+expr.term.Trans()
		Return t_expr+".slice("+t_args+")"
	End
	
	Method TransArrayExpr$( expr:ArrayExpr )
		Local t$
		For Local elem:=EachIn expr.exprs
			If t t+=","
			t+=elem.Trans()
		Next
		Return "["+t+"]"
	End
	
	Method TransIntrinsicExpr$( decl:Decl,expr:Expr,args:Expr[] )
	
		Local texpr$,arg0$,arg1$,arg2$
		
		If expr texpr=TransSubExpr( expr )
		
		If args.Length>0 And args[0] arg0=args[0].Trans()
		If args.Length>1 And args[1] arg1=args[1].Trans()
		If args.Length>2 And args[2] arg2=args[2].Trans()
		
		Local id$=decl.munged[1..]
		
		Select id
		'
		'global functions
		Case "print" Return "print("+arg0+")"
		Case "error" Return "error("+arg0+")"
		
		'string/array methods
		Case "length" Return texpr+".length"
		'
		'array methods
		Case "resize"
			Local ty:=ArrayType( expr.exprType ).elemType
			If NumericType( ty ) Return "resize_number_array"+Bra( texpr+","+arg0 )
			If StringType( ty ) Return "resize_string_array"+Bra( texpr+","+arg0 )
			If ObjectType( ty ) Return "resize_object_array"+Bra( texpr+","+arg0 )
			InternalErr
		'string methods
		Case "find" Return texpr+".indexOf"+Bra( arg0+","+arg1 )
		Case "findlast" Return texpr+".lastIndexOf"+Bra( arg0 )
		Case "findlast2" Return texpr+".lastIndexOf"+Bra( arg0+","+arg1 )
		Case "trim" Return "string_trim"+Bra( texpr )
'		Case "join" Return "string_join"+Bra( texpr+","+arg0 )
		Case "join" Return arg0+".join"+Bra( texpr )
		Case "split" Return texpr+".split"+Bra( arg0 )
		Case "replace" Return "string_replace"+Bra(texpr+","+arg0+","+arg1)
		Case "tolower" Return texpr+".toLowerCase()"
		Case "toupper" Return texpr+".toUpperCase()"
		Case "contains" Return Bra( texpr+".indexOf"+Bra( arg0 )+"!=-1" )
		Case "startswith" Return "string_starts_with"+Bra( texpr+","+arg0 )
		Case "endswith" Return "string_ends_with"+Bra( texpr+","+arg0 )
		'string functions
		Case "fromchar" Return "String.fromCharCode"+Bra( arg0 )
		'
		'math functions
		Case "sin","cos","tan" Return "Math."+id+Bra( Bra( arg0 )+"*D2R" )
		Case "asin","acos","atan" Return Bra( "Math."+id+Bra( arg0 )+"*R2D" )
		Case "atan2" Return Bra( "Math."+id+Bra( arg0+","+arg1 )+"*R2D" )
		'
		Case "sqrt","floor","ceil","log" Return "Math."+id+Bra( arg0 )
		Case "pow" Return "Math."+id+Bra( arg0+","+arg1 )
		'
		End Select
		InternalErr
	End
	
	'***** Statements *****

	Method TransAssignStmt$( stmt:AssignStmt )
		If ENV_CONFIG="debug"
			Local ie:=IndexExpr( stmt.lhs )
			If ie
				Local t_rhs:=stmt.rhs.Trans()
				Local t_expr:=ie.expr.Trans()
				Local t_index:=TransExprNS( ie.index )
				Emit "dbg_array("+t_expr+","+t_index+")["+t_index+"]"+stmt.op+t_rhs
				Return
			Endif
		Endif
		Return Super.TransAssignStmt( stmt )
	End
	
	
	'***** Declarations *****
	
	Method EmitFuncDecl( decl:FuncDecl )

		localMungs=New StringMap<Decl>

		Local args$
		For Local arg:=EachIn decl.argDecls
			MungDecl arg,localMungs
			If args args+=","
			args+=TransValDecl( arg )
		Next
		
		Local t$="internal "
		
		If decl.overrides And Not decl.IsCtor() t+="override "
		
		If decl.IsStatic() And decl.ClassScope() t+="static "
		
		Emit t+"function "+decl.munged+Bra( args )+":"+TransType( decl.retType )+"{"
		
		If decl.IsAbstract()
			If VoidType( decl.retType )
				Emit "return;"
			Else
				Emit "return "+TransValue( decl.retType,"" )+";"
			Endif
		Else
			EmitBlock decl
		EndIf

		Emit "}"
		
		localMungs=globalMungs
	End

	Method EmitClassDecl( classDecl:ClassDecl )

		Local classid$=classDecl.munged
		Local superid$=classDecl.superClass.actual.munged
		
		Emit "class "+classid+" extends "+superid+"{"
		
		'members...
		For Local decl:=EachIn classDecl.Semanted
			Local tdecl:=FieldDecl( decl )
			If tdecl
				Emit "internal var "+TransValDecl( tdecl )+"="+tdecl.init.Trans()+";"
				Continue
			Endif
			
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit "internal static var "+TransValDecl( gdecl )+";"
				Continue
			Endif
			
			Local fdecl:=FuncDecl( decl )
			If fdecl
				EmitFuncDecl fdecl
				Continue
			Endif
		Next
		
		Emit "}"
	End
	
	Method TransApp$( app:AppDecl )
		
		globalMungs=New StringMap<Decl>
		
		app.mainFunc.munged="bb_Main"
		
		For Local decl:=EachIn app.Semanted
			MungDecl decl,globalMungs

			Local cdecl:=ClassDecl( decl )
			If Not cdecl Continue
			
			localMungs=New StringMap<Decl>
			For Local decl:=EachIn cdecl.Semanted
				If FuncDecl( decl ) And FuncDecl( decl ).IsCtor()
					decl.ident=cdecl.ident+"_"+decl.ident
				EndIf
				MungDecl decl,localMungs
			Next
		Next
	
		'decls
		'
		For Local decl:=EachIn app.Semanted
			If decl.IsNative() Continue

			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit "var "+TransValDecl( gdecl )+";"
				Continue
			EndIf
			
			Local fdecl:=FuncDecl( decl )
			If fdecl
				EmitFuncDecl fdecl
				Continue
			EndIf
			
			Local cdecl:=ClassDecl( decl )
			If cdecl
				EmitClassDecl cdecl
				Continue
			EndIf
		Next
		
		Emit "function bb_Init():void{"
		For Local decl:=Eachin app.semantedGlobals
			Emit TransGlobal( decl )+"="+decl.init.Trans()+";"
		Next
		Emit "}"
		
		Return JoinLines()
	End
	
End

