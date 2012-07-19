
' Module trans.cpptranslator
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class CppTranslator Extends Translator

	Method TransType$( ty:Type )
		ty=ty.Actual()
		If VoidType( ty ) Return "void"
		If BoolType( ty ) Return "bool"
		If IntType( ty ) Return "int"
		If FloatType( ty ) Return "float"
		If StringType( ty ) Return "String"
		If ArrayType( ty ) Return "Array<"+TransType( ArrayType( ty ).elemType )+" >"
		If ObjectType( ty ) Return ObjectType( ty ).classDecl.actual.munged+"*"
'		If ObjectType( ty ) Return "Handle<"+ObjectType( ty ).classDecl.actual.munged+">"
		InternalErr
	End

	Method TransLocalType$( ty:Type )
		If ObjectType( ty ) Return ObjectType( ty ).classDecl.actual.munged+"*"
		Return TransType( ty )
	End
	
	Method TransValue$( ty:Type,value$ )
		If value
			If IntType( ty ) And value.StartsWith( "$" ) Return "0x"+value[1..]
			If BoolType( ty ) Return "true"
			If IntType( ty ) Return value
			If FloatType( ty ) Return value+"f"
			If StringType( ty ) Return "String("+Enquote( value )+")"
		Else
			If BoolType( ty ) Return "false"
			If NumericType( ty ) Return "0"
			If StringType( ty ) Return "String()"
			If ArrayType( ty ) Return "Array<"+TransType( ArrayType(ty).elemType )+" >()"
			If ObjectType( ty ) Return "0"
		EndIf
		InternalErr
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
		Return TransType( init.exprType )+" "+munged+"="+init.Trans()
	End
	
	Method EmitPushErr()
		Emit "pushErr();"
	End
	
	Method EmitSetErr( info$ )
		Emit "errInfo=~q"+info.Replace( "\","/" )+"~q;"
	End
	
	Method EmitPopErr()
		Emit "popErr();"
	End

	'***** Declarations *****
	
	Method TransStatic$( decl:Decl )
		If decl.IsExtern()
			Return decl.munged
		Else If _env And decl.scope And decl.scope=_env.ClassScope()
			Return decl.munged
		Else If ClassDecl( decl.scope )
			Return decl.scope.munged+"::"+decl.munged
		Else If ModuleDecl( decl.scope )
			Return decl.munged
		EndIf
		InternalErr
	End
	
	Method TransGlobal$( decl:GlobalDecl )
		Return TransStatic( decl )
	End
	
	Method TransField$( decl:FieldDecl,lhs:Expr )
		If lhs Return TransSubExpr( lhs )+"->"+decl.munged
		Return decl.munged
	End
		
	Method TransFunc$( decl:FuncDecl,args:Expr[],lhs:Expr )
		If decl.IsMethod()
			If lhs Return TransSubExpr( lhs )+"->"+decl.munged+TransArgs( args )
			Return decl.munged+TransArgs( args )
		EndIf
		Return TransStatic( decl )+TransArgs( args )
	End
	
	Method TransSuperFunc$( decl:FuncDecl,args:Expr[] )
		Return decl.ClassScope().munged+"::"+decl.munged+TransArgs( args )
	End
	
	'***** Expressions *****

	Method TransConstExpr$( expr:ConstExpr )
		Return TransValue( expr.exprType,expr.value )
	End
	
	Method TransNewObjectExpr$( expr:NewObjectExpr )
		Local ctor:=FuncDecl( expr.ctor.actual )
		Local cdecl:=ClassDecl( expr.classDecl.actual )
		'
		Return "(new "+cdecl.munged+"())->"+ctor.munged+TransArgs( expr.args )
	End
	
	Method TransNewArrayExpr$( expr:NewArrayExpr )
		Local texpr$=expr.expr.Trans()
		Local elemTy:=ArrayType( expr.exprType ).elemType
		'
		Return "Array<"+TransType( elemTy )+" >"+Bra( texpr )
	End
		
	Method TransSelfExpr$( expr:SelfExpr )
		Return "this"
	End
	
	Method TransCastExpr$( expr:CastExpr )
		Local texpr$=Bra( expr.expr.Trans() )
		Local dst:=expr.exprType
		Local src:=expr.expr.exprType
		
		If BoolType( dst )
			If BoolType( src ) Return texpr
			If IntType( src ) Return Bra( texpr+"!=0" )
			If FloatType( src ) Return Bra( texpr+"!=0.0f" )
			If ArrayType( src ) Return Bra( texpr+".Length()!=0" )
			If StringType( src ) Return Bra( texpr+".Length()!=0" )
			If ObjectType( src ) Return Bra( texpr+"!=0" )
		Else If IntType( dst )
			If BoolType( src ) Return Bra( texpr+"?1:0" )
			If IntType( src ) Return texpr
			If FloatType( src ) Return "int"+Bra(texpr)
			If StringType( src ) Return texpr+".ToInt()"
		Else If FloatType( dst )
			If IntType( src ) Return "float"+Bra(texpr)
			If FloatType( src ) Return texpr
			If StringType( src ) Return texpr+".ToFloat()"
		Else If StringType( dst )
			If IntType( src ) Return "String"+Bra( texpr )
			If FloatType( src ) Return "String"+Bra( texpr )
			If StringType( src ) Return texpr
		EndIf
		
		'upcast
		If src.ExtendsType( dst )
			If ArrayType( src ) And ArrayType( dst )
				Return "(("+TransType(dst)+")"+texpr+")"
			Endif
			Return texpr
		EndIf

		'downcast		
		If dst.ExtendsType( src )
			Return "dynamic_cast<"+dst.GetClass().actual.munged+"*>"+Bra( "("+src.GetClass().actual.munged+"*)"+texpr )
'			Return "dynamic_cast<"+dst.GetClass().actual.munged+"*>"+Bra( texpr )
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
		Return t_lhs+TransBinaryOp( expr.op )+t_rhs
	End
	
	Method TransIndexExpr$( expr:IndexExpr )
		Local t_expr:=TransSubExpr( expr.expr )
		Local t_index:=expr.index.Trans()
		If StringType( expr.expr.exprType ) 
			Return "(int)"+t_expr+"["+t_index+"]"		
		Else If ENV_CONFIG="debug"
			Return t_expr+".At("+t_index+")"
		Else
			Return t_expr+"["+t_index+"]"
		Endif
	End
	
	Method TransSliceExpr$( expr:SliceExpr )
		Local t_expr:=TransSubExpr( expr.expr )
		Local t_args:="0"
		If expr.from t_args=expr.from.Trans()
		If expr.term t_args+=","+expr.term.Trans()
		Return t_expr+".Slice("+t_args+")"
	End
	
	Method TransArrayExpr$( expr:ArrayExpr )
		Local elemType:=ArrayType( expr.exprType ).elemType

		Local tmp:=New LocalDecl( "",Type.voidType,Null )
		MungDecl tmp,localMungs
		
		Local t$
		For Local elem:=EachIn expr.exprs
			If t t+=","
			t+=elem.Trans()
		Next
		
		Local tt$
'		If Not _env tt="static "

		Emit tt+TransType( elemType )+" "+tmp.munged+"[]={"+t+"};"
		
		Return "Array<"+TransType( elemType )+" >("+tmp.munged+","+expr.exprs.Length+")"
	End

	Method TransIntrinsicExpr$( decl:Decl,expr:Expr,args:Expr[] )
		Local texpr$,arg0$,arg1$,arg2$
		
		If expr texpr=TransSubExpr( expr )
		
		If args.Length>0 And args[0] arg0=args[0].Trans()
		If args.Length>1 And args[1] arg1=args[1].Trans()
		If args.Length>2 And args[2] arg2=args[2].Trans()
		
		Local id$=decl.munged[1..]
		Local id2$=id[..1].ToUpper()+id[1..]
		
		Select id
		'
		'global functions
		Case "print" Return "Print"+Bra( arg0 )
		Case "error" Return "Error"+Bra( arg0 )
		'
		'string/array methods
		Case "length" Return texpr+".Length()"
		Case "resize" Return texpr+".Resize"+Bra( arg0 )
		
		'string methods
		Case "find" Return texpr+".Find"+Bra( arg0+","+arg1 )
		Case "findlast" Return texpr+".FindLast"+Bra( arg0 )
		Case "findlast2" Return texpr+".FindLast"+Bra( arg0+","+arg1 )
		Case "trim" Return texpr+".Trim()"
		Case "join" Return texpr+".Join"+Bra( arg0 )
		Case "split" Return texpr+".Split"+Bra( arg0 )
		Case "replace" Return texpr+".Replace"+Bra( arg0+","+arg1 )
		Case "tolower" Return texpr+".ToLower()"
		Case "toupper" Return texpr+".ToUpper()"
		Case "contains" Return texpr+".Contains"+Bra( arg0 )
		Case "startswith" Return texpr+".StartsWith"+Bra( arg0 )
		Case "endswith" Return texpr+".EndsWith"+Bra( arg0 )
		
		'string functions
		Case "fromchar" Return "String"+Bra( "(Char)"+Bra(arg0)+",1" )

		'math methods
		Case "sin","cos","tan" Return "(float)"+id+Bra( Bra(arg0)+"*0.0174532925" )
		Case "asin","acos","atan" Return "(float)"+Bra( id+Bra(arg0)+"*57.2957795" )
		Case "atan2" Return "(float)"+Bra( id+Bra(arg0+","+arg1)+"*57.2957795" )
		Case "sqrt","floor","ceil","log" Return "(float)"+id+Bra( arg0 )
		Case "pow" Return "(float)"+id+Bra( arg0+","+arg1 )
		'
		End Select
		InternalErr
	End

	'***** Statements *****
	
	Method TransAssignStmt$( stmt:AssignStmt )
#rem	
		If stmt.op="=" And stmt.rhs And ObjectType( stmt.rhs.exprType )
			'
			'assign object somewhere!
			'
			If VarExpr( stmt.lhs )
			
				Local decl:VarDecl=VarDecl( VarExpr(stmt.lhs).decl.actual )
				If FieldDecl( decl )
					Local lhs$="this",rhs$=stmt.rhs.Trans()
					Return "SetField(this,&"+decl.ClassScope.munged+"::"+decl.munged+","+stmt.rhs.Trans()+")"
				Else If GlobalDecl( decl )
					Return "SetGlobal(&"+decl.munged+","+stmt.rhs.Trans()+")"
				Else If LocalDecl( decl )
					Return "SetLocal(&"+decl.munged+","+stmt.rhs.Trans()+")"
				Endif
			
			Else If MemberVarExpr( stmt.lhs )
			
				Local decl:VarDecl=VarDecl( MemberVarExpr(stmt.lhs).decl.actual )
				If FieldDecl( decl )
					Local lhs$=MemberVarExpr(stmt.lhs).expr.Trans(),rhs$=stmt.rhs.Trans()
					Return "SetField("+lhs+",&"+decl.ClassScope.munged+"::"+decl.munged+","+rhs+")"
				Endif
				
			Else If IndexExpr( stmt.lhs )
			
			Else
				InternalErr
			Endif
		Endif
#end
		If stmt.rhs Return stmt.lhs.TransVar()+TransAssignOp(stmt.op)+stmt.rhs.Trans()
		Return stmt.lhs.Trans()
	End
	
	'***** Declarations *****
	
	Method EmitFuncProto( decl:FuncDecl )
		localMungs=New StringMap<Decl>

		Local args$
		For Local arg:=EachIn decl.argDecls
			MungDecl arg,localMungs
			If args args+=","
			args+=TransLocalType( arg.ty )+" "+arg.munged
		Next

		Local t$
		If decl.IsMethod() t+="virtual "
		If decl.IsStatic() And decl.ClassScope() t+="static "
		
		Emit t+TransLocalType( decl.retType )+" "+decl.munged+Bra( args )+";"

		localMungs=globalMungs
	End
	
	Method EmitFuncDecl( decl:FuncDecl )
		localMungs=New StringMap<Decl>
		Local args$
		For Local arg:=EachIn decl.argDecls
			localMungs.Insert arg.munged,arg
			If args args+=","
			args+=TransLocalType( arg.ty )+" "+arg.munged
		Next
		
		Local id$=decl.munged
		If decl.ClassScope() id=decl.ClassScope().munged+"::"+id
		
		Emit TransLocalType( decl.retType )+" "+id+Bra( args )+"{"
		
		EmitBlock decl

		Emit "}"

		localMungs=globalMungs
	End
	
	Method EmitClassProto( classDecl:ClassDecl )
		Local classid$=classDecl.munged
		Local superid$=classDecl.superClass.actual.munged
		
		Emit "class "+classid+" : public "+superid+"{"
		Emit "public:"

		'fields
		For Local decl:=EachIn classDecl.Semanted
			Local fdecl:=FieldDecl( decl )
			If fdecl
				Emit TransType( fdecl.ty )+" "+fdecl.munged+";"
				Continue
			EndIf
		Next

		'fields ctor		
		Emit classid+"();"
		
		'methods		
		For Local decl:=EachIn classDecl.Semanted
		
			Local fdecl:=FuncDecl( decl )
			If fdecl
				EmitFuncProto fdecl
				Continue
			EndIf
			
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit "static "+TransType( gdecl.ty )+" "+gdecl.munged+";"
				Continue
			EndIf
		Next

		'gc_mark
		Emit "void mark();"

		Emit "};"
	End
	
	Method EmitMark( id$,ty:Type )
		If ObjectType( ty ) Or ArrayType( ty )
			Emit "gc_mark("+id+");"
			Return
		Endif
	End
	
	Method EmitClassDecl( classDecl:ClassDecl )
	
		Local classid$=classDecl.munged
		Local superid$=classDecl.superClass.actual.munged
		
		'fields ctor		
		Emit classid+"::"+classid+"(){"
		For Local decl:=EachIn classDecl.Semanted
			Local fdecl:=FieldDecl( decl )
			If Not fdecl Continue
			Emit fdecl.munged+"="+fdecl.init.Trans()+";"
		Next
		Emit "}"

		'methods		
		For Local decl:=EachIn classDecl.Semanted
		
			Local fdecl:=FuncDecl( decl )
			If fdecl
				EmitFuncDecl fdecl
				Continue
			EndIf
			
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit TransType( gdecl.ty )+" "+classid+"::"+gdecl.munged+";"
				Continue
			EndIf
		Next
		
		'gc_mark
		Emit "void "+classid+"::mark(){"
		If classDecl.superClass 
			Emit classDecl.superClass.munged+"::mark();"
		Endif
		For Local tdecl:=Eachin classDecl.Semanted
			Local decl:=FieldDecl( tdecl )
			If decl EmitMark decl.munged,decl.ty
		Next
		Emit "}"

	End
	
	Method TransApp$( app:AppDecl )
	
		globalMungs=New StringMap<Decl>
		
		app.mainFunc.munged="bb_Main"
		
		'Munging
		For Local decl:=EachIn app.Semanted
			If decl.IsExtern() Or LocalDecl( decl ) Continue
		
			MungDecl decl,globalMungs

			Local cdecl:=ClassDecl( decl )
			If cdecl
				Emit "class "+decl.munged+";"
			
				localMungs=New StringMap<Decl>
				For Local decl:=EachIn cdecl.Semanted
					MungDecl decl,localMungs
				Next
				localMungs=globalMungs
			EndIf
		Next
		
		'Prototypes/header!
		For Local decl:=EachIn app.Semanted
			If decl.IsExtern() Or LocalDecl( decl ) Continue
		
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit "extern "+TransType( gdecl.ty )+" "+gdecl.munged+";"	'forward reference...
				Continue
			EndIf
		
			Local fdecl:=FuncDecl( decl )
			If fdecl
				EmitFuncProto fdecl
				Continue
			EndIf
		
			Local cdecl:=ClassDecl( decl )
			If cdecl
				EmitClassProto cdecl
				Continue
			EndIf
		Next
		
		'declarations!
		For Local decl:=EachIn app.Semanted
			If decl.IsExtern() Or LocalDecl( decl ) Continue
			
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit TransType( gdecl.ty )+" "+gdecl.munged+";"
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
		
		Emit "int bb_Init(){"
		For Local decl:=Eachin app.semantedGlobals
			Emit TransGlobal( decl )+"="+decl.init.Trans()+";"
		Next
		Emit "return 0;"
		Emit "}"

		Emit "void gc_mark(){"
		For Local decl:=Eachin app.semantedGlobals
			EmitMark TransGlobal( decl ),decl.ty
		Next
		Emit "}"

		Return JoinLines()
	End
	
End
