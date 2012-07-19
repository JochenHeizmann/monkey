
' Module trans.expr
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Class Expr
	Field exprType:Type
	
	Method ToString$()
		Return "expression"
	End
	
	Method Semant:Expr()
		Todo
	End
	
	Method SemantSet:Expr( op$,rhs:Expr )
		Err ToString()+" cannot be assigned to"
	End
	
	Method SemantFunc:Expr( args:Expr[] )
		Return Null
	End
	
	Method SemantScope:ScopeDecl()
		Return Null
	End

	Method Eval$()
		Err ToString()+" is not constant"
	End
	
	Method EvalConst:Expr()
		Return New ConstExpr( exprType,Eval() ).Semant()
	End
	
	Method Trans$()
		Todo
	End
	
	Method TransStmt$()
		Return Trans()
	End
	
	Method TransVar$()
		InternalErr
	End

	'semant and cast
	Method Semant:Expr( ty:Type,castFlags=0 )
		If exprType.EqualsType( ty ) Return Self
		Return New CastExpr( ty,Self,castFlags ).Semant()
	End

	'expr and ty already semanted!
	Method CastTo:Expr( ty:Type,castFlags=0 )
		If exprType.EqualsType( ty ) Return Self
		Return New CastExpr( ty,Self,castFlags ).Semant()
	End
	
	'actually the same as cast!
	Method SemantTo:Expr( ty:Type,castFlags=0 )
		If exprType.EqualsType( ty ) Return Self
		Return New CastExpr( ty,Self,castFlags ).Semant()
	End
	
	Method SemantArgs( args:Expr[] )
		For Local i=0 Until args.Length
			If args[i] args[i]=args[i].Semant()
		Next
	End
	
	Method BalanceTypes:Type( lhs:Type,rhs:Type )
		If StringType( lhs ) Or StringType( rhs ) Return Type.stringType
		If FloatType( lhs ) Or FloatType( rhs ) Return Type.floatType
		If IntType( lhs ) Or IntType( rhs ) Return Type.intType
		
		If lhs.ExtendsType( rhs ) Return rhs
		If rhs.ExtendsType( lhs ) Return lhs

		Return Null
	End
	
	Method CastArgs:Expr[]( args:Expr[],funcDecl:FuncDecl )
		'
		If args.Length>funcDecl.argDecls.Length InternalErr
		'
		args=args.Resize( funcDecl.argDecls.Length )
		'
		For Local i=0 Until args.Length
			If args[i]
				args[i]=args[i].CastTo( funcDecl.argDecls[i].ty )
			Else If funcDecl.argDecls[i].init
				args[i]=funcDecl.argDecls[i].init	
			Else
				Err "Missing function argument '"+funcDecl.argDecls[i].ident+"'."
			EndIf
		Next
		Return args
	End

End

'	exec a stmt, return an expr
Class StmtExpr Extends Expr
	Field stmt:Stmt
	Field expr:Expr
	
	Method New( stmt:Stmt,expr:Expr )
		Self.stmt=stmt
		Self.expr=expr
	End
	
	Method Semant:Expr()
		If exprType Return Self

		stmt.Semant()
		expr=expr.Semant()
		exprType=expr.exprType
		Return Self
	End
	
	Method Trans$()
		Return _trans.TransStmtExpr( Self )
	End

End

'	literal
Class ConstExpr Extends Expr
	Field ty:Type
	Field value$
	
	Method New( ty:Type,value$ )
		Self.ty=ty
		Self.value=value
	End
	
	Method ToString$()
		Return value
	End
	
	Method Semant:Expr()
		If exprType Return Self

		ty=ty.Semant()
		exprType=ty
		Return Self
	End
	
	Method Eval$()
		Return value
	End
	
	Method EvalConst:Expr()
		Return Self
	End
	
	Method Trans$()
		Return _trans.TransConstExpr( Self )
	End

End

Class VarExpr Extends Expr
	Field decl:VarDecl
	
	Method New( decl:VarDecl )
		Self.decl=decl
	End
	
	Method Semant:Expr()
		If exprType Return Self
		
		exprType=decl.ty
		Return Self
	End
	
	Method SemantSet:Expr( op$,rhs:Expr )
		Semant
		Return Self
	End
	
	Method Trans$()
		If decl=decl.actual Return _trans.TransVarExpr( Self )
		Local expr:Expr=New VarExpr( VarDecl( decl.actual ) )
		expr=New CastExpr( exprType,expr,CAST_EXPLICIT|CAST_TEMPLATE )
		Return expr.Semant().Trans()
	End
	
	Method TransVar$()
		Return _trans.TransVarExpr( Self )
	End
	
End

Class MemberVarExpr Extends Expr
	Field expr:Expr
	Field decl:VarDecl
	
	Method New( expr:Expr,decl:VarDecl )
		Self.expr=expr
		Self.decl=decl
	End
	
	Method Semant:Expr()
		If exprType Return Self
	
		exprType=decl.ty
		Return Self
	End
	
	Method SemantSet:Expr( op$,rhs:Expr )
		Semant
		Return Self
	End
	
	Method Trans$()
		If decl=decl.actual Return _trans.TransMemberVarExpr( Self )
		Local expr:Expr=New MemberVarExpr( Self.expr,VarDecl( decl.actual ) )
		expr=New CastExpr( exprType,expr,CAST_EXPLICIT|CAST_TEMPLATE )
		Return expr.Semant().Trans()
	End
	
	Method TransVar$()
		Return _trans.TransMemberVarExpr( Self )
 	End
End

Class InvokeExpr Extends Expr
	Field decl:FuncDecl
	Field args:Expr[]
	
	Method New( decl:FuncDecl,args:Expr[] )
		Self.decl=decl
		Self.args=args
	End
	
	Method Semant:Expr()
		If exprType Return Self
		
		args=CastArgs( args,decl )
		
		exprType=decl.retType
		Return Self
	End
	
	Method Trans$()
		If decl=decl.actual Return _trans.TransInvokeExpr( Self )
		Local expr:Expr=New InvokeExpr( FuncDecl( decl.actual ),Self.args )
		expr=New CastExpr( exprType,expr,CAST_EXPLICIT|CAST_TEMPLATE )
		Return expr.Semant().Trans()
	End
	
	Method TransStmt$()
		Return _trans.TransInvokeExpr( Self )
	End
End

Class InvokeMemberExpr Extends Expr
	Field expr:Expr
	Field decl:FuncDecl
	Field args:Expr[]
	
	Method New( expr:Expr,decl:FuncDecl,args:Expr[] )
		Self.expr=expr
		Self.decl=decl
		Self.args=args
	End
	
	Method Semant:Expr()
		If exprType Return Self
		
		args=CastArgs( args,decl )
	
		exprType=decl.retType
		
		'Array $resize hack!
		If ArrayType( exprType ) And VoidType( ArrayType( exprType ).elemType )
			exprType=expr.exprType
		Endif
		
		Return Self
	End
	
	Method Trans$()
		If decl=decl.actual Return _trans.TransInvokeMemberExpr( Self )
		Local expr:Expr=New InvokeMemberExpr( Self.expr,FuncDecl( decl.actual ),Self.args )
		expr=New CastExpr( exprType,expr,CAST_EXPLICIT|CAST_TEMPLATE )
		Return expr.Semant().Trans()
	End
	
	Method TransStmt$()
		Return _trans.TransInvokeMemberExpr( Self )
	End
	
End

Class NewObjectExpr Extends Expr
	Field ty:Type
	Field args:Expr[]
	Field ctor:FuncDecl	
	Field classDecl:ClassDecl
	
	Method New( ty:Type,args:Expr[] )
		Self.ty=ty
		Self.args=args
	End
	
	Method Semant:Expr()
		If exprType Return Self
		
		ty=ty.Semant()
		SemantArgs args
		
		Local objTy:ObjectType=ObjectType( ty )
		If Not objTy
			Err "Expression is not a class."
		EndIf

		classDecl=objTy.classDecl
		
		If classDecl.IsTemplateArg()
			Err "Cannot create instance of template class."
		EndIf
		
		If classDecl.IsAbstract()
			Err "Cannot create instance of abstract class."
		Endif

		ctor=classDecl.FindFuncDecl( "new",args )
		If Not ctor
			Err "No suitable constructor found for class "+classDecl.ToString()+"."
		Endif
		
		classDecl.attrs|=CLASS_INSTANCED

		args=CastArgs( args,ctor )

		exprType=ty
		Return Self
	End
	
	Method Trans$()
		Return _trans.TransNewObjectExpr( Self )
	End
End

Class NewArrayExpr Extends Expr
	Field ty:ArrayType
	Field expr:Expr
	
	Method New( ty:Type,expr:Expr )
		Self.ty=New ArrayType( ty )
		Self.expr=expr
		
	End
	
	Method Semant:Expr()
		If exprType Return Self
	
		ty=ArrayType( ty.Semant() )
		expr=expr.Semant().CastTo( Type.intType )
		exprType=ty
		Return Self
	End
	
	Method Trans$()
		Return _trans.TransNewArrayExpr( Self )
	End

End

'	super.ident( args )
Class InvokeSuperExpr Extends Expr
	Field ident$
	Field args:Expr[]
	Field funcDecl:FuncDecl
	Field classScope:ClassDecl
	Field superClass:ClassDecl

	Method New( ident$,args:Expr[] )
		Self.ident=ident
		Self.args=args
		
	End
	
	Method Semant:Expr()
		If exprType Return Self
	
		If _env.FuncScope().IsStatic() Err "Illegal use of Super."
		
		classScope=_env.ClassScope()
		superClass=classScope.superClass

		If Not superClass Err "Class has no super class."

		SemantArgs args
		funcDecl=superClass.FindFuncDecl( ident,args )
		If Not funcDecl Err "Can't find superclass method '"+ident+"'."
		
		args=CastArgs( args,funcDecl )
		exprType=funcDecl.retType
		Return Self
	End
	
	Method Trans$()
		Return _trans.TransInvokeSuperExpr( Self )
	End

End

'	self
Class SelfExpr Extends Expr

	Method New()
		
	End
	
	Method Semant:Expr()
		If exprType Return Self
	
		If _env.FuncScope().IsStatic() Err "Illegal use of Self."

		exprType=New ObjectType( _env.ClassScope() )
		Return Self
	End
	
	Method Trans$()
		Return _trans.TransSelfExpr( Self )
	End

End

Const CAST_EXPLICIT=1	'otherwise implicit
Const CAST_TEMPLATE=2	'template style cast

Class CastExpr Extends Expr
	Field ty:Type
	Field expr:Expr
	Field flags
	
	Method New( ty:Type,expr:Expr,flags=0 )
		Self.ty=ty
		Self.expr=expr
		Self.flags=flags
		
	End
	
	Method Semant:Expr()
		If exprType Return Self
		
		ty=ty.Semant()
		expr=expr.Semant()
		
		Local src:Type=expr.exprType
		
		If (flags & CAST_TEMPLATE)
			ty=ty.Actual()
			src=src.Actual()
		EndIf
		
		'equal?
		If src.EqualsType( ty ) Return expr
		
		'upcast?
		If src.ExtendsType( ty )
		
			'cast from void[] to T[]
			'
			If ArrayType(src) And VoidType( ArrayType(src).elemType )
				Return New ConstExpr( ty,"" ).Semant()
			EndIf
		
			'Box/unbox?...
			If ObjectType( ty ) And Not ObjectType( src )
			
				'Box!
				expr=New NewObjectExpr( ty,[expr] ).Semant()
				
			Else If ObjectType( src ) And Not ObjectType( ty )
			
				'Unbox!
				Local op$
				If BoolType( ty )
					op="ToBool"
				Else If IntType( ty ) 
					op="ToInt"
				Else If FloatType( ty )
					op="ToFloat"
				Else If StringType( ty )
					op="ToString"
				Else
					InternalErr
				EndIf
				Local fdecl:FuncDecl=src.GetClass().FindFuncDecl( op,[] )
				expr=New InvokeMemberExpr( expr,fdecl,[] ).Semant()
			EndIf
			exprType=ty

		Else If BoolType( ty )

			If (flags & CAST_EXPLICIT) exprType=ty
		
		Else If ty.ExtendsType( src )
		
			If flags & CAST_EXPLICIT
				'if both objects
				If ObjectType( ty ) And ObjectType( src ) exprType=ty
				
				'or both NOT objects
				If Not ObjectType( ty ) And Not ObjectType( src ) exprType=ty

			EndIf
		EndIf
		
		If Not exprType
			Err "Cannot convert from type "+src.ToString()+" to type "+ty.ToString()+"."
		EndIf
		
		If ConstExpr( expr ) Return EvalConst()
		
		Return Self
	End
	
	Method Eval$()
		Local val$=expr.Eval()
		If Not val Return val
		If BoolType( exprType )
			If IntType( expr.exprType )
				If Int( val ) Return "1"
				Return ""
			Else If FloatType( expr.exprType )
				If Float( val ) Return "1"
				Return ""
			Else If StringType( expr.exprType )
				If val.Length Return "1"
				Return ""
			EndIf
		Else If IntType( exprType )
			If BoolType( expr.exprType )
				If val Return "1"
				Return "0"
			EndIf
			Return Int( val )
		Else If FloatType( exprType )
			Return Float( val )
		Else If StringType( exprType )
			Return String( val )
		EndIf
		Return Super.Eval()
	End
	
	Method Trans$()
		Return _trans.TransCastExpr( Self )
	End

End

'op = '+', '-', '~' 
Class UnaryExpr Extends Expr
	Field op$,expr:Expr
	
	Method New( op$,expr:Expr )
		Self.op=op
		Self.expr=expr
		
	End
	
	Method Semant:Expr()
		If exprType Return Self
		
		Select op
		Case "+","-"
			expr=expr.Semant()
			If Not NumericType( expr.exprType ) Err expr.ToString()+" must be numeric for use with unary operator '"+op+"'"
			exprType=expr.exprType
		Case "~~"
			expr=expr.Semant().CastTo( Type.intType )
			exprType=Type.intType
		Case "not"
			expr=expr.Semant().CastTo( Type.boolType,CAST_EXPLICIT )
			exprType=Type.boolType
		Default
			InternalErr
		End Select
		
		If ConstExpr( expr ) Return EvalConst()
		
		Return Self
	End
	
	Method Eval$()
		Select op
		Case "+"
			Return expr.Eval()
		Case "~~"
			Return ~Int( expr.Eval() )
		Case "-"
			If IntType( exprType ) Return -Int( expr.Eval() )
			If FloatType( exprType ) Return -Float( expr.Eval() )
			InternalErr
		Case "not"
			If expr.Eval() Return "" Else Return "1"
		End Select
		InternalErr
	End
	
	Method Trans$()
		Return _trans.TransUnaryExpr( Self )
	End

End

Class BinaryExpr Extends Expr
	Field op$
	Field lhs:Expr
	Field rhs:Expr
	
	Method Trans$()
		Return _trans.TransBinaryExpr( Self )
	End

End

' * / + / & ~ | ^ shl shr
Class BinaryMathExpr Extends BinaryExpr
	Field ty:Type

	Method New( op$,lhs:Expr,rhs:Expr )
		Self.op=op
		Self.lhs=lhs
		Self.rhs=rhs
	End
	
	Method Semant:Expr()
		If exprType Return Self
	
		lhs=lhs.Semant()
		rhs=rhs.Semant()
		
		Select op
		Case "&","~~","|","mod","shl","shr"
			exprType=Type.intType
		Default
			exprType=BalanceTypes( lhs.exprType,rhs.exprType )
			If StringType( exprType )
				If op<>"+" 
					Err "Illegal string operator."
				EndIf
			Else If Not NumericType( exprType )
				Err "Illegal expression type."
			EndIf
		End Select
		
		lhs=lhs.CastTo( exprType )
		rhs=rhs.CastTo( exprType )
		
		If ConstExpr( lhs ) And ConstExpr( rhs ) Return EvalConst()

		Return Self
	End
	
	Method Eval$()
		Local lhs$=Self.lhs.Eval()
		Local rhs$=Self.rhs.Eval()
		If IntType( exprType )
			Local x=Int(lhs),y=Int(rhs)
			Select op
			Case "*" Return x*y
			Case "/" Return x/y
			Case "mod" Return x Mod y
			Case "shl" Return x Shl y
			Case "shr" Return x Shr y
			Case "+" Return x + y
			Case "-" Return x - y
			Case "&" Return x & y
			Case "~~" Return x ~ y
			Case "|" Return x | y
			End
		Else If FloatType( exprType )
			Local x#=Float(lhs),y#=Float(rhs)
			Select op
			Case "*" Return x * y
			Case "/" Return x / y
			Case "+" Return x + y
			Case "-" Return x - y
			End
		Else If StringType( exprType )
			Select op
			Case "+" Return lhs+rhs
			End
		EndIf
		InternalErr
	End
	
End

'=,<>,<,<=,>,>=
Class BinaryCompareExpr Extends BinaryExpr
	Field ty:Type

	Method New( op$,lhs:Expr,rhs:Expr )
		Self.op=op
		Self.lhs=lhs
		Self.rhs=rhs
	End
	
	Method Semant:Expr()
		If exprType Return Self
	
		lhs=lhs.Semant()
		rhs=rhs.Semant()

		ty=BalanceTypes( lhs.exprType,rhs.exprType )
		If Not ty
			Err "Illegal expression type."
		EndIf

		lhs=lhs.CastTo( ty )
		rhs=rhs.CastTo( ty )

		exprType=Type.boolType
		
		If ConstExpr( lhs ) And ConstExpr( rhs ) Return EvalConst()
		
		Return Self
	End
	
	Method Eval$()
		Local r=-1
		If IntType( ty )
			Local lhs:=Int( Self.lhs.Eval() )
			Local rhs:=Int( Self.rhs.Eval() )
			Select op
			Case "="  r=(lhs= rhs)
			Case "<>" r=(lhs<>rhs)
			Case "<"  r=(lhs< rhs)
			Case "<=" r=(lhs<=rhs)
			Case ">"  r=(lhs> rhs)
			Case ">=" r=(lhs>=rhs)
			End Select
		Else If FloatType( ty )
			Local lhs:=Float( Self.lhs.Eval() )
			Local rhs:=Float( Self.rhs.Eval() )
			Select op
			Case "="  r=(lhs= rhs)
			Case "<>" r=(lhs<>rhs)
			Case "<"  r=(lhs< rhs)
			Case "<=" r=(lhs<=rhs)
			Case ">"  r=(lhs> rhs)
			Case ">=" r=(lhs>=rhs)
			End Select
		Else If StringType( ty )
			Local lhs:=String( Self.lhs.Eval() )
			Local rhs:=String( Self.rhs.Eval() )
			Select op
			Case "="  r=(lhs= rhs)
			Case "<>" r=(lhs<>rhs)
			Case "<"  r=(lhs< rhs)
			Case "<=" r=(lhs<=rhs)
			Case ">"  r=(lhs> rhs)
			Case ">=" r=(lhs>=rhs)
			End Select
		EndIf
		If r=1 Return "1"
		If r=0 Return ""
		InternalErr
	End
End

'and, or
Class BinaryLogicExpr Extends BinaryExpr

	Method New( op$,lhs:Expr,rhs:Expr )
		Self.op=op
		Self.lhs=lhs
		Self.rhs=rhs
	End
	
	Method Semant:Expr()
		If exprType Return Self
		
		lhs=lhs.Semant().CastTo( Type.boolType,CAST_EXPLICIT )
		rhs=rhs.Semant().CastTo( Type.boolType,CAST_EXPLICIT )
		
		exprType=Type.boolType
		
		If ConstExpr( lhs ) And ConstExpr( rhs ) Return EvalConst()
		
		Return Self
	End
	
	Method Eval$()
		Select op
		Case "and" If lhs.Eval() And rhs.Eval() Return "1" Else Return ""
		Case "or"  If lhs.Eval() Or rhs.Eval() Return "1" Else Return ""
		End Select
		InternalErr
	End
End

Class IndexExpr Extends Expr
	Field expr:Expr
	Field index:Expr
	
	Method New( expr:Expr,index:Expr )
		Self.expr=expr
		Self.index=index
		
	End
	
	Method Semant:Expr()
		If exprType Return Self
	
		expr=expr.Semant()
		index=index.Semant().CastTo( Type.intType )
		
		If StringType( expr.exprType )
			exprType=Type.intType
		Else If ArrayType( expr.exprType )
			exprType=ArrayType( expr.exprType ).elemType
		Else
			Err "Only strings and arrays may be indexed."
		EndIf

		Return Self
	End
	
	Method SemantSet:Expr( op$,rhs:Expr )
		Semant
		Return Self
	End

	Method Trans$()
		Return _trans.TransIndexExpr( Self )
	End
	
	Method TransVar$()
		Return _trans.TransIndexExpr( Self )
	End

End

Class SliceExpr Extends Expr
	Field expr:Expr
	Field from:Expr
	Field term:Expr
	
	Method New( expr:Expr,from:Expr,term:Expr )
		Self.expr=expr
		Self.from=from
		Self.term=term
		
	End
	
	Method Semant:Expr()
		If exprType Return Self
	
		expr=expr.Semant()
		If ArrayType( expr.exprType ) Or StringType( expr.exprType )
			If from from=from.Semant().CastTo( Type.intType )
			If term term=term.Semant().CastTo( Type.intType )
			exprType=expr.exprType
		Else
			Err "Slices can only be used on strings or arrays."
		EndIf
		
'		If ConstExpr( expr ) And ConstExpr( from ) And ConstExpr( term ) Return EvalConst()

		Return Self
	End
	
	Method Eval$()
		Local from=Int( Self.from.Eval() )
		Local term=Int( Self.term.Eval() )
		If StringType( expr.exprType )
			Return expr.Eval()[ from..term ]
		Else If ArrayType( expr.exprType )
			Todo
		EndIf
	End
	
	Method Trans$()
		Return _trans.TransSliceExpr( Self )
	End
End

Class ArrayExpr Extends Expr
	Field exprs:Expr[]
	
	Method New( exprs:Expr[] )
		Self.exprs=exprs
		
	End
	
	Method Semant:Expr()
		If exprType Return Self
		
		exprs[0]=exprs[0].Semant()
		Local ty:Type=exprs[0].exprType
		
		For Local i=1 Until exprs.Length
			exprs[i]=exprs[i].Semant()
			ty=BalanceTypes( ty,exprs[i].exprType )
		Next
		
		For Local i=0 Until exprs.Length
			exprs[i]=exprs[i].CastTo( ty )
		Next
		
		exprType=New ArrayType( ty )
		Return Self	
	End
	
	Method Trans$()
		Return _trans.TransArrayExpr( Self )
	End

End
