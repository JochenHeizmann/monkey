
' Module trans.javatranslator
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class JavaTranslator Extends Translator

	Method TransType$( ty:Type )
		ty=ty.Actual()
		If VoidType( ty ) Return "void"
		If BoolType( ty ) Return "boolean"
		If IntType( ty ) Return "int"
		If FloatType( ty ) Return "float"
		If StringType( ty ) Return "String"
		If ArrayType( ty ) Return TransType( ArrayType( ty ).elemType )+"[]"
		If ObjectType( ty ) Return ObjectType( ty ).classDecl.munged
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
'			If ArrayType( ty ) Return Bra( "new "+TransType(ArrayType(ty).elemType)+"[0]" )
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
		EndIf
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
		EndIf
		InternalErr
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
		EndIf
		
		'upcast
		If src.ExtendsType( dst )
			Return texpr
		EndIf

		'downcast		
		If dst.ExtendsType( src )
			Local tmp:=New LocalDecl( "",src,expr.expr )
			MungDecl tmp,localMungs
			Emit TransDecl( tmp )+"="+tmp.init.Trans()+";"
			Return "($t instanceof $c ? ($c)$t : null)".Replace( "$t",tmp.munged ).Replace( "$c",TransType(dst) )
		EndIf
		
		InternalErr
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
#rem
		'array concatenation?		
		If expr.op="+" And ArrayType( expr.exprType )
			Return "(("+TransType( expr.exprType )+")bb_std_lang.concatArrays"+Bra( lhs+","+rhs )+")"
		EndIf
#end
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
		For Local elem:=EachIn expr.exprs
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
		localMungs=New StringMap<Decl>
		
		Local args$
		For Local arg:=EachIn decl.argDecls
			MungDecl arg,localMungs
			If args args+=","
			args+=TransDecl( arg )
		Next
		
		Local t$
		If decl.IsStatic() t+="static "
		
		Emit t+TransType( decl.retType )+" "+decl.munged+Bra( args )+"{"
		
		EmitBlock decl

		Emit "}"
	End
	
	Method EmitClassDecl( classDecl:ClassDecl )
		Local classid$=classDecl.munged
		Local superid$=classDecl.superClass.actual.munged
		
		Emit "class "+classid+" extends "+superid+"{"
		
		For Local decl:=EachIn classDecl.Semanted

			Local tdecl:=FieldDecl( decl )
			If tdecl
				Emit TransDecl( tdecl )+"="+tdecl.init.Trans()+";"
				Continue
			EndIf
			
			Local fdecl:=FuncDecl( decl )
			If fdecl
				EmitFuncDecl fdecl
				Continue
			EndIf
			
			Local gdecl:=GlobalDecl( decl )
			If gdecl
				Emit "static "+TransDecl( gdecl )+";"
				Continue
			EndIf
		Next
		
		Emit "}"

	End
	
	Method TransApp$( app:AppDecl )
	
		app.mainModule.munged="bb_"
		
		globalMungs=New StringMap<Decl>
		localMungs=globalMungs

		'***** Mung *****
		For Local decl:=EachIn app.Semanted
			
			MungDecl decl,globalMungs

			Local cdecl:=ClassDecl( decl )
			If cdecl
				localMungs=New StringMap<Decl>
				For Local decl:=EachIn cdecl.Semanted
					MungDecl decl,localMungs
				Next
				localMungs=globalMungs
			EndIf
		Next

		'Translate classes		
		For Local decl:=EachIn app.Semanted
			
			Local cdecl:=ClassDecl( decl )
			If cdecl
				EmitClassDecl cdecl
			EndIf
		Next
		
		'Translate globals
		For Local mdecl:=EachIn app.imported.Values()

			Emit "class "+mdecl.munged+"{"

			For Local decl:=EachIn mdecl.Semanted
				If decl.IsExtern() Or decl.scope.ClassScope() Continue
	
				Local gdecl:=GlobalDecl( decl )
				If gdecl
					Emit "static "+TransDecl( gdecl )+";"
					Continue
				EndIf
				
				Local fdecl:=FuncDecl( decl )
				If fdecl
					EmitFuncDecl fdecl
					Continue
				EndIf
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
		
		For Local line$=EachIn lines
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
				EndIf
			Else
				code+=line
 			EndIf
		Next
		Return "~n"+pkg+"~n"+imps+"~n"+code
	End
	
End

