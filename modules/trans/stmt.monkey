
' Module trans.stmt
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class Stmt
	Field errInfo$
	
	Method New()
		errInfo=_errInfo
	End
	
	Method Semant()
		PushErr errInfo
		OnSemant
		PopErr
	End
	
	Method OnSemant() Abstract

	Method Trans$() Abstract

End

Class DeclStmt Extends Stmt
	Field decl:Decl
	
	Method New( decl:Decl )
		Self.decl=decl
	End
	
	Method New( id$,ty:Type,init:Expr )
		Self.decl=New LocalDecl( id,ty,init,0 )	
	End
	
	Method OnSemant()
		decl.Semant
		_env.InsertDecl decl
	End
	
	Method Trans$()
		Return _trans.TransDeclStmt( Self )
	End
End

Class AssignStmt Extends Stmt
	Field op$
	Field lhs:Expr
	Field rhs:Expr
	
	Method New( op$,lhs:Expr,rhs:Expr )
		Self.op=op
		Self.lhs=lhs
		Self.rhs=rhs
	End
	
	Method OnSemant()
		rhs=rhs.Semant()
		lhs=lhs.SemantSet( op,rhs )
		If InvokeExpr( lhs ) Or InvokeMemberExpr( lhs )
			rhs=Null
		Else
			rhs=rhs.CastTo( lhs.exprType )
		EndIf
	End
	
	Method Trans$()
		_errInfo=errInfo
		Return _trans.TransAssignStmt( Self )
	End
End

Class ExprStmt Extends Stmt
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
		
	End
	
	Method OnSemant()
		expr=expr.Semant()
	End

	Method Trans$()
		Return _trans.TransExprStmt( Self )
	End
End

Class ReturnStmt Extends Stmt
	Field expr:Expr

	Method New( expr:Expr )
		Self.expr=expr
		
	End
	
	Method OnSemant()
		Local fdecl:FuncDecl=_env.FuncScope()
		If expr
			If fdecl.IsCtor() Err "Constructors may not return a value."
			If VoidType( fdecl.retType ) Err "Void functions may not return a value."
			expr=New CastExpr( fdecl.retType,expr ).Semant()
		Else If fdecl.IsCtor()
			expr=New SelfExpr().Semant()
		Else If Not VoidType( fdecl.retType )
			If _env.ModuleScope().IsStrict() Err "Missing return expression."
			expr=New ConstExpr( fdecl.retType,"" ).Semant()
		EndIf
	End
	
	Method Trans$()
		Return _trans.TransReturnStmt( Self )
	End
End

Class BreakStmt Extends Stmt

	Method OnSemant()
		If Not _loopnest Err "Break statement must appear inside a loop."
	End
	
	Method Trans$()
		Return _trans.TransBreakStmt( Self )
	End
	
End

Class ContinueStmt Extends Stmt

	Method OnSemant()
		If Not _loopnest Err "Continue statement must appear inside a loop."
	End
	
	Method Trans$()
		Return _trans.TransContinueStmt( Self )
	End
	
End

Class IfStmt Extends Stmt
	Field expr:Expr
	Field thenBlock:BlockDecl
	Field elseBlock:BlockDecl
	
	Method New( expr:Expr,thenBlock:BlockDecl,elseBlock:BlockDecl )
		Self.expr=expr
		Self.thenBlock=thenBlock
		Self.elseBlock=elseBlock
	End
	
	Method OnSemant()
		expr=expr.Semant().CastTo( Type.boolType,CAST_EXPLICIT )
		thenBlock.Semant
		elseBlock.Semant
	End
	
	Method Trans$()
		Return _trans.TransIfStmt( Self )
	End
End

Class WhileStmt Extends Stmt
	Field expr:Expr
	Field block:BlockDecl
	
	Method New( expr:Expr,block:BlockDecl )
		Self.expr=expr
		Self.block=block
	End
	
	Method OnSemant()
		expr=expr.Semant().CastTo( Type.boolType,CAST_EXPLICIT )
		_loopnest+=1
		block.Semant
		_loopnest-=1
	End
	
	Method Trans$()
		Return _trans.TransWhileStmt( Self )
	End
End

Class RepeatStmt Extends Stmt
	Field block:BlockDecl
	Field expr:Expr
	
	Method New( block:BlockDecl,expr:Expr )
		Self.block=block
		Self.expr=expr
		
	End
	
	Method OnSemant()
		_loopnest+=1
		block.Semant
		_loopnest-=1
		expr=expr.Semant().CastTo( Type.boolType,CAST_EXPLICIT )
	End
	
	Method Trans$()
		Return _trans.TransRepeatStmt( Self )
	End
End

Class ForStmt Extends Stmt
	Field init:Stmt	'assignment or local decl...
	Field expr:Expr
	Field incr:Stmt	'assignment...
	Field block:BlockDecl
	
	Method New( init:Stmt,expr:Expr,incr:Stmt,block:BlockDecl )
		Self.init=init
		Self.expr=expr
		Self.incr=incr
		Self.block=block
	End
	
	Method OnSemant()
		PushEnv block
		init.Semant
		PopEnv
	
		expr=expr.Semant()
		
		_loopnest+=1
		block.Semant
		_loopnest-=1

		incr.Semant
		
		'dodgy as hell! Reverse comparison for backward loops!
		Local assop:AssignStmt=AssignStmt( incr )
		Local addop:BinaryExpr=BinaryExpr( assop.rhs )
		Local stpval$=addop.rhs.Eval()
		If stpval.StartsWith( "-" )
			Local bexpr:BinaryExpr=BinaryExpr( expr )
			Select bexpr.op
			Case "<" bexpr.op=">"
			Case "<=" bexpr.op=">="
			End Select
		EndIf
		
	End
	
	Method Trans$()
		Return _trans.TransForStmt( Self )
	End
End
