
' Module trans.cstranslator
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class CsTranslator Extends Translator

	Method TransType$( ty:Type )
		If VoidType( ty ) Return "void"
		If BoolType( ty ) Return "bool"
		If IntType( ty ) Return "int"
		If FloatType( ty ) Return "float"
		If StringType( ty ) Return "String"
		If ArrayType( ty ) Return TransType( ArrayType(ty).elemType )+"[]"
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
		Local vdecl:=ValDecl( decl )
		If vdecl Return TransType( vdecl.ty )+" "+decl.munged
		InternalErr
	End
	
	Method TransArgs$( args:Expr[] )
		Local t$
		For Local arg:=Eachin args
			If t t+=","
			t+=arg.Trans()
		Next
		Return Bra( t )
	End
	
	'***** Utility *****
	
	Method TransLocalDecl$( munged$,init:Expr )
		Return TransType( init.exprType )+" "+munged+"="+init.Trans()
	End
	
	Method EmitEnter( func$ )
		Emit "bb_std_lang.pushErr();"
	End
	
	Method EmitSetErr( info$ )
		Emit "bb_std_lang.errInfo=~q"+info.Replace( "\","/" )+"~q;"
	End
	
	Method EmitLeave()
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
		If ty.ActualType().EqualsType( src.ActualType() ) Return expr
		
		If Not ObjectType( src ) Err "Can't convert from "+src.ToString()+" to "+ty.ToString()

		Return "(("+TransType(ty)+")"+Bra(expr)+")"
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
		Return "base."+decl.munged+TransArgs( args )
	End

	'***** Expressions *****
	
	Method TransConstExpr$( expr:ConstExpr )
		Return TransValue( expr.exprType,expr.value )
	End
	
	Method TransNewObjectExpr$( expr:NewObjectExpr )
		Local t$="(new "+expr.classDecl.actual.munged+"())"
		If expr.ctor t+="."+expr.ctor.actual.munged+TransArgs( expr.args )
		Return t
	End
	
	Method TransNewArrayExpr$( expr:NewArrayExpr )
		Local texpr$=expr.expr.Trans()
		Local elemTy:=ArrayType( expr.exprType ).elemType
		'
		If StringType( elemTy ) Return "bb_std_lang.stringArray"+Bra(texpr)
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
		Local dst:=expr.exprType
		Local src:=expr.expr.exprType
		Local uexpr$=expr.expr.Trans()
		Local texpr$=Bra( uexpr )
		
		If BoolType( dst )
			If BoolType( src ) Return texpr
			If IntType( src ) Return Bra( texpr+"!=0" )
			If FloatType( src ) Return Bra( texpr+"!=0.0f" )
			If StringType( src ) Return Bra( texpr+".Length!=0" )
			If ArrayType( src ) Return Bra( texpr+".Length!=0" )
			If ObjectType( src ) Return Bra( texpr+"!=null" )
		Else If IntType( dst )
			If BoolType( src ) Return Bra( texpr+"?1:0" )
			If IntType( src ) Return texpr
			If FloatType( src ) Return "(int)"+texpr
			If StringType( src ) Return "int.Parse"+texpr
		Else If FloatType( dst )
			If IntType( src ) Return "(float)"+texpr
			If FloatType( src ) Return texpr
'			If StringType( src ) Return "float.Parse"+texpr
			If StringType( src ) Return "float.Parse"+Bra(uexpr+",CultureInfo.InvariantCulture")
		Else If StringType( dst )
			If IntType( src ) Return texpr+".ToString()"
'			If FloatType( src ) Return texpr+".ToString()"
			If FloatType( src ) Return texpr+".ToString(CultureInfo.InvariantCulture)"
			If StringType( src ) Return texpr
		Endif
		
		If src.GetClass().ExtendsClass( dst.GetClass() )
			Return texpr
		Else If dst.GetClass().ExtendsClass( src.GetClass() )
			Local tmp:=New LocalDecl( "",src,Null )
			MungDecl tmp
			Emit TransType( src )+" "+tmp.munged+"="+expr.expr.Trans()+";"
			Return "($t is $c ? ($c)$t : null)".Replace( "$t",tmp.munged ).Replace( "$c",TransType(dst) )
		Endif

		Err "CS translator can't convert "+src.ToString()+" to "+dst.ToString()
	End
	
	Method TransUnaryExpr$( expr:UnaryExpr )
		Local pri=ExprPri( expr )
		Local t_expr$=TransSubExpr( expr.expr,pri )
		Return TransUnaryOp( expr.op )+t_expr
	End
	
	Method TransBinaryExpr$( expr:BinaryExpr )
		If BinaryCompareExpr( expr ) And StringType( expr.lhs.exprType ) And StringType( expr.rhs.exprType )
			Return Bra( TransSubExpr( expr.lhs )+".CompareTo("+expr.rhs.Trans()+")"+TransBinaryOp( expr.op,"" )+"0" )
		Endif
		Local pri=ExprPri( expr )
		Local t_lhs$=TransSubExpr( expr.lhs,pri )
		Local t_rhs$=TransSubExpr( expr.rhs,pri-1 )
		Return t_lhs+TransBinaryOp( expr.op,t_rhs )+t_rhs

	End
	
	Method TransIndexExpr$( expr:IndexExpr )
		Local t_expr:=TransSubExpr( expr.expr )
		Local t_index:=expr.index.Trans()
		If StringType( expr.expr.exprType ) Return "(int)"+t_expr+"["+t_index+"]"
		Return t_expr+"["+t_index+"]"
	End
	
	Method TransSliceExpr$( expr:SliceExpr )
		Local t_expr:=expr.expr.Trans()
		Local t_args:="0"
		If expr.from t_args=expr.from.Trans()
		If expr.term t_args+=","+expr.term.Trans()
		Return "(("+TransType( expr.exprType )+")bb_std_lang.slice("+t_expr+","+t_args+"))"
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
		Local id2$=id[..1].ToUpper()+id[1..]

		Select id

		'global functions
		Case "print" Return "bb_std_lang.Print"+Bra( arg0 )
		Case "error" Return "bb_std_lang.Error"+Bra( arg0 )

		'string/array methods
		Case "length"
			If StringType( expr.exprType ) Return texpr+".Length"
			Return "bb_std_lang.length"+Bra( texpr )
		
		'array methods
		Case "resize" Return "("+TransType( expr.exprType )+")bb_std_lang.resize"+Bra( texpr+","+arg0 )

		'string methods
		Case "compare" Return texpr+".CompareTo"+Bra( arg0 )
		Case "find" Return texpr+".IndexOf"+Bra( arg0+","+arg1 )
		Case "findlast" Return texpr+".LastIndexOf"+Bra( arg0 )
		Case "findlast2" Return texpr+".LastIndexOf"+Bra( arg0+","+arg1 )
		Case "trim" Return texpr+".Trim()"
		Case "join" Return "String.Join"+Bra( texpr+","+arg0 )
		Case "split" Return "bb_std_lang.split"+Bra( texpr+","+arg0 )
		Case "replace" Return texpr+".Replace"+Bra( arg0+","+arg1 )
		Case "tolower" Return texpr+".ToLower()"
		Case "toupper" Return texpr+".ToUpper()"
		Case "contains" Return Bra( texpr+".IndexOf"+Bra( arg0 )+"!=-1" )
		Case "startswith" Return texpr+".StartsWith"+Bra( arg0 )
		Case "endswith" Return texpr+".EndsWith"+Bra( arg0 )
		'string functions
		Case "fromchar" Return "new String"+Bra("(char)"+Bra( arg0 )+",1")

		'trig functions - degrees
		Case "sin","cos","tan" Return "(float)Math."+id2+Bra( Bra(arg0)+"*bb_std_lang.D2R" )
		Case "asin","acos","atan" Return "(float)"+Bra( "Math."+id2+Bra(arg0)+"*bb_std_lang.R2D" )
		Case "atan2" Return "(float)"+Bra( "Math."+id2+Bra(arg0+","+arg1)+"*bb_std_lang.R2D" )

		'trig functions - radians
		Case "sinr","cosr","tanr" Return "(float)Math."+id2[..-1]+Bra( arg0 )
		Case "asinr","acosr","atanr" Return "(float)Math."+id2[..-1]+Bra( arg0 )
		Case "atan2r" Return "(float)Math."+id2[..-1]+Bra( arg0+","+arg1 )

		'misc math functions
		Case "sqrt","floor","log","exp" Return "(float)Math."+id2+Bra(arg0)
		Case "ceil" Return "(float)Math.Ceiling"+Bra(arg0)
		Case "pow" Return "(float)Math."+id2+Bra( arg0+","+arg1 )

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

		Local t$=TransType( odecl.retType )+" "+decl.munged+Bra( args )

		If decl.ClassScope() And decl.ClassScope().IsInterface()
			Emit t+";"
		Else If decl.IsAbstract()
			If decl.overrides
				Emit "public abstract override "+t+";"
			Else
				Emit "public abstract "+t+";"
			Endif
		Else
			Local q$="public "
			If decl.IsStatic()
				q+="static "
			Else If decl.overrides And Not decl.IsCtor()
				q+="override "
			Else
				q+="virtual "
			Endif
			
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
	
		Local classid$=classDecl.munged
		Local superid$=classDecl.superClass.actual.munged
		
		If classDecl.IsInterface() 
		
			Local bases$
			For Local iface:=Eachin classDecl.implments
				If bases bases+="," Else bases=" : "
				bases+=iface.actual.munged
			Next
			
			Emit "interface "+classid+bases+"{"
			For Local decl:=Eachin classDecl.Semanted
				Local fdecl:=FuncDecl( decl )
				If Not fdecl Continue
				EmitFuncDecl fdecl
			Next
			Emit "}"
			Return
		Endif
	
		Local bases$=" : "+superid
		For Local iface:=Eachin classDecl.implments
			bases+=","+iface.actual.munged
		Next
		
		Local q$
		If classDecl.IsAbstract() q+="abstract "
		
		Emit q+"class "+classid+bases+"{"
		
		For Local decl:=Eachin classDecl.Semanted
			Local tdecl:=FieldDecl( decl )
			If tdecl
				Emit "public "+TransDecl( tdecl )+"="+tdecl.init.Trans()+";"
				Continue
			Endif
			
			Local fdecl:=FuncDecl( decl )
			If fdecl
				EmitFuncDecl fdecl
				Continue
			Endif
			
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit "public static "+TransDecl( gdecl )+";"
				Continue
			Endif
		Next
		
		Emit "}"
	End
	
	Method TransApp$( app:AppDecl )
		
		app.mainModule.munged="bb_"
		app.mainFunc.munged="bbMain"
		
		For Local decl:=Eachin app.imported.Values
			MungDecl decl
		Next

		For Local decl:=Eachin app.Semanted

			MungDecl decl

			Local cdecl:=ClassDecl( decl )
			If Not cdecl Continue
			
			PushMungScope
			
			For Local decl:=Eachin cdecl.Semanted
			
				If FuncDecl( decl ) And Not FuncDecl( decl ).IsMethod()
					decl.ident=cdecl.ident+"_"+decl.ident
				Endif
				
				MungDecl decl
			Next
			
			PopMungScope
		Next
		
		'classes		
		For Local decl:=Eachin app.Semanted
			
			Local cdecl:=ClassDecl( decl )
			If cdecl
				EmitClassDecl cdecl
				Continue
			Endif
		Next
		
		'Translate globals
		For Local mdecl:=Eachin app.imported.Values()

			Emit "class "+mdecl.munged+"{"

			For Local decl:=Eachin mdecl.Semanted
				If decl.IsExtern() Or decl.scope.ClassScope() Continue
			
				Local gdecl:=GlobalDecl( decl )
				If gdecl
					Emit "public static "+TransDecl( gdecl )+";"
					Continue
				Endif
				
				Local fdecl:=FuncDecl( decl )
				If fdecl
					EmitFuncDecl fdecl
					Continue
				Endif
			Next
			
			If mdecl=app.mainModule
				Emit "public static int bbInit(){"
				For Local decl:=Eachin app.semantedGlobals
					Emit TransGlobal( decl )+"="+decl.init.Trans()+";"
				Next
				Emit "return 0;"
				Emit "}"
			Endif

			Emit "}"
		Next
#rem		
		Emit "class bb_Init{"
		Emit "public static int Init(){"
		For Local decl:=Eachin app.semantedGlobals
			Emit TransGlobal( decl )+"="+decl.init.Trans()+";"
		Next
		Emit "return 0;"
		Emit "}"
		Emit "}"
#end		
		Return JoinLines()
	End
	
	Method PostProcess$( source$ )
		'
		'move using decls to top
		'
		Local lines$[]=source.Split( "~n" )
		'
		Local head$,usings$,code$,used:=New StringMap<StringObject>
	
		For Local line$=Eachin lines
			If line.StartsWith( "using " )
				Local i=line.Find( ";" )
				If i=-1 InternalErr
				line=line[..i+1]
				If Not used.Contains( line )
					usings+=line+"~n"
					used.Insert line,line
				Endif
			Else If usings
				code+=line+"~n"
			Else 
				head+=line+"~n"
 			Endif
		Next
		Return head+usings+code
	End
	
End
