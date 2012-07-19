
' Module trans.javatranslator
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class JavaTranslator Extends Translator

	Method TransType$( ty:Type )
		If VoidType( ty ) Return "void"
		If BoolType( ty ) Return "boolean"
		If IntType( ty ) Return "int"
		If FloatType( ty ) Return "float"
		If StringType( ty ) Return "String"
		If ArrayType( ty ) Return TransType( ArrayType( ty ).elemType )+"[]"
		If ObjectType( ty ) Return ty.GetClass().actual.munged
		InternalErr
	End
	
	Method TransValue$( ty:Type,value$ )
		If value
			If IntType( ty ) And value.StartsWith( "$" ) Return "0x"+value[1..]
			If BoolType( ty ) Return "true"
			If IntType( ty ) Return value
			If FloatType( ty ) Return value+"f"
			If StringType( ty ) Return Enquote( value )
		Else
			If BoolType( ty ) Return "false"
			If NumericType( ty ) Return "0"
			If StringType( ty ) Return "~q~q"
			If ArrayType( ty ) 
				Local elemTy:=ArrayType( ty ).elemType
				Local t$="[0]"
				While ArrayType( elemTy )
					elemTy=ArrayType( elemTy ).elemType
					t+="[]"
				Wend
				Return "new "+TransType( elemTy )+t
			Endif
			If ObjectType( ty ) Return "null"
		Endif
		InternalErr
	End

	Method TransDecl$( decl:Decl )
		Local id$=decl.munged

		Local vdecl:=ValDecl( decl )
		If vdecl Return TransType( vdecl.ty )+" "+id
		
		InternalErr
	End
	
	Method TransArgs$( args:Expr[] )
		Local t$
		For Local arg:=Eachin args
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
		Emit "bb_std_lang.pushErr();"
	End
	
	Method EmitSetErr( info$ )
		Emit "bb_std_lang.errInfo=~q"+info.Replace( "\","/" )+"~q;"
	End
	
	Method EmitPopErr()
		Emit "bb_std_lang.popErr();"
	End

	'***** Declarations *****
	
	Method TransStatic$( decl:Decl )
		If decl.IsExtern()
			Return decl.munged
		Else If _env And decl.scope And decl.scope=_env.ClassScope()
			Return decl.munged
		Else If ClassDecl( decl.scope )
			Return decl.scope.munged+"."+decl.munged
		Else If ModuleDecl( decl.scope )
			Return decl.scope.munged+"."+decl.munged
		Endif
		InternalErr
	End
	
	Method TransTemplateCast$( ty:Type,src:Type,expr$ )
		If ObjectType(ty) And ObjectType(src) And ty.GetClass().actual<>src.GetClass().actual
			Return "(("+TransType(ty)+")"+Bra(expr)+")"
		Endif
		Return expr
	End
	
	Method TransGlobal$( decl:GlobalDecl )
		Return TransStatic( decl )
	End
	
	Method TransField$( decl:FieldDecl,lhs:Expr )
		If lhs Return TransSubExpr( lhs )+"."+decl.munged
		Return decl.munged
	End
		
	Method TransFunc$( decl:FuncDecl,args:Expr[],lhs:Expr )
		If decl.IsMethod()
			If lhs Return TransSubExpr( lhs )+"."+decl.munged+TransArgs( args )
			Return decl.munged+TransArgs( args )
		Endif
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
		'
		Local ctor:=FuncDecl( expr.ctor.actual )
		Local cdecl:=ClassDecl( expr.classDecl.actual )
		'
		Return "(new "+cdecl.munged+"())."+ctor.munged+TransArgs( expr.args )
	End
	
	Method TransNewArrayExpr$( expr:NewArrayExpr )
		Local texpr$=expr.expr.Trans()
		Local elemTy:=ArrayType( expr.exprType ).elemType
		'
		Local t$="["+texpr+"]"
		While ArrayType( elemTy )
			elemTy=ArrayType( elemTy ).elemType
			t+="[]"
		Wend
		'
		Return "new "+TransType( elemTy )+t
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
			If StringType( src ) Return Bra( texpr+".length()!=0" )
			If ArrayType( src ) Return Bra( texpr+".length!=0" )
			If ObjectType( src ) Return Bra( texpr+"!=null" )
		Else If IntType( dst )
			If BoolType( src ) Return Bra( texpr+"?1:0" )
			If IntType( src ) Return texpr
			If FloatType( src ) Return "(int)"+texpr
			If StringType( src ) Return "Integer.parseInt"+texpr
		Else If FloatType( dst )
			If IntType( src ) Return "(float)"+texpr
			If FloatType( src ) Return texpr
			If StringType( src ) Return "Float.parseFloat"+texpr
		Else If StringType( dst )
			If IntType( src ) Return "String.valueOf"+texpr
			If FloatType( src ) Return "String.valueOf"+texpr
			If StringType( src ) Return texpr
		Endif
		
		If src.GetClass().ExtendsClass( dst.GetClass() )
			Return texpr
		Else If dst.GetClass().ExtendsClass( src.GetClass() )
			Local tmp:=New LocalDecl( "",src,Null )
			MungDecl tmp
			Emit TransType( src )+" "+tmp.munged+"="+expr.expr.Trans()+";"
			Return "($t instanceof $c ? ($c)$t : null)".Replace( "$t",tmp.munged ).Replace( "$c",TransType(dst) )
		Endif

		Err "Java translator can't convert "+src.ToString()+" to "+dst.ToString()
	End
	
	Method TransUnaryExpr$( expr:UnaryExpr )
		Local texpr$=expr.expr.Trans()
		If ExprPri( expr.expr )>ExprPri( expr ) texpr=Bra( texpr )
		Return TransUnaryOp( expr.op )+texpr
	End
	
	Method TransBinaryExpr$( expr:BinaryExpr )
		Local lhs$=expr.lhs.Trans()
		Local rhs$=expr.rhs.Trans()
		
		'String compare
		If BinaryCompareExpr( expr ) And StringType( expr.lhs.exprType ) And StringType( expr.rhs.exprType )
			Return Bra( lhs+".compareTo"+Bra(rhs)+TransBinaryOp( expr.op )+"0" )
		Endif
		
		Local pri=ExprPri( expr )
		If ExprPri( expr.lhs )>pri lhs=Bra( lhs )
		If ExprPri( expr.rhs )>=pri rhs=Bra( rhs )
		Return lhs+TransBinaryOp( expr.op )+rhs
	End
	
	Method TransIndexExpr$( expr:IndexExpr )
		Local texpr$=expr.expr.Trans()
		Local index$=expr.index.Trans()
		If StringType( expr.expr.exprType ) Return "(int)"+texpr+".charAt("+index+")"
		Return texpr+"["+index+"]"
	End
	
	Method TransSliceExpr$( expr:SliceExpr )
		Local texpr$=expr.expr.Trans()
		Local from$=",0",term$
		If expr.from from=","+expr.from.Trans()
		If expr.term term=","+expr.term.Trans()
		If ArrayType( expr.exprType )
			Return "(("+TransType( expr.exprType )+")bb_std_lang.sliceArray"+Bra( texpr+from+term )+")"
		Else If StringType( expr.exprType )
			Return "bb_std_lang.slice("+texpr+from+term+")"
		Endif
		InternalErr
	End
	
	Method TransArrayExpr$( expr:ArrayExpr )
		Local t$
		For Local elem:=Eachin expr.exprs
			If t t+=","
			t+=elem.Trans()
		Next
		Return "new "+TransType( expr.exprType )+"{"+t+"}"
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
		Case "print" Return "bb_std_lang.print"+Bra( arg0 )
		Case "error" Return "bb_std_lang.error"+Bra( arg0 )
				
		'string/array methods
		Case "length"
			If StringType( expr.exprType ) Return texpr+".length()"
			Return "bb_std_lang.arrayLength"+Bra( texpr )
		'
		Case "resize" Return "(("+TransType( expr.exprType )+")bb_std_lang.resizeArray"+Bra( texpr+","+arg0 )+")"
		
		'string methods
		Case "compare" Return texpr+".compareTo"+Bra( arg0 )
		Case "find" Return texpr+".indexOf"+Bra( arg0+","+arg1 )
		Case "findlast" Return texpr+".lastIndexOf"+Bra( arg0 )
		Case "findlast2" Return texpr+".lastIndexOf"+Bra( arg0+","+arg1 )
		Case "trim" Return texpr+".trim()"
		Case "join" Return "bb_std_lang.join"+Bra( texpr+","+arg0 )
		Case "split" Return "bb_std_lang.split"+Bra( texpr+","+arg0 )
		Case "replace" Return "bb_std_lang.replace"+Bra( texpr+","+arg0+","+arg1 )
		Case "tolower" Return texpr+".toLowerCase()"
		Case "toupper" Return texpr+".toUpperCase()"
		Case "contains" Return Bra( texpr+".indexOf"+Bra( arg0 )+"!=-1" )
		Case "startswith" Return texpr+".startsWith"+Bra( arg0 )
		Case "endswith" Return texpr+".endsWith"+Bra( arg0 )
		'string functions		
		Case "fromchar" Return "String.valueOf"+Bra("(char)"+Bra( arg0 ) )
		'
		'math methods
		Case "sin","cos","tan" Return "(float)Math."+id+Bra( Bra(arg0)+"*bb_std_lang.D2R" )
		Case "asin","acos","atan" Return "(float)"+Bra( "Math."+id+Bra(arg0)+"*bb_std_lang.R2D" )
		Case "atan2" Return "(float)"+Bra( "Math."+id+Bra(arg0+","+arg1)+"*bb_std_lang.R2D" )
		'
		Case "sqrt","floor","ceil","log" Return "(float)Math."+id+Bra(arg0)
  		Case "pow" Return "(float)Math."+id+Bra( arg0+","+arg1 )
		'
		End Select
		InternalErr
	End

	'***** Statements *****

	'***** Declarations *****
	
	Method EmitFuncDecl( decl:FuncDecl )
		PushMungScope
		
		'Find decl we override
		Local odecl:=decl
		While odecl.overrides
			odecl=odecl.overrides
		Wend

		'Generate 'args' string and arg casts
		Local args$
		Local argCasts:=New StringStack
		For Local i=0 Until decl.argDecls.Length
			Local arg:=decl.argDecls[i]
			Local oarg:=odecl.argDecls[i]
			MungDecl arg
			If args args+=","
			args+=TransType( oarg.ty )+" "+arg.munged
			If arg.ty.EqualsType( oarg.ty ) Continue
			Local t$=arg.munged
			arg.munged=""
			MungDecl arg
			argCasts.Push TransDecl( arg )+"="+Bra(TransType(arg.ty))+Bra(t)+";"
		Next
		
		Local t$="public "+TransType( odecl.retType )+" "+decl.munged+Bra( args )
		
		If decl.ClassScope() And decl.ClassScope().IsInterface()
			Emit t+";"
		Else
			Local q$
			If decl.IsStatic() q+="static "
			Emit q+t+"{"
			For Local t$=Eachin argCasts
				Emit t
			Next
			EmitBlock decl
			Emit "}"
		Endif
		
		PopMungScope
	End
	
	Method EmitClassDecl( classDecl:ClassDecl )
	
		If classDecl.IsTemplateInst()
			InternalErr
		Endif
	
		Local classid$=classDecl.actual.munged
		Local superid$=classDecl.superClass.actual.munged

		If classDecl.IsInterface() 

			Local bases$
			For Local iface:=Eachin classDecl.implments
				If bases bases+="," Else bases=" extends "
				bases+=iface.actual.munged
			Next

			Emit "interface "+classid+bases+"{"
			
			For Local decl:=Eachin classDecl.Semanted
				If Not FuncDecl( decl ) InternalErr
				EmitFuncDecl FuncDecl( decl )
			Next
			Emit "}"
			Return
		Endif
		
		Local bases$
		For Local iface:=Eachin classDecl.implments
			If bases bases+="," Else bases=" implements "
			bases+=iface.actual.munged
		Next
		
		Emit "class "+classid+" extends "+superid+bases+"{"
		
		For Local decl:=Eachin classDecl.Semanted

			Local tdecl:=FieldDecl( decl )
			If tdecl
				Emit TransDecl( tdecl )+"="+tdecl.init.Trans()+";"
				Continue
			Endif
			
			Local fdecl:=FuncDecl( decl )
			If fdecl
				EmitFuncDecl fdecl
				Continue
			Endif
			
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit "static "+TransDecl( gdecl )+";"
				Continue
			Endif
		Next
		
		Emit "}"

	End
	
	Method TransApp$( app:AppDecl )

		app.mainModule.munged="bb_"
		app.mainFunc.munged="bb_Main"
		
		For Local decl:=Eachin app.imported.Values()
			MungDecl decl
		Next

		For Local decl:=Eachin app.Semanted
		
			MungDecl decl

			Local cdecl:=ClassDecl( decl )
			If Not cdecl Continue
			
			PushMungScope

'			MungOverrides cdecl
			
			For Local decl:=Eachin cdecl.Semanted
			
				If FuncDecl( decl ) And Not FuncDecl( decl ).IsMethod()
					decl.ident=cdecl.ident+"_"+decl.ident
				Endif			
				
				MungDecl decl
			Next
			
			PopMungScope
		Next

		For Local decl:=Eachin app.Semanted
			
			Local cdecl:=ClassDecl( decl )
			If cdecl
				EmitClassDecl cdecl
			Endif
		Next
		
		For Local mdecl:=Eachin app.imported.Values()

			Emit "class "+mdecl.munged+"{"

			For Local decl:=Eachin mdecl.Semanted
				If decl.IsExtern() Or decl.scope.ClassScope() Continue
	
				Local gdecl:=GlobalDecl( decl )
				If gdecl
					Emit "static "+TransDecl( gdecl )+";"
					Continue
				Endif
				
				Local fdecl:=FuncDecl( decl )
				If fdecl
					EmitFuncDecl fdecl
					Continue
				Endif
			Next

			Emit "}"
		Next
		
		Emit "class bb_Init{"
		Emit "public static int Init(){"
		For Local decl:=Eachin app.semantedGlobals
			Emit TransGlobal( decl )+"="+decl.init.Trans()+";"
		Next
		Emit "return 0;"
		Emit "}"
		Emit "}"
		
		Return JoinLines()

	End
	
	Method PostProcess$( source$ )
		'
		'move package/imports to top
		'
		Local lines$[]=source.Split( "~n" )
		
		Local pkg$,imps$,code$,imped:=New StringMap<StringObject>
		
		For Local line$=Eachin lines
			'
			line+="~n"
			'
			If line.StartsWith( "package" )
				If pkg Err "Multiple package decls"
				pkg=line
  			Else If line.StartsWith( "import " ) 
				Local i=line.Find( ";" )
				If i=-1 InternalErr
				line=line[..i+1]
				If Not imped.Contains( line )
					imps+=line+"~n"
					imped.Insert line,line
				Endif
			Else
				code+=line
 			Endif
		Next
		Return "~n"+pkg+"~n"+imps+"~n"+code
	End
	
End

