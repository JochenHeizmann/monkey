
' interpreter translator

Private

Import trans

Import runtime

Alias Expr=runtime.Expr
Alias Stmt=runtime.Stmt
Alias FuncDecl=runtime.FuncDecl
Alias ClassDecl=runtime.ClassDecl

'Alias AssignStmt=runtime.AssignStmt

Function InternalErr()
	Error "InternalErr!"
End

Class ITranslator Extends trans.Translator

	Field _nfuncs,_funcs:=New StringMap<FuncDecl>
	Field _nglobals,_globals:=New StringMap<GlobalVar>
	Field _nclasses,_classes:=New StringMap<ClassDecl>
	Field _nfields,_fields:=New StringMap<FieldVar>
	Field _nlocals,_locals:=New StringMap<LocalVar>
	Field _methods:=New StringMap<Int>
	Field _blocks:=New Stack<Block>
	Field _retexpr:Expr
	Field _retstmt:Stmt
	
	Method Ret( expr:Expr )
		If _retexpr InternalErr
		_retexpr=expr
	End
	
	Method Ret( stmt:Stmt )
		If _retstmt InternalErr
		_retstmt=stmt
	End
	
	Method Trans:Expr( expr:trans.Expr )
		expr.Trans
		Local ret:=_retexpr
		_retexpr=Null
		Return ret
	End
	
	Method Transv:ValueExpr( expr:trans.Expr )
		Local ret:=Trans( expr )
		If ValueExpr( ret ) Return ValueExpr( ret )
		If BoolType( expr.exprType ) Return New BoolValueExpr( ret )
		If IntType( expr.exprType ) Return New IntValueExpr( ret )
		If FloatType( expr.exprType ) Return New FloatValueExpr( ret )
		If StringType( expr.exprType ) Return New StringValueExpr( ret )
		If ArrayType( expr.exprType ) Return New ArrayValueExpr( ret )
		InternalErr
	End
	
	Method Trans:Stmt( stmt:trans.Stmt )
		stmt.Trans
		Local ret:=_retstmt
		_retstmt=Null
		Return ret
	End
	
	Method Emit( stmt:Stmt )
		If Not stmt Return
		_blocks.Top().stmts.Push stmt
	End
	
	'could push/pop _nlocals...?
	Method Trans:Block( tblock:trans.BlockDecl )
		_blocks.Push New Block
		For Local stmt:=Eachin tblock.stmts
			_errinfo=stmt.errInfo
			Emit Trans( stmt )
		Next
		Return _blocks.Pop()
	End
	
	Method TransValue:Value( ty:trans.Type,value$ )
		If value
			If BoolType( ty ) Return New IntValue( Int(value) )
			If IntType( ty ) Return New IntValue( Int(value) )
			If FloatType( ty ) Return New FloatValue( Float(value) )
			If StringType( ty ) Return New StringValue( value )
		Else
			If BoolType( ty ) Return New IntValue(0)
			If IntType( ty ) Return New IntValue(0)
			If FloatType( ty ) Return New FloatValue(0)
			If StringType( ty ) Return New StringValue("")
			If ObjectType( ty ) Return runtime._null
		Endif
		InternalErr
	End
	
	Method TransArgs:ValueExpr[]( args:trans.Expr[] )
		Local vargs:=New ValueExpr[args.Length]
		For Local i=0 Until args.Length
			vargs[i]=Transv( args[i] )
		Next
		Return vargs
	End
	
	Method AllocFunc:FuncDecl( fdecl:trans.FuncDecl )
		If fdecl.munged InternalErr
		Local func:=New FuncDecl
		fdecl.munged=_nfuncs
		_funcs.Set fdecl.munged,func
		_nfuncs+=1
		Return func
	End

	Method AllocClass:ClassDecl( cdecl:trans.ClassDecl )
		If cdecl.munged InternalErr
		Local clas:=New ClassDecl
		cdecl.munged=_nclasses
		_classes.Set cdecl.munged,clas
		_nclasses+=1
		Return clas
	End

	Method AllocMethod:FuncDecl( fdecl:trans.FuncDecl,index )
		Local func:=AllocFunc( fdecl )
		_methods.Set fdecl.munged,index
		Return func
	End
	
	Method AllocField:FieldVar( tdecl:trans.FieldDecl,index )
		If tdecl.munged InternalErr
		Local var:=New FieldVar( index )
		tdecl.munged=_nfields
		_fields.Set tdecl.munged,var
		_nfields+=1
		Return var
	End
	
	Method AllocGlobal:GlobalVar( decl:trans.GlobalDecl )
		If decl.munged InternalErr
		decl.munged=_nglobals
		Local var:=New GlobalVar( _nglobals )
		_globals.Set decl.munged,var
		_nglobals+=1
		Return var
	End	
	
	Method AllocLocal:LocalVar( decl:trans.LocalDecl )
		If decl.munged InternalErr
		decl.munged=_nlocals
		Local var:=New LocalVar( _nlocals )
		_locals.Set decl.munged,var
		_nlocals+=1
		Return var
	End	

	'***** Expressions *****
		
	Method TransConstExpr$( expr:trans.ConstExpr )
		Ret New ConstValueExpr( TransValue( expr.exprType,expr.value ) )
	End
	
	Method TransNewObjectExpr$( expr:trans.NewObjectExpr )
		Local clas:=_classes.Get( expr.classDecl.munged )
		Local ctor:=_funcs.Get( expr.ctor.munged )
		Local args:=TransArgs( expr.args )
		Ret New runtime.NewObjectExpr( clas,ctor,args )
	End
	
	Method TransNewArrayExpr$( expr:trans.NewArrayExpr )
		Ret New runtime.NewArrayExpr( Trans( expr.expr ),TransValue( expr.ty,"" ) )
	End
	
	Method TransSelfExpr$( expr:trans.SelfExpr )
		Ret New runtime.SelfExpr
	End
	
	Method TransCastExpr$( expr:trans.CastExpr )
		Ret Transv( expr.expr )
	End
	
	Method TransUnaryExpr$( expr:trans.UnaryExpr )
		Select expr.op
		Case "+"
			Ret Trans( expr.expr )
		Case "-"
			Ret New runtime.NegExpr( Trans( expr.expr ) )
		Case "not"
			Ret New runtime.NotExpr( Trans( expr.expr ) )
		Case "~"
			Ret New runtime.BNotExpr( Trans( expr.expr ) )
		End
	End
	
	Method TransBinaryExpr$( expr:trans.BinaryExpr )
		Select expr.op
		Case "*" 
			Ret New runtime.MulExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "/" 
			Ret New runtime.DivExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "mod" 
			Ret New runtime.ModExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "+" 
			Ret New runtime.AddExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "-" 
			Ret New runtime.SubExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "shl"
			Ret New runtime.ShlExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "shr"
			Ret New runtime.ShrExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "and"
			Ret New runtime.AndExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "or"
			Ret New runtime.OrExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "&"
			Ret New runtime.BAndExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "|"
			Ret New runtime.BOrExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "~"
			Ret New runtime.BXorExpr( Trans( expr.lhs ),Trans( expr.rhs ) )
		Case "="
			Ret New runtime.EqExpr( Transv( expr.lhs),Transv( expr.rhs ) )
		Case "<>"
			Ret New runtime.NeExpr( Transv( expr.lhs ),Transv( expr.rhs ) )
		Case "<"
			Ret New runtime.LtExpr( Transv( expr.lhs ),Transv( expr.rhs ) )
		Case "<="
			Ret New runtime.LeExpr( Transv( expr.lhs ),Transv( expr.rhs ) )
		Case ">"
			Ret New runtime.GtExpr( Transv( expr.lhs ),Transv( expr.rhs ) )
		Case ">="
			Ret New runtime.GeExpr( Transv( expr.lhs ),Transv( expr.rhs ) )
		Default
			InternalErr
		End
	End
	
	Method TransIndexExpr$( expr:trans.IndexExpr )
		If StringType( expr.expr.exprType )
			Print "HERE!"
			Ret New runtime.IndexStringExpr( Trans( expr.expr ),Trans( expr.index ) )
		Else
			Ret New runtime.IndexExpr( Trans( expr.expr ),Trans( expr.index ) )
		Endif
	End
	
	Method TransSliceExpr$( expr:trans.SliceExpr )
		If StringType( expr.expr.exprType )
			Ret New runtime.SliceStringExpr( Trans( expr.expr ),Trans( expr.from ),Trans( expr.term ) )
		Else
			Ret New runtime.SliceExpr( Trans( expr.expr ),Trans( expr.from ),Trans( expr.term ) )
		Endif
	End
	
	Method TransArrayExpr$( expr:trans.ArrayExpr )
		Ret New runtime.ArrayExpr( TransArgs( expr.exprs ) )
	End
	
	Method TransStmtExpr$( expr:trans.StmtExpr )
		Emit Trans( expr.stmt )
		Ret Trans( expr.expr )
	End
	
	Method TransVarExpr$( expr:trans.VarExpr )
		Local decl:=expr.decl
		
		If LocalDecl( decl ) 
			Ret _locals.Get( decl.munged )
		Else If FieldDecl( decl )
			Ret _fields.Get( decl.munged )
		Else If GlobalDecl( decl )
			Ret _globals.Get( decl.munged )
		Else
			InternalErr
		Endif
	End
	
	Method TransMemberVarExpr$( expr:trans.MemberVarExpr )
		Local decl:=expr.decl

		If decl.munged.StartsWith( "$" )
			InternalErr
		Endif
		
		Local var:=_fields.Get( decl.munged )
		
		If expr.expr
			Ret New runtime.MemberExpr( Transv( expr.expr ),var )
		Else
			Print "!!!!!"
			InternalErr
			Ret var
		Endif
	End
	
	Method TransInvokeExpr$( expr:trans.InvokeExpr )
		Local decl:=expr.decl

		If decl.munged.StartsWith( "$" )
			Local a0:Expr,a1:Expr
			If expr.args.Length And expr.args[0] a0=Trans( expr.args[0] )
			If expr.args.Length>1 And expr.args[1] a1=Trans( expr.args[1] )
			Select decl.munged[1..]
			Case "print"
				Ret New PrintExpr( a0 )
			Case "error"
				Ret New ErrorExpr( a0 )
			Case "sin"
				Ret New MathSin( a0 )
			Case "cos"
				Ret New MathCos( a0 )
			Case "tan"
				Ret New MathTan( a0 )
			Case "asin"
				Ret New MathASin( a0 )
			Case "acos"
				Ret New MathACos( a0 )
			Case "atan"
				Ret New MathATan( a0 )
			Case "atan2"
				Ret New MathATan2( a0,a1 )
			Case "sinr"
				Ret New MathSinr( a0 )
			Case "cosr"
				Ret New MathCosr( a0 )
			Case "tanr"
				Ret New MathTanr( a0 )
			Case "asinr"
				Ret New MathASinr( a0 )
			Case "acosr"
				Ret New MathACosr( a0 )
			Case "atanr"
				Ret New MathATanr( a0 )
			Case "atan2r"
				Ret New MathATan2r( a0,a1 )
			Case "sqrt"
				Ret New MathSqrt( a0 )
			Case "floor"
				Ret New MathFloor( a0 )
			Case "ceil"
				Ret New MathCeil( a0 )
			Case "log"
				Ret New MathLog( a0 )
			Case "exp"
				Ret New MathExp( a0 )
			Case "pow"
				Ret New MathPow( a0,a1 )
			Default
				InternalErr
			End
			Return
		Endif

		Local args:=TransArgs( expr.args )
		
		If decl.IsMethod()
			Local meth:=_methods.Get( decl.munged )
			Ret New InvokeMethodExpr( meth,args )
		Else
			Local func:=_funcs.Get( decl.munged )
			Ret New InvokeFuncExpr( func,args )
		Endif
	End
	
	Method TransInvokeMemberExpr$( expr:trans.InvokeMemberExpr )
		Local decl:=expr.decl
		
		If decl.munged.StartsWith( "$" )
			Local ex:=Trans( expr.expr ),a0:Expr,a1:Expr
			If expr.args.Length And expr.args[0] a0=Trans( expr.args[0] )
			If expr.args.Length>1 And expr.args[1] a1=Trans( expr.args[1] )
			
			Select decl.munged[1..]
			Case "length"
				If StringType( expr.expr.exprType )
					Ret New StringLength( ex )
				Else
					Ret New ArrayLength( ex )
				Endif
			Case "resize"
				Ret New ArrayResize( ex,a0,TransValue( ArrayType(expr.exprType).elemType,"" ) )
			Case "compare"
				Ret New StringCompare( ex,a0 )
			Case "contains"
				Ret New StringContains( ex,a0 )
			Case "endswith"
				Ret New StringEndsWith( ex,a0 )
			Case "find"
				Ret New StringFind( ex,a0,a1 )
			Case "findlast"
				Ret New StringFindLast( ex,a0 )
			Case "findlast2"
				Ret New StringFindLast2( ex,a0,a1 )
			Case "trim"
				Ret New StringTrim( ex )
			Case "join"
				Ret New StringJoin( ex,a0 )
			Case "split"
				Ret New StringSplit( ex,a0 )
			Case "replace"
				Ret New StringReplace( ex,a0,a1 )
			Case "tolower"
				Ret New StringToLower( ex )
			Case "toupper"
				Ret New StringToUpper( ex )
			Case "startswith"
				Ret New StringStartsWith( ex,a0 )
			Default
				InternalErr
			End
			Return
		Endif

		If decl.IsMethod()
			Local args:=TransArgs( expr.args )
			Local meth:=_methods.Get( decl.munged )
			Local inv:=New InvokeMethodExpr( meth,args )
			Ret New MemberExpr( Transv( expr.expr ),inv )
		Else
			InternalErr
		Endif
	End
	
	Method TransInvokeSuperExpr$( expr:trans.InvokeSuperExpr )
		Local func:=_funcs.Get( expr.funcDecl.munged )
		Local args:=TransArgs( expr.args )
		Ret New InvokeFuncExpr( func,args )
	End
	
	Method TransExprStmt$( stmt:trans.ExprStmt )
		Ret New runtime.ExprStmt( Transv( stmt.expr ) )
	End

	Method TransAssignStmt$( stmt:trans.AssignStmt )
		If Not stmt.rhs
			Ret Trans( stmt.lhs )
			Return
		Endif
		
		If stmt.tmp1
			Local var:=AllocLocal( stmt.tmp1 )
			Emit New LocalDeclStmt( var.index,Transv( stmt.tmp1.init ) )
		Endif

		If stmt.tmp2
			Local var:=AllocLocal( stmt.tmp2 )
			Emit New LocalDeclStmt( var.index,Transv( stmt.tmp2.init ) )
		Endif
	
		Ret New runtime.AssignStmt( Transv( stmt.lhs ),Transv( stmt.rhs ) )
	End
	
	Method TransReturnStmt$( stmt:trans.ReturnStmt )
		If stmt.expr
			Ret New runtime.ReturnStmt( Transv( stmt.expr ) )
		Else
			Ret New runtime.ReturnStmt( Null )
		Endif
	End
	
	Method TransContinueStmt$( stmt:trans.ContinueStmt )
		Ret New runtime.ContinueStmt
	End
	
	Method TransBreakStmt$( stmt:trans.BreakStmt )
		Ret New runtime.ExitStmt
	End

	Method TransDeclStmt$( stmt:trans.DeclStmt )
		Local decl:=LocalDecl( stmt.decl )
		If decl
			Local var:=AllocLocal( decl )
			Ret New runtime.LocalDeclStmt( var.index,Transv( decl.init ) )
			Return
		Endif
		InternalErr
	End
	
	Method TransIfStmt$( stmt:trans.IfStmt )
		Local expr:=Trans( stmt.expr )
		Local thenBlock:=Trans( stmt.thenBlock )
		Local elseBlock:=Trans( stmt.elseBlock )
		Ret New runtime.IfStmt( expr,thenBlock,elseBlock )
	End
	
	Method TransWhileStmt$( stmt:trans.WhileStmt )
		Local expr:=Trans( stmt.expr )
		Local block:=Trans( stmt.block )
		Ret New runtime.WhileStmt( expr,block )
	End

	Method TransRepeatStmt$( stmt:trans.RepeatStmt )
		Local block:=Trans( stmt.block )
		Local expr:=Trans( stmt.expr )
		Ret New runtime.RepeatStmt( block,expr )
	End

	Method TransForStmt$( stmt:trans.ForStmt )
		Local init:=Trans( stmt.init )
		Local expr:=Trans( stmt.expr )
		Local incr:=Trans( stmt.incr )
		Local block:=Trans( stmt.block )
		Ret New runtime.ForStmt( init,expr,incr,block )
	End
	
	Method TransFuncDecl:FuncDecl( fdecl:trans.FuncDecl )
		Local func:=_funcs.Get( fdecl.munged )
		
		_nlocals=0
		_locals.Clear

		For Local arg:=Eachin fdecl.argDecls
			Local var:=AllocLocal( arg )
		Next

		func.block=Trans( fdecl )
		func.locals=_nlocals
	End
	
	Method TransClassDecl:ClassDecl( cdecl:trans.ClassDecl )

		Local clas:=_classes.Get( cdecl.munged )
		
		If Not cdecl.superClass.IsExtern()
			Local sclas:=_classes.Get( cdecl.superClass.munged )
			clas.superclass=clas
			For Local i=0 Until sclas.fields.Length
				clas.fields[i]=sclas.fields[i]
			Next
			For Local i=0 Until sclas.methods.Length
				clas.methods[i]=sclas.methods[i]
			Next
		End
		
		For Local decl:=Eachin cdecl.Semanted
		
			Local gdecl:=trans.GlobalDecl( decl )
			If gdecl
				Local var:=_globals.Get( gdecl.munged )
				_globalinits[var.index]=Transv( gdecl.init )
				Continue
			Endif

			Local tdecl:=trans.FieldDecl( decl )
			If tdecl
				Local var:=_fields.Get( tdecl.munged )
				clas.fields[var.index]=Transv( tdecl.init )
				Continue
			Endif

			Local fdecl:=trans.FuncDecl( decl )
			
			If fdecl And fdecl.IsMethod()
				TransFuncDecl fdecl
				Local func:=_funcs.Get( fdecl.munged )
				Local index:=_methods.Get( fdecl.munged )
				clas.methods[index]=func
				Continue
			End
			
			If fdecl
				TransFuncDecl fdecl
				Continue
			End

		Next
		
	End
	
	Method TransBlock$( block:BlockDecl )
		Emit Trans( block )
	End
	
	Method TransApp$( app:AppDecl )
	
		For Local decl:=Eachin app.Semanted
		
			Local gdecl:=trans.GlobalDecl( decl )
			If gdecl
				Local var:=AllocGlobal( gdecl )
				Continue
			Endif
		
			Local fdecl:=trans.FuncDecl( decl )
			If fdecl
				Local func:=AllocFunc( fdecl )
				Continue
			Endif
			
			Local cdecl:=trans.ClassDecl( decl )
			If cdecl
			
				Local clas:=AllocClass( cdecl )
				
				Local nfields,nmethods
				
				If Not cdecl.superClass.IsExtern()
					Local sclas:=_classes.Get( cdecl.superClass.munged )
					nfields=sclas.fields.Length
					nmethods=sclas.methods.Length
				End

				For Local decl:=Eachin cdecl.Semanted
				
					Local gdecl:=trans.GlobalDecl( decl )
					If gdecl
						Local var:=AllocGlobal( gdecl )
						Continue
					Endif
		
					Local tdecl:=trans.FieldDecl( decl )
					If tdecl
						Local var:=AllocField( tdecl,nfields )
						nfields+=1
						Continue
					End
					
					Local fdecl:=trans.FuncDecl( decl )
					
					If fdecl And fdecl.IsMethod()
						If fdecl.overrides
							Local index:=_methods.Get( fdecl.overrides.munged )
							Local func:=AllocMethod( fdecl,index )
						Else
							Local func:=AllocMethod( fdecl,nmethods )
							nmethods+=1
						Endif
						Continue
					Endif
					
					If fdecl
						Local func:=AllocFunc( fdecl )
						Continue
					Endif

				Next
				
				clas.fields=New ValueExpr[nfields]
				clas.methods=New FuncDecl[nmethods]

				Continue
			Endif
		Next
		
		runtime._globalinits=New ValueExpr[_nglobals]

		For Local decl:=Eachin app.Semanted
		
			Local gdecl:=trans.GlobalDecl( decl )
			If gdecl
				Local var:=_globals.Get( gdecl.munged )
				runtime._globalinits[var.index]=Transv( gdecl.init )
				Continue
			Endif
		
			Local fdecl:=trans.FuncDecl( decl )
			If fdecl
				TransFuncDecl fdecl
				Continue
			Endif
			
			Local cdecl:=trans.ClassDecl( decl )
			If cdecl
				TransClassDecl cdecl
				Continue
			Endif
			
		Next

		runtime._globals=New Value[_nglobals]

		For Local i=0 Until _nglobals
			runtime._globals[i]=_globalinits[i].Eval()
		Next
		
		Local func:=_funcs.Get( app.mainFunc.munged )
		
		func.Exec( [] )

	End
	
End

Public

Function Run( source$ )

	ENV_LANG="cpp"
	ENV_CONFIG="release"
	ENV_MODPATH=".;c:\dropbox\monkeydev\modules"
	
	CONFIG_RELEASE=True
	
	Env.Set "HOST",ENV_HOST
	Env.Set "LANG",ENV_LANG
	Env.Set "TARGET",ENV_TARGET
	Env.Set "CONFIG",ENV_CONFIG
	
	Local path:="test.txt"
	
	path=RealPath( path )

	Local app:AppDecl=New AppDecl
	
	Local toker:Toker=New Toker( "_main_.monkey",source )
	
	Local parser:Parser=New Parser( toker,app )
	
	parser.ParseMain
	
	app.Semant

	_trans=New ITranslator
	
	_trans.TransApp app
	
End

