
' Module trans.trans
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Global _trans:Translator

Class Translator

	Field indent$
	Field lines:=New StringList
	Field unreachable,broken
	Field funcMungs:=New StringMap<FuncDeclList>
	Field mungedScopes:=New StringMap<StringSet>
	
	Method BeginLocalScope()
		mungedScopes.Set "$",New StringSet
	End
	
	Method EndLocalScope()
		mungedScopes.Set "$",Null
	End
	
	Method MungMethodDecl( fdecl:FuncDecl )

		If fdecl.munged Return
		
		If fdecl.overrides
			MungMethodDecl fdecl.overrides
			fdecl.munged=fdecl.overrides.munged
			Return
		Endif
		
		Local funcs:=funcMungs.Get( fdecl.ident )
		If funcs
			For Local tdecl:=Eachin funcs
				If fdecl.argDecls.Length=tdecl.argDecls.Length
					Local match=True
					For Local i=0 Until fdecl.argDecls.Length
						Local ty:=fdecl.argDecls[i].type
						Local ty2:=tdecl.argDecls[i].type
						If ty.EqualsType( ty2 ) Continue
						match=False
						Exit
					Next
					If match
						fdecl.munged=tdecl.munged
						Return
					Endif
				Endif
			Next
		Else
			funcs=New FuncDeclList
			funcMungs.Set fdecl.ident,funcs
		Endif
		
		fdecl.munged="m_"+fdecl.ident
		If Not funcs.IsEmpty() fdecl.munged+=String(funcs.Count()+1)
		funcs.AddLast fdecl
	End
	
	Method MungDecl( decl:Decl )

		If decl.munged Return

		Local fdecl:FuncDecl=FuncDecl( decl )
		If fdecl And fdecl.IsMethod() Return MungMethodDecl( fdecl )
		
		Local mscope$,cscope$
		If decl.ClassScope() cscope=decl.ClassScope().munged
		If decl.ModuleScope() mscope=decl.ModuleScope().munged
		
		Local id:=decl.ident,munged$,scope$
		
		'id=id.Replace( "_","_1" )
		
		If LocalDecl( decl )
			scope="$"
			munged="t_"+id
		Else If FieldDecl( decl )
			scope=cscope
			munged="f_"+id
		Else If GlobalDecl( decl ) Or FuncDecl( decl )
			If cscope And ENV_LANG<>"js"
				scope=cscope
				munged="g_"+id
			Else If cscope
				munged=cscope+"_"+id
			Else
				munged=mscope+"_"+id
			Endif
		Else If ClassDecl( decl )
			munged=mscope+"_"+id
		Else If ModuleDecl( decl )
			munged="bb_"+id
		Else
			InternalErr
		Endif
		
		Local set:=mungedScopes.Get( scope )
		If set
			If set.Contains( munged )
				Local id=1
				Repeat
					id+=1
					Local t$=munged+String(id)
					If set.Contains(t) Continue
					munged=t
					Exit
				Forever
			Endif
		Else
			If scope="$" InternalErr
			set=New StringSet
			mungedScopes.Set scope,set
		Endif
		set.Insert munged
		decl.munged=munged
	End
	
	Method Bra$( str$ )
		If str.StartsWith( "(" ) And str.EndsWith( ")" )
			Local n=1
			For Local i=1 Until str.Length-1
				Select str[i..i+1]
				Case "("
					n+=1
				Case ")"
					n-=1
					If Not n Return "("+str+")"
				End
			Next
			If n=1 Return str
'			If str.FindLast("(")<str.Find(")") Return str
		Endif
		Return "("+str+")"
	End
	
	'Utility C/C++ style...
	Method Enquote$( str$ )
		Return LangEnquote( str )
	End

	Method TransUnaryOp$( op$ )
		Select op
		Case "+" Return "+"
		Case "-" Return "-"
		Case "~~" Return op
		Case "not" Return "!"
		End Select
		InternalErr
	End
	
	Method TransBinaryOp$( op$,rhs$ )
		Select op
		Case "+","-"
			If rhs.StartsWith( op ) Return op+" "
			Return op
		Case "*","/" Return op
		Case "shl" Return "<<"
		Case "shr" Return ">>"
		Case "mod" Return " % "
		Case "and" Return " && "
		Case "or" Return " || "
		Case "=" Return "=="
		Case "<>" Return "!="
		Case "<","<=",">",">=" Return op
		Case "&","|" Return op
		Case "~~" Return "^"
		End Select
		InternalErr
	End
	
	Method TransAssignOp$( op$ )
		Select op
		Case "~~=" Return "^="
		Case "mod=" Return "%="
		Case "shl=" Return "<<="
		Case "shr=" Return ">>="
		End
		Return op
	End
	
	Method ExprPri( expr:Expr )
		'
		'1=primary,
		'2=postfix
		'3=prefix
		'
		If NewObjectExpr( expr )
			Return 3
		Else If UnaryExpr( expr )
			Select UnaryExpr( expr ).op
			Case "+","-","~~","not" Return 3
			End Select
			InternalErr
		Else If BinaryExpr( expr )
			Select BinaryExpr( expr ).op
			Case "*","/","mod" Return 4
			Case "+","-" Return 5
			Case "shl","shr" Return 6
			Case "<","<=",">",">=" Return 7
			Case "=","<>" Return 8
			Case "&" Return 9
			Case "~~" Return 10
			Case "|" Return 11
			Case "and" Return 12
			Case "or" Return 13
			End
			InternalErr
		Endif
		Return 2
	End
	
	Method TransSubExpr$( expr:Expr,pri=2 )
		Local t_expr$=expr.Trans()
		If ExprPri( expr )>pri t_expr=Bra( t_expr )
		Return t_expr
	End
	
	Method TransExprNS$( expr:Expr )
		If Not expr.SideEffects() Return expr.Trans()
'		If VarExpr( expr ) Return expr.Trans()
'		If ConstExpr( expr ) Return expr.Trans()
		Return CreateLocal( expr )
	End

	Method CreateLocal$( expr:Expr )
		Local tmp:=New LocalDecl( "",0,expr.exprType,expr )
		MungDecl tmp
		Emit TransLocalDecl( tmp.munged,expr )+";"
		Return tmp.munged
	End
	
	'***** Utility *****
	
	Method TransValue$( ty:Type,value$ ) Abstract

	Method TransLocalDecl$( munged$,init:Expr ) Abstract
	
	Method EmitEnter( func$ )
	End
	
	Method EmitSetErr( errInfo$ )
	End
	
	Method EmitLeave()
	End
	
	'***** Declarations *****
	
	Method TransGlobal$( decl:GlobalDecl ) Abstract
	
	Method TransField$( decl:FieldDecl,lhs:Expr ) Abstract
	
	Method TransFunc$( decl:FuncDecl,args:Expr[],lhs:Expr ) Abstract
	
	Method TransSuperFunc$( decl:FuncDecl,args:Expr[] ) Abstract
	
	
	'***** Expressions *****
	
	Method TransConstExpr$( expr:ConstExpr ) Abstract
	
	Method TransNewObjectExpr$( expr:NewObjectExpr ) Abstract
	
	Method TransNewArrayExpr$( expr:NewArrayExpr ) Abstract
	
	Method TransSelfExpr$( expr:SelfExpr ) Abstract
	
	Method TransCastExpr$( expr:CastExpr ) Abstract
	
	Method TransUnaryExpr$( expr:UnaryExpr ) Abstract
	
	Method TransBinaryExpr$( expr:BinaryExpr ) Abstract
	
	Method TransIndexExpr$( expr:IndexExpr ) Abstract
	
	Method TransSliceExpr$( expr:SliceExpr ) Abstract
	
	Method TransArrayExpr$( expr:ArrayExpr ) Abstract
	
	Method TransIntrinsicExpr$( decl:Decl,expr:Expr,args:Expr[] ) Abstract
	
	'***** Simple statements *****
	
	'Expressions
	Method TransStmtExpr$( expr:StmtExpr )
		Local t$=expr.stmt.Trans()
		If t Emit t+";"
		Return expr.expr.Trans()
	End
	
'	Method TransTemplateCast$( ty:Type,src:Type,expr$ )
'		Return expr
'	End
	
	Method TransVarExpr$( expr:VarExpr )
		Local decl:=VarDecl( expr.decl )
		
		If decl.munged.StartsWith( "$" ) Return TransIntrinsicExpr( decl,Null,[] )
		
		If LocalDecl( decl ) Return decl.munged
		
		If FieldDecl( decl ) Return TransField( FieldDecl( decl ),Null )
		
		If GlobalDecl( decl ) Return TransGlobal( GlobalDecl( decl ) )
		
		InternalErr
	End
	
	Method TransMemberVarExpr$( expr:MemberVarExpr )
		Local decl:=VarDecl( expr.decl )
		
		If decl.munged.StartsWith( "$" ) Return TransIntrinsicExpr( decl,expr.expr,[] )
		
		If FieldDecl( decl ) Return TransField( FieldDecl( decl ),expr.expr )

		InternalErr
	End
	
	Method TransInvokeExpr$( expr:InvokeExpr )
		Local decl:=FuncDecl( expr.decl ),t$
		
		If decl.munged.StartsWith( "$" ) Return TransIntrinsicExpr( decl,Null,expr.args )
		
		If decl Return TransFunc( FuncDecl(decl),expr.args,Null )
		
		InternalErr
	End
	
	Method TransInvokeMemberExpr$( expr:InvokeMemberExpr )
		Local decl:=FuncDecl( expr.decl ),t$

		If decl.munged.StartsWith( "$" ) Return TransIntrinsicExpr( decl,expr.expr,expr.args )
		
		If decl Return TransFunc( FuncDecl(decl),expr.args,expr.expr )	
		
		InternalErr
	End
	
	Method TransInvokeSuperExpr$( expr:InvokeSuperExpr )
		Local decl:=FuncDecl( expr.funcDecl ),t$

		If decl.munged.StartsWith( "$" ) Return TransIntrinsicExpr( decl,expr,[] )
		
		If decl Return TransSuperFunc( FuncDecl( decl ),expr.args )
		
		InternalErr
	End
	
	Method TransExprStmt$( stmt:ExprStmt )
		Return stmt.expr.TransStmt()
	End
	
	Method TransAssignStmt$( stmt:AssignStmt )
		If Not stmt.rhs
			Return stmt.lhs.Trans()
		Endif
		
		If stmt.tmp1
			MungDecl stmt.tmp1
			Emit TransLocalDecl( stmt.tmp1.munged,stmt.tmp1.init )+";"
		Endif
		If stmt.tmp2
			MungDecl stmt.tmp2
			Emit TransLocalDecl( stmt.tmp2.munged,stmt.tmp2.init )+";"
		Endif
		
		Return TransAssignStmt2( stmt )
	End
	
	Method TransAssignStmt2$( stmt:AssignStmt )
		Return stmt.lhs.TransVar()+TransAssignOp( stmt.op )+stmt.rhs.Trans()
	End
	
	Method TransReturnStmt$( stmt:ReturnStmt )
		Local t$="return"
		If stmt.expr t+=" "+stmt.expr.Trans()
		unreachable=True
		Return t
	End
	
	Method TransContinueStmt$( stmt:ContinueStmt )
		unreachable=True
		Return "continue"
	End
	
	Method TransBreakStmt$( stmt:BreakStmt )
		unreachable=True
		broken+=1
		Return "break"
	End
	
	'***** Block statements - all very C like! *****
	
	Method Emit( t$ )
		If Not t Return
		If t.StartsWith( "}" )
			indent=indent[..indent.Length-1]
		Endif
		lines.AddLast indent+t
		'code+=indent+t+"~n"
		If t.EndsWith( "{" )
			indent+="~t"
		Endif
	End
	
	Method JoinLines$()
		Local code$=lines.Join( "~n" )
		lines.Clear
		Return code
	End
	
	'returns unreachable status!
	Method EmitBlock( block:BlockDecl )
	
		PushEnv block
		
		Local func:=FuncDecl( block )
		
		If func 
			If func.IsAbstract() InternalErr
			If ENV_CONFIG<>"release" EmitEnter func.scope.ident+"."+func.ident
		Endif

		For Local stmt:Stmt=Eachin block.stmts
		
			_errInfo=stmt.errInfo
			
			If unreachable	' And ENV_LANG<>"as"
				'If stmt.errInfo Print "Unreachable:"+stmt.errInfo
				Exit
			Endif

			If ENV_CONFIG<>"release"
				Local rs:=ReturnStmt( stmt )
				If rs
					If rs.expr
						EmitSetErr stmt.errInfo
						Local t_expr:=TransExprNS( rs.expr )'.Trans()
						EmitLeave
						Emit "return "+t_expr+";"
					Else
						EmitLeave
						Emit "return;"
					Endif
					unreachable=True
					Continue
				Endif
				EmitSetErr stmt.errInfo
			Endif
			
			Local t$=stmt.Trans()
			If t Emit t+";"
			
		Next
		
		Local unr=unreachable
		unreachable=False
		
		If func
			If unr
				'Actionscript's reachability analysis ain't so hot...tack on a return
				'just in case.
				If ENV_LANG="as" And Not VoidType( func.retType )
					If block.stmts.IsEmpty() Or Not ReturnStmt( block.stmts.Last() )
						Emit "return "+TransValue( func.retType,"" )+";"
					Endif
				Endif
			Else
				If ENV_CONFIG<>"release" EmitLeave
				
				If Not VoidType( func.retType )
					If func.IsCtor()
						Emit "return this;"
					Else
						If func.ModuleScope().IsStrict()
							_errInfo=func.errInfo
							Err "Missing return statement."
						Endif
						Emit "return "+TransValue( func.retType,"" )+";"
					Endif
				Endif
			Endif
		Endif
		
		PopEnv
		
		Return unr
	End
	
	Method TransDeclStmt$( stmt:DeclStmt )
		Local decl:=LocalDecl( stmt.decl )
		If decl
			MungDecl decl
			Return TransLocalDecl( decl.munged,decl.init )
		Endif
		Local cdecl:=ConstDecl( stmt.decl )
		If cdecl
			Return
		Endif
		InternalErr
	End
	
	Method TransIfStmt$( stmt:IfStmt )
		
		If ConstExpr( stmt.expr )
			If ConstExpr( stmt.expr ).value
				If EmitBlock( stmt.thenBlock ) unreachable=True
			Else If Not stmt.elseBlock.stmts.IsEmpty()
				If EmitBlock( stmt.elseBlock ) unreachable=True
			Endif
		Else If Not stmt.elseBlock.stmts.IsEmpty()
			Emit "if"+Bra( stmt.expr.Trans() )+"{"
			Local unr=EmitBlock( stmt.thenBlock )
			Emit "}else{"
			Local unr2=EmitBlock( stmt.elseBlock )
			Emit "}"
			If unr And unr2 unreachable=True
		Else
			Emit "if"+Bra( stmt.expr.Trans() )+"{"
			Local unr=EmitBlock( stmt.thenBlock )
			Emit "}"
		Endif
	End
	
	Method TransWhileStmt$( stmt:WhileStmt )
		Local nbroken=broken
		
		Emit "while"+Bra( stmt.expr.Trans() )+"{"
		Local unr=EmitBlock( stmt.block )
		Emit "}"
		
		If broken=nbroken And ConstExpr( stmt.expr ) And ConstExpr( stmt.expr ).value unreachable=True
		broken=nbroken
	End

	Method TransRepeatStmt$( stmt:RepeatStmt )
		Local nbroken=broken

		Emit "do{"
		Local unr=EmitBlock( stmt.block )
		Emit "}while(!"+Bra( stmt.expr.Trans() )+");"

		If broken=nbroken And ConstExpr( stmt.expr ) And Not ConstExpr( stmt.expr ).value unreachable=True
		broken=nbroken
	End

	Method TransForStmt$( stmt:ForStmt )
		Local nbroken=broken

		Local init$=stmt.init.Trans()
		Local expr$=stmt.expr.Trans()
		Local incr$=stmt.incr.Trans()

		Emit "for("+init+";"+expr+";"+incr+"){"
		Local unr=EmitBlock( stmt.block )
		Emit "}"
		
		If broken=nbroken And ConstExpr( stmt.expr ) And ConstExpr( stmt.expr ).value unreachable=True
		broken=nbroken
	End
	
	'module
	Method TransApp$( app:AppDecl ) Abstract

	Method PostProcess$( source$ ) 
		Return source
	End
	
End

