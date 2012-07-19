
' Module trans.jstranslator
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class JsTranslator Extends Translator

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
	
	Method TransArgs$( args:Expr[],first$="" )
		Local t$=first
		For Local arg:Expr=Eachin args
			If t t+=","
			t+=arg.Trans()
		Next
		Return Bra(t)
	End
	
	'***** Utility *****
	
	Method TransLocalDecl$( munged$,init:Expr )
		Return "var "+munged+"="+init.Trans()
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
	
	Method TransGlobal$( decl:GlobalDecl )
		Return decl.munged
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
		Endif
		Return decl.munged+TransArgs( args )
	End
	
	Method TransSuperFunc$( decl:FuncDecl,args:Expr[] )
		If decl.IsCtor() Return decl.munged+".call"+TransArgs( args,"this" )
		Return decl.scope.munged+".prototype."+decl.munged+".call"+TransArgs( args,"this" )
	End
	
	'***** Expressions *****
	
	Method TransConstExpr$( expr:ConstExpr )
		Return TransValue( expr.exprType,expr.value )
	End
	
	Method TransNewObjectExpr$( expr:NewObjectExpr )
		Local ctor:FuncDecl=FuncDecl( expr.ctor.actual )
		Local cdecl:ClassDecl=ClassDecl( expr.classDecl.actual )
		'
		Return ctor.munged+".call"+TransArgs( expr.args,"new "+cdecl.munged )
	End
	
	Method TransNewArrayExpr$( expr:NewArrayExpr )
		Local texpr$=expr.expr.Trans()
		Local ty:Type=ArrayType( expr.exprType ).elemType
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
		Local dst:Type=expr.exprType
		Local src:Type=expr.expr.exprType
		Local texpr$=Bra(expr.expr.Trans())
		
		If BoolType( dst )
			If BoolType( src ) Return texpr
			If IntType( src ) Return Bra(texpr+"!=0")
			If FloatType( src ) Return Bra(texpr+"!=0.0")
			If StringType( src ) Return Bra(texpr+".length!=0")
			If ArrayType( src ) Return Bra(texpr+".length!=0")
			If ObjectType( src ) Return Bra(texpr+"!=null" )
		Else If IntType( dst )
			If BoolType( src ) Return Bra(texpr+"?1:0")
			If IntType( src ) Return texpr
			If FloatType( src ) Return Bra(texpr+"|0")
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
			Return "object_downcast"+Bra( texpr+","+dst.GetClass().actual.munged )
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
		For Local elem:Expr=Eachin expr.exprs
			If t t+=","
			t+=elem.Trans()
		Next
		Return "["+t+"]"
	End
	
	'***** Statements *****
	
	Method TransAssignStmt$( stmt:AssignStmt )
		If ENV_CONFIG="debug"
			Local ie:=IndexExpr( stmt.lhs )
			If ie
				Local t_rhs:=stmt.rhs.Trans()
				Local t_expr:=ie.expr.Trans()
				Local t_index:=TransExprNS( ie.index )
				Emit "dbg_array("+t_expr+","+t_index+")["+t_index+"]"+TransAssignOp(stmt.op)+t_rhs
				Return
			Endif
		Endif
		Return Super.TransAssignStmt( stmt )
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
		'functions
		Case "print" Return "print"+Bra( arg0 )
		Case "error" Return "error"+Bra( arg0 )
		'
		'string/array methods
		Case "length" Return texpr+".length"
		
		'array methods
		Case "resize"
			Local ty:=ArrayType( expr.exprType ).elemType
			If BoolType( ty ) Return "resize_bool_array"+Bra( texpr+","+arg0 )
			If NumericType( ty ) Return "resize_number_array"+Bra( texpr+","+arg0 )
			If StringType( ty ) Return "resize_string_array"+Bra( texpr+","+arg0 )
			If ArrayType( ty ) Return "resize_array_array"+Bra( texpr+","+arg0 )
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
		Case "replace" Return "string_replace"+Bra( texpr+","+arg0+","+arg1 )
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
		'
		InternalErr
	End
	
	'***** Declarations *****

	Method EmitFuncDecl( decl:FuncDecl )
		PushMungScope
		
		Local args$
		For Local arg:ArgDecl=Eachin decl.argDecls
			MungDecl arg
			If args args+=","
			args+=arg.munged
		Next
		args=Bra(args)
		
		If decl.IsCtor()
			Emit "function "+decl.munged+args+"{"
		Else If decl.IsMethod()
			Emit decl.ClassScope().munged+".prototype."+decl.munged+"=function"+args+"{"
		Else
			Emit "function "+decl.munged+args+"{"
		EndIf

		EmitBlock decl
		
		Emit "}"
		
		PopMungScope
	End
	
	Method EmitClassDecl( classDecl:ClassDecl )
		PushEnv classDecl
		
		Local classid$=classDecl.munged
		Local superid$=classDecl.superClass.actual.munged
		
		'JS constructor - initializes fields
		'
		Emit "function "+classid+"(){"
		Emit superid+".call(this);"
		
		For Local decl:=Eachin classDecl.Semanted
			Local fdecl:=FieldDecl( decl )
			if fdecl Emit "this."+fdecl.munged+"="+fdecl.init.Trans()+";"
		Next
		
		Emit "}"
		
		'extends superclass object
		If superid<>"Object"
			Emit classid+".prototype=extend_class("+superid+");"
		EndIf

		'class members
		For Local decl:=Eachin classDecl.Semanted
			If decl.IsExtern() Continue

			Local fdecl:FuncDecl=FuncDecl( decl )
			If fdecl
				EmitFuncDecl fdecl
				Continue
			EndIf
			
			Local gdecl:GlobalDecl=GlobalDecl( decl )
			If gdecl
				Emit "var "+gdecl.munged+";"
				Continue
			Endif

		Next
	
		PopEnv
	End
	
	Method TransApp$( app:AppDecl )
		
		app.mainFunc.munged="bb_Main"
		
		For Local decl:=Eachin app.Semanted
			
			MungDecl decl

			Local cdecl:=ClassDecl( decl )
			If Not cdecl Continue

			'global mungs
			For Local decl:=Eachin cdecl.Semanted
				If FuncDecl( decl ) And Not FuncDecl( decl ).IsMethod() Or GlobalDecl( decl )
					MungDecl decl
				Endif
			Next
			
			'local mungs
			PushMungScope
			
			MungOverrides cdecl
			
			For Local decl:=Eachin cdecl.Semanted
				MungDecl decl
			Next
			
			PopMungScope
		Next
		
		For Local decl:=Eachin app.Semanted
			
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit "var "+gdecl.munged+";"
				Continue
			EndIf
			
			Local fdecl:FuncDecl=FuncDecl( decl )
			If fdecl
				EmitFuncDecl fdecl
				Continue
			EndIf
			
			Local cdecl:ClassDecl=ClassDecl( decl )
			If cdecl
				EmitClassDecl cdecl
				Continue
			EndIf
		Next

		Emit "function bb_Init(){"
		For Local decl:=Eachin app.semantedGlobals
			Emit decl.munged+"="+decl.init.Trans()+";"
		Next
		Emit "}"
		
		Return JoinLines()
	End
	
End