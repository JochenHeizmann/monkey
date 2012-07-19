
' Module trans.parser
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Global FILE_EXT$="monkey"

Class ScopeExpr Extends Expr
	Field scope:ScopeDecl

	Method New( scope:ScopeDecl )
		Self.scope=scope
	End
	
	Method Semant:Expr()
		SyntaxErr
	End

	Method SemantScope:ScopeDecl()
		Return scope
	End
End

Class ForEachinStmt Extends Stmt
	Field varid$
	Field varty:Type
	Field varlocal
	Field expr:Expr
	Field block:BlockDecl
	
	Field stmts:List=New List
	
	Method New( varid$,varty:Type,varlocal,expr:Expr,block:BlockDecl )
		Self.varid=varid
		Self.varty=varty
		Self.varlocal=varlocal
		Self.expr=expr
		Self.block=block
	End
	
	Method OnSemant()
		expr=expr.Semant()
		
		If ArrayType( expr.exprType ) Or StringType( expr.exprType )
		
			Local exprTmp:LocalDecl=New LocalDecl( "",Null,expr )
			Local indexTmp:LocalDecl=New LocalDecl( "",Null,New ConstExpr( Type.intType,"0" ) )

			Local lenExpr:Expr=New IdentExpr( "Length",New VarExpr( exprTmp ) )

			Local cmpExpr:Expr=New BinaryCompareExpr( "<",New VarExpr( indexTmp ),lenExpr )
			
			Local indexExpr:Expr=New IndexExpr( New VarExpr( exprTmp ),New VarExpr( indexTmp ) )
			Local addExpr:Expr=New BinaryMathExpr( "+",New VarExpr( indexTmp ),New ConstExpr( Type.intType,"1" ) )
			
			block.stmts.AddFirst New AssignStmt( "=",New VarExpr( indexTmp ),addExpr )
			
			If varlocal
				Local varTmp:LocalDecl=New LocalDecl( varid,varty,indexExpr )
				block.stmts.AddFirst New DeclStmt( varTmp )
			Else
				block.stmts.AddFirst New AssignStmt( "=",New IdentExpr( varid ),indexExpr )
			Endif
			
			Local whileStmt:WhileStmt=New WhileStmt( cmpExpr,block )
			
			block=New BlockDecl( block.scope )
			block.AddStmt New DeclStmt( exprTmp )
			block.AddStmt New DeclStmt( indexTmp )
			block.AddStmt whileStmt
		
		Else If ObjectType( expr.exprType )

			Local enumerInit:Expr=New FuncCallExpr( New IdentExpr( "ObjectEnumerator",expr ),[] )
			Local enumerTmp:LocalDecl=New LocalDecl( "",Null,enumerInit )

			Local hasNextExpr:Expr=New FuncCallExpr( New IdentExpr( "HasNext",New VarExpr( enumerTmp ) ),[] )
			Local nextObjExpr:Expr=New FuncCallExpr( New IdentExpr( "NextObject",New VarExpr( enumerTmp ) ),[] )

			If varlocal
				Local varTmp:LocalDecl=New LocalDecl( varid,varty,nextObjExpr )
				block.stmts.AddFirst New DeclStmt( varTmp )
			Else
				block.stmts.AddFirst New AssignStmt( "=",New IdentExpr( varid ),nextObjExpr )
			Endif
			
			Local whileStmt:WhileStmt=New WhileStmt( hasNextExpr,block )
			
			block=New BlockDecl( block.scope )
			block.AddStmt New DeclStmt( enumerTmp )
			block.AddStmt whileStmt

		Else
			SyntaxErr
		EndIf
		
		block.Semant
	End
	
	Method Trans$()
		_trans.EmitBlock block
	End

End

Class IdentExpr Extends Expr
	Field ident$
	Field expr:Expr

	Method New( ident$,expr:Expr=Null )
		Self.ident=ident
		Self.expr=expr
	End
	
	Method ToString$()
		Return ident
	End
	
	Method IdentScope:ScopeDecl()
		If Not expr Return _env
		Local scope:ScopeDecl=expr.SemantScope()
		If scope
			expr=Null
		Else
			expr=expr.Semant()
			scope=expr.exprType.GetClass()
			If Not scope Err "Expression has no scope."
		EndIf
		Return scope
	End
	
	Method IdentErr( scope:ScopeDecl )
		If scope
			Local close$
			For Local decl:=Eachin scope.Decls
				If ident.ToLower()=decl.ident.ToLower()
					close=decl.ident
				Endif
			Next
			If close
				Err "Identifier '"+ident+"' not found - perhaps you meant '"+close+"'?"
			Endif
		Endif
		Err "Identifier '"+ident+"' not found."
	End
	
	Method IdentNotFound()
	End
	
	Method IsVar()
		InternalErr
	End
	
	Method Semant:Expr()
		Return SemantSet( "",null )
	End
	
	Method SemantSet:Expr( op$,rhs:Expr )
	
		Local scope:ScopeDecl=IdentScope()
	
		Local vdecl:ValDecl=scope.FindValDecl( ident )
		If vdecl
			If ConstDecl( vdecl )
				If rhs Err "Constant '"+ident+"' cannot be modified."
				Return New ConstExpr( vdecl.ty,ConstDecl( vdecl ).value ).Semant()
			Else If FieldDecl( vdecl )
			
				If expr Return New MemberVarExpr( expr,VarDecl( vdecl ) ).Semant()
				
				If scope<>_env Or Not _env.FuncScope() Or _env.FuncScope().IsStatic() Err "Field '"+ident+"' cannot be accessed from here."
			EndIf

			Return New VarExpr( VarDecl( vdecl ) ).Semant()
		EndIf
		
		If op And op<>"="

			Local fdecl:FuncDecl=scope.FindFuncDecl( ident,[] )
			If Not fdecl IdentErr scope	'Err "identifier '"+ident+"' not found"

			If _env.ModuleScope().IsStrict() And Not fdecl.IsProperty() Err "Identifier '"+ident+"' cannot be used in this way."
			Local lhs:Expr
			
			If fdecl.IsStatic() Or (scope=_env And Not _env.FuncScope().IsStatic())
				lhs=New InvokeExpr( fdecl,[] )
			Else If expr
				Local tmp:LocalDecl=New LocalDecl( "",Null,expr )
				lhs=New InvokeMemberExpr( New VarExpr( tmp ),fdecl,[] )
				lhs=New StmtExpr( New DeclStmt( tmp ),lhs )
			Else
				Return Null
			EndIf
			
			Local bop$=op[..1]
			Select bop
			Case "*","/","shl","shr","+","-","&","|","~"
				rhs=New BinaryMathExpr( bop,lhs,rhs )
			Default
				InternalErr
			End Select
			rhs=rhs.Semant()
		EndIf
		
		Local args:Expr[]
		If rhs args=[rhs]
		
		Local fdecl:FuncDecl=scope.FindFuncDecl( ident,args )
		If Not fdecl IdentErr scope	'Err "identifier '"+ident+"' not found"

		If _env.ModuleScope().IsStrict() And Not fdecl.IsProperty() Err "Identifier '"+ident+"' cannot be used in this way."
		
		If Not fdecl.IsStatic()
			If expr Return New InvokeMemberExpr( expr,fdecl,args ).Semant()
			If scope<>_env Or _env.FuncScope().IsStatic() Err "Method '"+ident+"' cannot be accessed from here."
		EndIf

		Return New InvokeExpr( fdecl,args ).Semant()
	End

	Method SemantFunc:Expr( args:Expr[] )

		Local scope:ScopeDecl=IdentScope()
		
		Local fdecl:FuncDecl=scope.FindFuncDecl( ident,args )
		If fdecl
			If Not fdecl.IsStatic()
				If expr Return New InvokeMemberExpr( expr,fdecl,args ).Semant()
				If scope<>_env Or _env.FuncScope().IsStatic() Err "Method '"+ident+"' cannot be accessed from here."
			EndIf
			Return New InvokeExpr( fdecl,args ).Semant()
		EndIf
		
		If args.Length=1 And args[0] And ObjectType( args[0].exprType )
			Local cdecl:ClassDecl=ClassDecl( scope.FindScopeDecl( ident ) )
			If cdecl Return New CastExpr( New ObjectType( cdecl ),args[0],CAST_EXPLICIT ).Semant()
		Endif
		
		IdentErr scope
		
'		Err "function '"+ident+"' not found"

		Return Null
	End
	
	Method SemantScope:ScopeDecl()
		If Not expr Return _env.FindScopeDecl( ident )
		Local scope:ScopeDecl=expr.SemantScope()
		If scope Return scope.FindScopeDecl( ident )
	End
	
End

Class FuncCallExpr Extends Expr
	Field expr:Expr
	Field args:Expr[]
	
	Method New( expr:Expr,args:Expr[] )
		Self.expr=expr
		Self.args=args
		
	End
	
	Method Semant:Expr()
		SemantArgs args
		Return expr.SemantFunc( args )
	End

End

'***** Parser *****
Class Parser

	Field _toker:Toker
	Field _toke$
	Field _tokeType
	Field _tokerStack:=New List<Toker>
	
	Field _block:BlockDecl
	Field _blockStack:=New List<BlockDecl>
	Field _errStack:=New StringList
	
	Field _app:AppDecl
	Field _module:ModuleDecl
		
	Method SetErr()
		If _toker.Path
			_errInfo=_toker.Path+"<"+_toker.Line()+">"
		EndIf
	End
	
	Method PushBlock( block:BlockDecl )
		_blockStack.AddLast _block
		_errStack.AddLast _errInfo
		_block=block
	End
	
	Method PopBlock()
		_block=_blockStack.RemoveLast()
		_errInfo=_errStack.RemoveLast()
	End
	
	Method RealPath$( path$ )	
		Local popDir$=CurrentDir()
		ChangeDir ExtractDir( _toker.Path() )
		path=system.RealPath( path )
		ChangeDir popDir
		Return path
	End
	
	Method NextToke$()
		Local toke$=_toke
		
		Repeat
			_toke=_toker.NextToke()
			_tokeType=_toker.TokeType()
		Until _tokeType<>TOKE_SPACE
		
		If toke="," SkipEols
		
		Return _toke
	End
	
	Method Parse( toke$ )
		If _toke<>toke SyntaxErr
		NextToke
	End
	
	Method CParse( toke$ )
		If _toke<>toke Return False
		NextToke
		Return True
	End

	Method AtEos()
		Return _toke="" Or _toke="~n" Or _toke="else"
	End
	
	Method SkipEols()
		While CParse( "~n" )
		Wend
		SetErr
	End
	
	Method ParseStringLit$()
		If _tokeType<>TOKE_STRINGLIT Err "Expecting string literal."
		Local str$=BmxUnquote( _toke )
		NextToke
		Return str
	End
	
	Method ParseIdent$()
		Select _toke
		Case "@" NextToke
		Case "string","array","object"
		Default	If _tokeType<>TOKE_IDENT SyntaxErr
		End
		Local id$=_toke
		NextToke
		Return id
	End
	
	Method ParseIdentType:IdentType()
		Local id$=ParseIdent()
		If CParse( "." ) id+="."+ParseIdent()
		Local args:IdentType[]
		If CParse( "<" )
			Local nargs
			Repeat
				Local arg:IdentType=ParseIdentType()
				If args.Length=nargs args=args.Resize( nargs+10 )
				args[nargs]=arg
				nargs+=1
			Until Not CParse(",")
			args=args[..nargs]
			Parse ">"
		EndIf
		Return New IdentType( id,args )
	End
	
	Method ParseNewType:Type()
		If CParse( "void" ) Return Type.voidType
		If CParse( "bool" ) Return Type.boolType
		If CParse( "int" ) Return Type.intType
		If CParse( "float" ) Return Type.floatType
		If CParse( "string" ) Return Type.stringType
		If CParse( "object" ) Return Type.objectType
		Return ParseIdentType()
	End
	
	Method ParseDeclType:Type()
		Local ty:Type
		Select _toke
		Case "?"
			NextToke
			ty=Type.boolType
		Case "%"
			NextToke
			ty=Type.intType
		Case "#"
			NextToke
			ty=Type.floatType
		Case "$"
			NextToke
			ty=Type.stringType
		Case ":"
			NextToke
			ty=ParseNewType()
		Default
			If _module.IsStrict() SyntaxErr
			ty=Type.intType
		End Select
		While CParse( "[]" )
			ty=New ArrayType( ty )
		Wend
		Return ty
	End

	Method ParseStmtArgs:Expr[]()
		Local args:Expr[]
		If CParse( "()" ) Return args
		Local nargs
		If Not AtEos()
 			Repeat
				Local arg:Expr
				If Not AtEos() And _toke<>"," arg=ParseExpr()
				If args.Length=nargs args=args.Resize( nargs+10 )
				args[nargs]=arg
				nargs+=1
				If CParse( ")" ) Exit 'stmt args kludge!
				If AtEos() Exit
				Parse ","
 			Forever
		EndIf
		Return args[..nargs]
	End
	
	Method ParseArgs:Expr[]( stmt )
		If stmt Return ParseStmtArgs()
		Local args:Expr[]
		If CParse( "()" ) Return args
		If Not CParse( "(" ) Return args
		Local nargs
		If _toke<>")"
 			Repeat
				Local arg:Expr
				If _toke<>")" And _toke<>"," arg=ParseExpr()
				If args.Length=nargs args=args.Resize( nargs+10 )
				args[nargs]=arg
				nargs+=1
				If _toke=")" Exit
				Parse ","
 			Forever
  		EndIf
		Parse ")"
		Return args[..nargs]
	End
	
	Method ParseArrayExpr:ArrayExpr()
		Parse "["
		Local args:Expr[],nargs
		Repeat
			Local arg:Expr=ParseExpr()
			If args.Length=nargs args=args.Resize( nargs+10 )
			args[nargs]=arg
			nargs+=1
		Until Not CParse(",")
		args=args[..nargs]
		Parse "]"
		Return New ArrayExpr( args )
	End
	
	Method ParsePrimaryExpr:Expr( stmt )
	
		Local expr:Expr
		Select _toke
		Case "("
			NextToke
			expr=ParseExpr()
			If _toke="," Return expr	'stmt args kludge!
			Parse ")"
		Case "["
			expr=ParseArrayExpr()
		Case "[]"
			NextToke
			expr=New ConstExpr( Type.emptyArrayType,"" )
		Case "."
			expr=New ScopeExpr( _module )
		Case "new"
			NextToke
			Local ty:Type=ParseNewType()
			If CParse( "[" )
				Local len:=ParseExpr()
				Parse "]"
				While CParse( "[]" )
					ty=New ArrayType( ty)
				Wend
				expr=New NewArrayExpr( ty,len )
			Else
				expr=New NewObjectExpr( ty,ParseArgs( stmt ) )
			EndIf
		Case "null"
			NextToke
			expr=New ConstExpr( Type.nullObjectType,"" )
		Case "true"
			NextToke
			expr=New ConstExpr( Type.boolType,"1" )
		Case "false"
			NextToke
			expr=New ConstExpr( Type.boolType,"" )
		Case "bool","int","float","string","array","object"
			Local id$=_toke
			Local ty:Type=ParseNewType()
			If CParse( "(" )
				expr=ParseExpr()
				Parse ")"
				expr=New CastExpr( ty,expr,CAST_EXPLICIT )
			Else
				expr=New IdentExpr( id )
			EndIf
		Case "self"
			NextToke
			expr=New SelfExpr
		Case "super"
			NextToke
			Parse "."
			If _toke="new"
				Err "Call to super class constructor must be first statement in a constructor."
			EndIf
			Local id$=ParseIdent()
			expr=New InvokeSuperExpr( id,ParseArgs( stmt ) )
		Default
			Select _tokeType
			Case TOKE_IDENT
				expr=New IdentExpr( ParseIdent() )
			Case TOKE_INTLIT
				Local t$=_toke
				If _toke.StartsWith( "$" )
					Local val=0
					t=t.ToUpper()
					For Local i=1 Until t.Length
						Local ch=_toke[i]
						If ch>=48 And ch<58
							val=val Shl 4 | (ch & 15)
						Else
							val=val Shl 4 | ((ch & 15)+9)
						EndIf
					Next
					t=String( val )
				EndIf
				expr=New ConstExpr( Type.intType,t )
				NextToke
			Case TOKE_FLOATLIT
				expr=New ConstExpr( Type.floatType,_toke )
				NextToke
			Case TOKE_STRINGLIT
				expr=New ConstExpr( Type.stringType,BmxUnquote( _toke ) )
				NextToke
			Default
				SyntaxErr
			End Select
		End Select

		Repeat
			Select _toke
			Case "."
				NextToke
				expr=New IdentExpr( ParseIdent(),expr )
			Case "(","()"
				expr=New FuncCallExpr( expr,ParseArgs( stmt ) )
			Case "["
				NextToke
				If CParse( ".." )
					If _toke="]"
						expr=New SliceExpr( expr,Null,Null )
					Else
						expr=New SliceExpr( expr,Null,ParseExpr() )
					EndIf
				Else
					Local from:Expr=ParseExpr()
					If CParse( ".." )
						If _toke="]"
							expr=New SliceExpr( expr,from,Null )
						Else
							expr=New SliceExpr( expr,from,ParseExpr() )
						EndIf
					Else
						expr=New IndexExpr( expr,from )
					EndIf
				EndIf
				Parse "]"
			Default
				Return expr
			End Select
		Forever
		
	End
	
	Method ParseUnaryExpr:Expr()

		SkipEols
	
		Local op$=_toke
		Select op
		Case "+","-","~~","not"
			NextToke
			Local expr:Expr=ParseUnaryExpr()
			Return New UnaryExpr( op,expr )
		End Select
		Return ParsePrimaryExpr( False )
	End
	
	Method ParseMulDivExpr:Expr()
		Local expr:Expr=ParseUnaryExpr()
		Repeat
			Local op$=_toke
			Select op
			Case "*","/","mod","shl","shr"
				NextToke
				Local rhs:Expr=ParseUnaryExpr()
				expr=New BinaryMathExpr( op,expr,rhs )
			Default
				Return expr
			End Select
		Forever
	End
	
	Method ParseAddSubExpr:Expr()
		Local expr:Expr=ParseMulDivExpr()
		Repeat
			Local op$=_toke
			Select op
			Case "+","-"
				NextToke
				Local rhs:Expr=ParseMulDivExpr()
				expr=New BinaryMathExpr( op,expr,rhs )
			Default
				Return expr
			End Select
		Forever
	End
	
	Method ParseBitandExpr:Expr()
		Local expr:Expr=ParseAddSubExpr()
		Repeat
			Local op$=_toke
			Select op
			Case "&","~~"
				NextToke
				Local rhs:Expr=ParseAddSubExpr()
				expr=New BinaryMathExpr( op,expr,rhs )
			Default
				Return expr
			End Select
		Forever
	End
	
	Method ParseBitorExpr:Expr()
		Local expr:Expr=ParseBitandExpr()
		Repeat
			Local op$=_toke
			Select op
			Case "|"
				NextToke
				Local rhs:Expr=ParseBitandExpr()
				expr=New BinaryMathExpr( op,expr,rhs )
			Default
				Return expr
			End Select
		Forever
	End
	
	Method ParseCompareExpr:Expr()
		Local expr:Expr=ParseBitorExpr()
		Repeat
			Local op$=_toke
			Select op
			Case "=","<",">","<=",">=","<>"
				NextToke
				If op=">" And (_toke="=")
					op+=_toke
					NextToke
				Else If op="<" And (_toke="=" Or _toke=">")
					op+=_toke
					NextToke
				EndIf
				Local rhs:Expr=ParseBitorExpr()
				expr=New BinaryCompareExpr( op,expr,rhs )
			Default
				Return expr
			End Select
		Forever
	End
	
	Method ParseAndExpr:Expr()
		Local expr:Expr=ParseCompareExpr()
		Repeat
			Local op$=_toke
			If op="and"
				NextToke
				Local rhs:Expr=ParseCompareExpr()
				expr=New BinaryLogicExpr( op,expr,rhs )
			Else
				Return expr
			EndIf
		Forever
	End
	
	Method ParseOrExpr:Expr()
		Local expr:Expr=ParseAndExpr()
		Repeat
			Local op$=_toke
			If op="or"
				NextToke
				Local rhs:Expr=ParseAndExpr()
				expr=New BinaryLogicExpr( op,expr,rhs )
			Else
				Return expr
			EndIf
		Forever
	End
	
	Method ParseExpr:Expr()

		Return ParseOrExpr()
	End
	
	Method ParseIfStmt( term$ )

		CParse "if"

		Local expr:Expr=ParseExpr()
		
		CParse "then"
		
		Local thenBlock:BlockDecl=New BlockDecl( _block )
		Local elseBlock:BlockDecl=New BlockDecl( _block )
		
		Local eatTerm
		If Not term
			If _toke="~n" term="end" Else term="~n"
			eatTerm=True
		EndIf

		PushBlock thenBlock
		While _toke<>term
			Select _toke
			Case "endif"
				If term="end" Exit
				SyntaxErr
			Case "else","elseif"
				Local elif=_toke="elseif"
				NextToke
				If _block=elseBlock
					SyntaxErr
				EndIf
				PopBlock
				PushBlock elseBlock
				If elif Or _toke="if"
					ParseIfStmt term
				EndIf
			Default
				ParseStmt
			End
		Wend
		PopBlock

		If eatTerm
			If _toke="end"
				NextToke
				CParse "if"
			Else
				NextToke
			Endif
		Endif
		
		Local stmt:IfStmt=New IfStmt( expr,thenBlock,elseBlock )
		
		_block.AddStmt stmt
	End
	
	Method ParseWhileStmt()
	
		Parse "while"
		
		Local expr:Expr=ParseExpr()
		Local block:BlockDecl=New BlockDecl( _block )
		
		PushBlock block
		While Not CParse( "wend" )
			If CParse( "end" )
				CParse "while"
				Exit
			Endif
			ParseStmt
		Wend
		PopBlock
		
		Local stmt:WhileStmt=New WhileStmt( expr,block )
		
		_block.AddStmt stmt
	End
	
	Method ParseRepeatStmt()

		Parse "repeat"
		
		Local block:BlockDecl=New BlockDecl( _block )
		
		PushBlock block
		While _toke<>"until" And _toke<>"forever"
			ParseStmt
		Wend
		PopBlock
		
		SetErr
		
		Local expr:Expr
		If CParse( "until" )
			expr=ParseExpr()
		Else
			Parse "forever"
			expr=New ConstExpr( Type.boolType,"" )
		EndIf
		
		Local stmt:RepeatStmt=New RepeatStmt( block,expr )
		
		_block.AddStmt stmt
	End
	
	Method ParseForStmt()
	
		Parse "for"
		
		Local varid$,varty:Type,varlocal
		
		If CParse( "local" )
			varlocal=True
			varid=ParseIdent()
			If Not CParse( ":=" )
				varty=ParseDeclType()
				Parse( "=" )
			Endif
		Else
			varlocal=False
			varid=ParseIdent()
			Parse "="
		Endif
		
		If CParse( "eachin" )
			Local expr:Expr=ParseExpr()
			Local block:BlockDecl=New BlockDecl( _block )
			
			PushBlock block
			While Not CParse( "next" )
				If CParse( "end" )
					CParse "for"
					Exit
				Endif
				ParseStmt
			Wend
			PopBlock
			
			Local stmt:ForEachinStmt=New ForEachinStmt( varid,varty,varlocal,expr,block )
			
			_block.AddStmt stmt
			Return
		EndIf
		
		Local from:Expr=ParseExpr()
		
		Local op$
		If CParse( "to" )
			op="<="
		Else If CParse( "until" )
			op="<"
		Else
			Err "Expecting 'To' or 'Until'."
		EndIf
		
		Local term:Expr=ParseExpr()
		
		Local stp:Expr
		
		If CParse( "step" )
			stp=ParseExpr()
		Else
			stp=New ConstExpr( Type.intType,"1" )
		EndIf
		
		Local init:Stmt,expr:Expr,incr:Stmt
		
		If varlocal
			Local indexVar:LocalDecl=New LocalDecl( varid,varty,from,0 )
			init=New DeclStmt( indexVar )
			expr=New BinaryCompareExpr( op,New VarExpr( indexVar ),term )
			incr=New AssignStmt( "=",New VarExpr( indexVar ),New BinaryMathExpr( "+",New VarExpr( indexVar ),stp ) )
		Else
			init=New AssignStmt( "=",New IdentExpr( varid ),from )
			expr=New BinaryCompareExpr( op,New IdentExpr( varid ),term )
			incr=New AssignStmt( "=",New IdentExpr( varid ),New BinaryMathExpr( "+",New IdentExpr( varid ),stp ) )
		Endif
		
		Local block:BlockDecl=New BlockDecl( _block )
		
		PushBlock block
		While Not CParse( "next" )
			If CParse( "end" )
				CParse "for"
				Exit
			Endif
			ParseStmt
		Wend
		PopBlock
		
		NextToke

		Local stmt:ForStmt=New ForStmt( init,expr,incr,block )
		
		_block.AddStmt stmt
	End
	
	Method ParseReturnStmt()
		Parse "return"
		Local expr:Expr
		If Not AtEos() expr=ParseExpr()
		_block.AddStmt New ReturnStmt( expr )
	End
	
	Method ParseExitStmt()
		Parse "exit"
		_block.AddStmt New BreakStmt
	End
	
	Method ParseContinueStmt()
		Parse "continue"
		_block.AddStmt New ContinueStmt
	End
	
	Method ParseSelectStmt()
		Parse "select"
		
		Local block:BlockDecl=_block

		Local tmpVar:LocalDecl=New LocalDecl( "",Null,ParseExpr() )

		block.AddStmt New DeclStmt( tmpVar )
		
		While _toke<>"end" And _toke<>"default"
			SetErr
			Select _toke
			Case "~n"
				NextToke
			Case "case"
				NextToke
				Local comp:Expr
				Repeat
					Local expr:Expr=New VarExpr( tmpVar )
					expr=New BinaryCompareExpr( "=",expr,ParseExpr() )
					If comp
						comp=New BinaryLogicExpr( "or",comp,expr )
					Else
						comp=expr
					EndIf
				Until Not CParse(",")
				
				Local thenBlock:BlockDecl=New BlockDecl( _block )
				Local elseBlock:BlockDecl=New BlockDecl( _block )
				
				Local ifstmt:IfStmt=New IfStmt( comp,thenBlock,elseBlock )
				block.AddStmt ifstmt
				block=ifstmt.thenBlock
				
				PushBlock block
				While _toke<>"case" And _toke<>"default" And _toke<>"end"
					ParseStmt
				Wend
				PopBlock
				
				block=elseBlock
			Default
				SyntaxErr
			End Select
		Wend
		
		If _toke="default"
			NextToke
			PushBlock block
			While _toke<>"end"
				SetErr
				Select _toke
				Case "case"
					Err "Case can not appear after default."
				Case "default"
					Err "Select statement can have only one default block."
				End Select
				ParseStmt
			Wend
			PopBlock
		EndIf
		
		SetErr
		Parse "end"
		CParse "select"
	End
	
	Method ParseStmt()
		SetErr
		Select _toke
		Case "const","local"
			ParseDeclStmts
		Case "return"
			ParseReturnStmt()
		Case "exit"
			ParseExitStmt()
		Case "continue"
			ParseContinueStmt()
		Case "if"
			ParseIfStmt( "" )
		Case "while"
			ParseWhileStmt()
		Case "repeat"
			ParseRepeatStmt()
		Case "for"
			ParseForStmt()
		Case "select"
			ParseSelectStmt()
		Case "~n"
			NextToke
		Default
		
			Local expr:Expr=ParsePrimaryExpr( True )
			
			Select _toke
			Case "=","*=","/=","+=","-=","&=","|=","~=","mod","shl","shr"
				If IdentExpr( expr ) Or IndexExpr( expr )
					Local op$=_toke
					NextToke
					If Not op.EndsWith( "=" )
						Parse "="
						op+="="
					Endif
					_block.AddStmt New AssignStmt( op,expr,ParseExpr() )
				Else
					SyntaxErr
				Endif
				Return
			End
			
			If IdentExpr( expr ) 
				expr=New FuncCallExpr( expr,ParseStmtArgs() )
			Else If FuncCallExpr( expr) Or InvokeSuperExpr( expr ) Or NewObjectExpr( expr )
			Else
				SyntaxErr
			Endif
			_block.AddStmt New ExprStmt( expr )

		End Select
	End
	
	Method ParseDecl:Decl( toke$,attrs )
		SetErr
		Local id$=ParseIdent()
		Local ty:Type
		Local init:Expr
		If attrs & DECL_EXTERN
			ty=ParseDeclType()
		Else If CParse( ":=" )
			init=ParseExpr()
		Else
			ty=ParseDeclType()
			If CParse( "=" )
				init=ParseExpr()
			Else If CParse( "[" )
				Local len:=ParseExpr()
				Parse "]"
				While CParse( "[]" )
					ty=New ArrayType(ty)
				Wend
				init=New NewArrayExpr( ty,len )
				ty=New ArrayType( ty )
			Else If toke<>"const"
				init=New ConstExpr( ty,"" )
			Else
				Err "Constants must be initialized."
			EndIf
		EndIf
		
		Local decl:ValDecl
		
		Select toke
		Case "global" decl=New GlobalDecl( id,ty,init,attrs )
		Case "field"  decl=New FieldDecl( id,ty,init,attrs )
		Case "const"  decl=New ConstDecl( id,ty,init,attrs )
		Case "local"  decl=New LocalDecl( id,ty,init,attrs )
		End Select
		
		If decl.IsExtern() 
			If CParse( "=" )
				decl.munged=ParseStringLit()
			Else
				decl.munged=decl.ident
			EndIf
		EndIf
	
		Return decl
	End
	
	Method ParseDecls:List<Decl>( toke$,attrs )
		If toke Parse toke

		Local decls:=New List<Decl>
		Repeat
			Local decl:Decl=ParseDecl( toke,attrs )
			decls.AddLast decl
			If Not CParse(",") Return decls
		Forever
	End
	
	Method ParseDeclStmts()
		Local toke$=_toke
		NextToke
		Repeat
			Local decl:Decl=ParseDecl( toke,0 )
			_block.AddStmt New DeclStmt( decl )
		Until Not CParse(",")
	End
	
	Method ParseFuncDecl:FuncDecl( toke$,attrs )
		SetErr

		If toke Parse toke
	
		Local id$
		Local ty:Type
		
		If attrs & FUNC_METHOD
			If _toke="new"
				id=_toke
				NextToke
				ty=Null
				attrs|=FUNC_CTOR
				attrs&=~FUNC_METHOD
			Else
				id=ParseIdent()
				ty=ParseDeclType()
			EndIf
		Else
			id=ParseIdent()
			ty=ParseDeclType()
		EndIf
		
		Local args:ArgDecl[]
		If CParse( "()" )
		Else
			Parse "("
			SkipEols
			If _toke<>")"
				Local nargs
				Repeat
					Local id$=ParseIdent()
					Local ty:Type=ParseDeclType()
					Local init:Expr
					If CParse( "=" ) init=ParseExpr()
					Local arg:ArgDecl=New ArgDecl( id,ty,init )
					If args.Length=nargs args=args.Resize( nargs+10 )
					args[nargs]=arg
					nargs+=1
					If _toke=")" Exit
					Parse ","
				Forever
				args=args[..nargs]
			EndIf
			Parse ")"
		Endif

		Repeat		
			If CParse( "final" )
				attrs|=DECL_FINAL
			Else If CParse( "abstract" )
				attrs|=DECL_ABSTRACT
			Else If CParse( "property" )
				If attrs & FUNC_METHOD
					attrs|=FUNC_PROPERTY
				Else
					Err "Only methods can be properties."
				EndIf
			Else
				Exit
			EndIf
		Forever
		
		Local funcDecl:FuncDecl=New FuncDecl( id,ty,args,attrs )
		
		If funcDecl.IsExtern()
			funcDecl.munged=funcDecl.ident
			If CParse( "=" )
				funcDecl.munged=ParseStringLit()
				
				'Array $resize hack!
				If funcDecl.munged="$resize"
					funcDecl.retType=Type.emptyArrayType
				Endif
				
			Endif
			Return funcDecl
		EndIf
		
		If funcDecl.IsAbstract() Return funcDecl
		
		'Ok, only first statement of a constructor can call super constructor - not pretty, should be in semant.
		If attrs & FUNC_CTOR
			SkipEols
			If CParse( "super" )
				Parse "."
				If _toke="new"
					Local id$=_toke
					NextToke
					funcDecl.superCtor=New InvokeSuperExpr( id,ParseArgs( True ) )
					funcDecl.AddStmt New ExprStmt( funcDecl.superCtor )
				Else
					Local id$=ParseIdent()
					funcDecl.AddStmt New ExprStmt( New InvokeSuperExpr( id,ParseArgs( True ) ) )
				EndIf
			Else	'Invoke super default ctor
					'funcDecl.superCtor=New InvokeSuperExpr( "new",[] )
					'funcDecl.AddStmt New ExprStmt( funcDecl.superCtor )
			EndIf
		EndIf

		PushBlock funcDecl
		While _toke<>"end"
			ParseStmt
		Wend
		PopBlock

		NextToke
		If toke CParse toke
		
		Return funcDecl
	End
	
	Method ParseClassDecl:ClassDecl( toke$,attrs )
		SetErr

		If toke Parse toke
		
		Local id$=ParseIdent()
		Local superTy:IdentType
		Local args:ClassDecl[]
		If CParse( "<" )
			If attrs & DECL_EXTERN
				Err "Extern classes cannot be templates."
			EndIf
'			args=New ClassDecl[]
			Local nargs
			Repeat
				Local decl:ClassDecl=ParseClassDecl( "",CLASS_TEMPLATEARG )
				If args.Length=nargs args=args.Resize( nargs+10 )
				args[nargs]=decl
				nargs+=1
			Until Not CParse(",")
			args=args[..nargs]
			Parse ">"
		EndIf
		
		If CParse( "extends" )
			If CParse( "null" )
				If (attrs & DECL_EXTERN)
					superTy=Null
				Else
					Err "Only extern objects can extend null."
				EndIf
			Else
				superTy=ParseIdentType()
			EndIf
		Else
			superTy=New IdentType( "object",[] )
		EndIf
		
		Repeat
			If CParse( "final" )
				attrs|=DECL_FINAL
			Else If CParse( "abstract" )
				attrs|=DECL_ABSTRACT
			Else
				Exit
			EndIf
		Forever

		Local classDecl:ClassDecl=New ClassDecl( id,superTy,args,attrs )
		
		If classDecl.IsExtern() 
			classDecl.munged=classDecl.ident
			If CParse( "=" ) classDecl.munged=ParseStringLit()
		EndIf
		
		If classDecl.IsTemplateArg() Return classDecl

		Local decl_attrs=(attrs & DECL_EXTERN)
		
		While _toke<>"end"
			SetErr
			Select _toke
			Case "~n"
				NextToke
			Case "private"
				NextToke
				decl_attrs=decl_attrs | DECL_PRIVATE
			Case "public"
				NextToke
				decl_attrs=decl_attrs & ~DECL_PRIVATE
			Case "const","global","field"
				classDecl.InsertDecls ParseDecls( _toke,decl_attrs )
			Case "method"
				Local decl:FuncDecl=ParseFuncDecl( _toke,decl_attrs|FUNC_METHOD )
				If decl.IsCtor() decl.retType=New ObjectType( classDecl )
				classDecl.InsertDecl decl
			Case "function"
				Local decl:FuncDecl=ParseFuncDecl( _toke,decl_attrs )
				classDecl.InsertDecl decl
			Default
				SyntaxErr
			End Select
		Wend
		
		NextToke
		
		If toke CParse toke
		
		Return classDecl
	End
	
	Method ParseModPath$()
		Local path$=ParseIdent()
		While CParse( "." )
			path+="."+ParseIdent()
		Wend
		Return path
	End
	
	Method ExtractModIdent$( modpath$ )
		Local i=modpath.FindLast( "." )
		If i<>-1 Return modpath[i+1..]
		Return modpath
	End
	
	Method PreProcessPath$( path$ )
		path=path.Replace( "${HOST}",ENV_HOST )
		path=path.Replace( "${LANG}",ENV_LANG )
		path=path.Replace( "${TARGET}",ENV_TARGET )
		Return path
	End
	
	Method ImportFile( filepath$ )

		If ENV_SAFEMODE
			If _app.mainModule=_module
				Err "Import of external files not permitted in safe mode."
			Endif
		Endif

		filepath=RealPath( filepath )
		
		If FileType( filepath )<>FILETYPE_FILE
			Err "File '"+filepath+"' not found."
		EndIf
		
		_app.fileImports.AddLast filepath
		
	End
	
	Method ImportModule( modpath$,pub )
	
		Local filepath$
		
		Local cd$=CurrentDir
		ChangeDir ExtractDir( _toker.Path )
		
		For Local dir:=Eachin ENV_MODPATH.Split( ";" )
		
			filepath=RealPath( dir )+"/"+modpath.Replace( ".","/" )+"."+FILE_EXT			'/blah/etc.monkey
			Local filepath2$=StripExt( filepath )+"/"+StripDir( filepath )					'/blah/etc/etc.monkey
			
'			Print "filepath="+filepath
'			Print "filepath2="+filepath2
			
			If FileType( filepath )=FILETYPE_FILE
				If FileType( filepath2 )<>FILETYPE_FILE Exit
				Err "Duplicate module file: '"+filepath+"' and '"+filepath2+"'."
			Endif
			
			filepath=filepath2
			If FileType( filepath )=FILETYPE_FILE Exit
			
			filepath=""
		Next
		
		ChangeDir cd
		
		If Not filepath Err "Module '"+modpath+"' not found."
		
		#rem
		filepath=RealPath( _toker.Path() )+modfile

		If FileType( path )<>FILETYPE_FILE
		
			filepath=ENV_PROJPATH+modfile
			
			If FileType( filepath )<>FILETYPE_FILE
		
				filepath=ENV_MODPATH+modfile
			
				If FileType( filepath )<>FILETYPE_FILE
					Err "Module '"+modpath+"' not found."
				Endif
				
			Endif
			
		Endif
		#end

		'Note: filepath needs to be an *exact* match.
		'
		'Would be nice to have a version of realpath that fixed case and normalized separators for this.
		'
		'Currently, frontend is assumed to have done this with main src path, proj dir and mod dir.

		If _module.imported.Contains( filepath ) Return
		
		Local mdecl:=_app.imported.ValueForKey( filepath )

		If Not mdecl 
			mdecl=ParseModule( filepath,_app )
		Endif
		
		_module.imported.Insert mdecl.filepath,mdecl
	
		If pub _module.pubImported.Insert mdecl.filepath,mdecl

		_module.InsertAlias mdecl.ident,mdecl

	End
	
	Method ValidateModIdent( id$ )
		If id.Length
			If IsAlpha( id[0] ) Or id[0]="_"[0]
				Local err
				For Local i=1 Until id.Length
					If IsAlpha( id[i] ) Or IsDigit( id[i] ) Or id[i]="_"[0] Continue
					err=1
					Exit
				Next
				If Not err Return
			Endif
		Endif
		Err "Invalid module identifier '"+id+"'."
	End
	
	Method ParseMain()
	
		SkipEols

		Local mattrs
		If CParse( "strict" ) mattrs|=MODULE_STRICT
		
		Local path$=_toker.Path()
		Local ident$=StripAll( path )
		Local munged$="bb_"+ident+"_"

		ValidateModIdent ident
		
		_module=New ModuleDecl( ident,munged,path,mattrs )
		
		_module.imported.Insert path,_module

		_app.InsertModule _module
		
		ImportModule "monkey.lang",False
		ImportModule "monkey",False

		Local attrs
		
		'Parse header - imports etc.
		While _toke
			SetErr
			Select _toke
			Case "~n"
				NextToke
			Case "public"
				NextToke
				attrs=0
			Case "private"
				NextToke
				attrs=DECL_PRIVATE
			Case "import"
				NextToke
				If _tokeType=TOKE_STRINGLIT
					ImportFile PreProcessPath( ParseStringLit() )
				Else
					ImportModule ParseModPath(),(attrs & DECL_PRIVATE)=0
				EndIf
			Default
				Exit
			End Select
		Wend
		
		'Parse main app
		While _toke
			SetErr
			Select _toke
			Case "~n"
				NextToke
			Case "public"
				NextToke
				attrs=0
			Case "private"
				NextToke
				attrs=DECL_PRIVATE
			Case "extern"
				If ENV_SAFEMODE
					If _app.mainModule=_module
						Err "Extern not permitted in safe mode."
					Endif
				Endif
				NextToke
				attrs=DECL_EXTERN
				If CParse( "private" ) attrs=attrs|DECL_PRIVATE
			Case "const","global"
				_module.InsertDecls ParseDecls( _toke,attrs )
			Case "class"
				_module.InsertDecl ParseClassDecl( _toke,attrs )
			Case "function"
				_module.InsertDecl ParseFuncDecl( _toke,attrs )
			Default
				SyntaxErr
			End Select
		Wend
		
	End
	
	Method New( toker:Toker,app:AppDecl )
		_toke="~n"
		_toker=toker
		_app=app
		SetErr
		NextToke
	End
End

Function PreProcess$( path$ )

	Local ifnest,con,line,source:=New StringList
	
	Local toker:=New Toker( path,LoadString( path ) )

	toker.NextToke
	
	Repeat
		If line
			source.AddLast "~n"
			While toker.Toke And toker.Toke<>"~n" And toker.TokeType<>TOKE_LINECOMMENT
				toker.NextToke
			Wend
			If Not toker.Toke Exit
			toker.NextToke
		Endif
		line+=1
		
		_errInfo=toker.Path+"<"+toker.Line+">"
		
		If toker.TokeType=TOKE_SPACE toker.NextToke
		
		If toker.Toke="#"
			Select toker.NextToke.ToLower
			Case "if"
				If con=ifnest
					toker.NextToke
					Local src$
					While toker.Toke And toker.Toke<>"~n"
						src+=toker.Toke
						toker.NextToke
					Wend
					If Eval( src,Type.boolType )="1" con+=1
				Endif
				ifnest+=1
			Case "rem"
				ifnest+=1
			Case "else"
				If Not ifnest Err "#Else without #If"
				If con=ifnest
					con=-con
				Else If con=ifnest-1
					toker.NextToke
					If toker.TokeType=TOKE_SPACE toker.NextToke
					If toker.Toke.ToLower="if"
						toker.NextToke
						Local src$
						While toker.Toke And toker.Toke<>"~n"
							src+=toker.Toke
							toker.NextToke
						Wend
						If Eval( src,Type.boolType )="1" con+=1
					Else
						con+=1
					Endif
				Endif
			Case "end","endif"
				If Not ifnest Err "#End without #If"
				ifnest-=1
				If con<0 con=-con
				If ifnest<con con=ifnest
			Case "print"
				If con=ifnest
					toker.NextToke
					Local src$
					While toker.Toke And toker.Toke<>"~n"
						src+=toker.Toke
						toker.NextToke
					Wend
					Print Eval( src,Type.stringType )
				Endif
			Default
				Err "Unrecognized preprocessor directive '"+toker.Toke+"'."
			End Select
		Else If con=ifnest
			Local line$
			While toker.Toke And toker.Toke<>"~n" And toker.TokeType<>TOKE_LINECOMMENT
				Local toke$=toker.Toke
				If toker.TokeType=TOKE_KEYWORD toke=toke.ToLower
				line+=toke
				toker.NextToke
			Wend
			If line source.AddLast line
		Endif

	Forever
	
	Return source.Join( "" )
End

Function ParseModule:ModuleDecl( path$,app:AppDecl )

	Local source$=PreProcess( path )
	
	Local toker:Toker=New Toker( path,source )

	Local parser:Parser=New Parser( toker,app )
	
	parser.ParseMain
	
	Return parser._module
End

'***** PUBLIC API ******

Function ParseApp:AppDecl( path$ )

	Local app:AppDecl=New AppDecl
	
	Local source$=PreProcess( path )
	
	Local toker:Toker=New Toker( path,source )
	
	Local parser:Parser=New Parser( toker,app )
	
	parser.ParseMain
	
	Return app
End

Function Eval$( source$,ty:Type )

	Local env:=New ScopeDecl
	
	env.InsertDecl New ConstDecl( "HOST",Type.stringType,New ConstExpr( Type.stringType,ENV_HOST ),0 )
	env.InsertDecl New ConstDecl( "LANG",Type.stringType,New ConstExpr( Type.stringType,ENV_LANG ),0 )
	env.InsertDecl New ConstDecl( "TARGET",Type.stringType,New ConstExpr( Type.stringType,ENV_TARGET ),0 )
	env.InsertDecl New ConstDecl( "CONFIG",Type.stringType,New ConstExpr( Type.stringType,ENV_CONFIG ),0 )
	
	PushEnv env
	
	Local toker:=New Toker( "",source )
	
	Local parser:=New Parser( toker,Null )
	
	Local expr:=parser.ParseExpr()
	
	If ty expr=New CastExpr( ty,expr,CAST_EXPLICIT )
	
	expr=expr.Semant()
	
	Local r$=expr.Eval()
	
	PopEnv
	
	Return r
End

