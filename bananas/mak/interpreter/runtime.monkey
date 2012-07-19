
'Interpreter runtime
Import intrinsic

Function InternalErr()
	Error "InternalErr! "+_errinfo
	
End

'runtime env
Global _retval:Value
Global _globals:Value[]
Global _globalinits:ValueExpr[]
Global _locals:Value[],_local0
Global _null:=New ObjectValue()
Global _self:=_null
Global _errinfo$

'***** Value classes *****

Class Value

	Method ToBool:Bool()
		InternalErr
	End

	Method ToInt:Int()
		InternalErr
	End
	
	Method ToFloat:Float()
		InternalErr
	End

	Method ToString:String()
		InternalErr
	End
	
	Method ToArray:Value[]()
		InternalErr
	End
	
	Method ToObject:ObjectValue()
		InternalErr
	End
	
	Method Equals:Bool( val:Value )
		InternalErr
	End
	
	Method Compare:Int( val:Value )
		InternalErr
	End

End

Class BoolValue Extends Value
	Field value:Bool

	Method New( value:Bool )
		Self.value=value
	End
	
	Method ToBool:Bool()
		Return value
	End
	
	Method ToInt:Int()
		If value Return 1
		Return 0
	End
	
	Method ToFloat:Float()
		If value Return 1.0
		Return 0.0
	End

	Method ToString:String()
		If value Return "1"
		Return "0"
	End
	
	Method ToArray:Value[]()
		InternalErr
	End
	
	Method ToObject:ObjectValue()
		InternalErr
	End
	
	Method Equals:Bool( val:Value )
		Return value=val.ToBool()
	End
End
	
Class IntValue Extends Value
	Field value:Int
	
	Method New( value:Int )
		Self.value=value
	End
	
	Method ToBool:Bool()
		Return value<>0
	End
	
	Method ToInt:Int()
		Return value
	End
	
	Method ToFloat:Float()
		Return Float( value )
	End
	
	Method ToString:String()
		Return String( value )
	End
	
	Method ToArray:Value[]()
		InternalErr
	End
	
	Method ToObject:ObjectValue()
		InternalErr
	End
	
	Method Equals:Bool( val:Value )
		Return value=val.ToInt()
	End

	Method Compare:Int( val:Value )
		Return value-val.ToInt()
	End
	
End

Class FloatValue Extends Value
	Field value:Float
	
	Method New( value:Float )
		Self.value=value
	End
	
	Method ToBool:Bool()
		Return value<>0.0
	End
	
	Method ToInt:Int()
		Return Int( value )
	End

	Method ToFloat:Float()
		Return value
	End

	Method ToString:String()
		Return String( value )
	End

	Method ToArray:Value[]()
		InternalErr
	End
	
	Method ToObject:ObjectValue()
		InternalErr
	End
	
	Method Equals:Bool( val:Value )
		Return value=val.ToFloat()
	End
	
	Method Compare:Int( val:Value )
		Local t:=val.ToFloat()
		If value<t Return -1
		Return value>t
	End
End

Class StringValue Extends Value
	Field value:String
	
	Method New( value:String )
		Self.value=value
	End
	
	Method ToBool:Bool()
		Return value.Length<>0
	End
	
	Method ToInt:Int()
		Return Int( value )
	End
	
	Method ToFloat:Float()
		Return Float( value )
	End

	Method ToString:String()
		Return value
	End
	
	Method Equals:Bool( val:Value )
		Return value=val.ToString()
	End
	
	Method Compare:Int( val:Value )
		Return value.Compare( val.ToString() )
	End
	
End

Class ArrayValue Extends Value
	Field value:Value[]
	
	Method New( value:Value[] )
		Self.value=value
	End

	Method New( vals:Int[] )
		Local n:=vals.Length
		value=New Value[n]
		For Local i=0 Until n
			value[i]=New IntValue( vals[i] )
		Next
	End
	
	Method New( vals:Float[] )
		Local n:=vals.Length
		value=New Value[n]
		For Local i=0 Until n
			value[i]=New FloatValue( vals[i] )
		Next
	End
	
	Method New( vals:String[] )
		Local n:=vals.Length
		value=New Value[n]
		For Local i=0 Until n
			value[i]=New StringValue( vals[i] )
		Next
	End
	
	Method ToBool:Bool()
		Return value.Length<>0
	End
	
	Method ToInt:Int()
		InternalErr
	End
	
	Method ToFloat:Float()
		InternalErr
	End
	
	Method ToString:String()
		InternalErr
	End
	
	Method ToArray:Value[]()
		Return value
	End

	Method ToObject:ObjectValue()
		InternalErr
	End
	
	Function ToIntArray:Int[]( vals:Value[] )
		Local n:=vals.Length
		Local t:=New Int[n]
		For Local i=0 Until n
			t[i]=vals[i].ToInt()
		Next
		Return t
	End
	
	Function ToFloatArray:Float[]( vals:Value[] )
		Local n:=vals.Length
		Local t:=New Float[n]
		For Local i=0 Until n
			t[i]=vals[i].ToFloat()
		Next
		Return t
	End
	
	Function ToStringArray:String[]( vals:Value[] )
		Local n:=vals.Length
		Local t:=New String[n]
		For Local i=0 Until n
			t[i]=vals[i].ToString()
		Next
		Return t
	End
	
	Function ToValueArray:Value[]( vals:Int[] )
		Local n:=vals.Length
		Local t:=New Value[n]
		For Local i=0 Until n
			t[i]=New IntValue( vals[i] )
		Next
		Return t
	End
	
	Function ToValueArray:Value[]( vals:Float[] )
		Local n:=vals.Length
		Local t:=New Value[n]
		For Local i=0 Until n
			t[i]=New FloatValue( vals[i] )
		Next
		Return t
	End
	
	Function ToValueArray:Value[]( vals:String[] )
		Local n:=vals.Length
		Local t:=New Value[n]
		For Local i=0 Until n
			t[i]=New StringValue( vals[i] )
		Next
		Return t
	End
	
End

Class ObjectValue Extends Value
	Field clas:ClassDecl
	Field fields:Value[]
	
	Method New( clas:ClassDecl )
		Self.clas=clas
		fields=New Value[clas.fields.Length]
		For Local i=0 Until fields.Length
			fields[i]=clas.fields[i].Eval()
		Next
	End
	
	Method ToBool:Bool()
		Return True
	End
	
	Method ToInt:Int()
		InternalErr
	End
	
	Method ToFloat:Float()
		InternalErr
	End
	
	Method ToString:String()
		InternalErr
	End
	
	Method ToArray:Value[]()
		InternalErr
	End

	Method ToObject:ObjectValue()
		Return Self
	End

	Method Equals:Bool( val:Value )
		Return Self=val
	End

End

'***** Expr classes *****

Class Expr

	Method EvalBool:Bool()
		InternalErr
	End

	Method EvalInt:Int()
		InternalErr
	End
	
	Method EvalFloat:Float()
		InternalErr
	End

	Method EvalString:String()
		InternalErr
	End
	
	Method EvalArray:Value[]()
		InternalErr
	End
	
	Method EvalObject:ObjectValue()
		InternalErr
	End
End

Class ValueExpr Extends Expr

	Method Eval:Value()
		InternalErr
	End
	
	Method Eval:Value( inst:ObjectValue )
		InternalErr
	End
	
	Method SetValue:Void( value:Value )
		InternalErr
	End
	
	Method SetValue:Void( inst:ObjectValue,value:Value )
		InternalErr
	End

	Method EvalBool:Bool()
		Return Eval().ToBool()
	End
	
	Method EvalInt:Int()
		Return Eval().ToInt()
	End
	
	Method EvalFloat:Float()
		Return Eval().ToFloat()
	End

	Method EvalString:String()
		Return Eval().ToString()
	End
	
	Method EvalArray:Value[]()
		Return Eval().ToArray()
	End
	
	Method EvalObject:ObjectValue()
		Return Eval().ToObject()
	End
	
End

Class ConstValueExpr Extends ValueExpr
	Field value:Value
	
	Method New( value:Value )
		Self.value=value
	End
	
	Method Eval:Value()
		Return value
	End
	
	Method EvalBool:Bool()
		Return value.ToBool()
	End
	
	Method EvalInt:Int()
		Return value.ToInt()
	End
	
	Method EvalFloat:Float()
		Return value.ToFloat()
	End
	
	Method EvalString:String()
		Return value.ToString()
	End
	
	Method EvalArray:Value[]()
		Return value.ToArray()
	End
	
	Method EvalObject:ObjectValue()
		Return value.ToObject()
	End

End

Class BoolValueExpr Extends ValueExpr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method Eval:Value()
		Return New BoolValue( expr.EvalBool() )
	End
End

Class IntValueExpr Extends ValueExpr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method Eval:Value()
		Return New IntValue( expr.EvalInt() )
	End
End

Class FloatValueExpr Extends ValueExpr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method Eval:Value()
		Return New FloatValue( expr.EvalFloat() )
	End
End

Class StringValueExpr Extends ValueExpr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method Eval:Value()
		Return New StringValue( expr.EvalString() )
	End
End

Class ArrayValueExpr Extends ValueExpr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method Eval:Value()
		Return New ArrayValue( expr.EvalArray() )
	End
End

Class LocalVar Extends ValueExpr
	Field index
	
	Method New( index )
		Self.index=index
	End
	
	Method Eval:Value()
		Return _locals[_local0+index]
	End
	
	Method SetValue:Void( value:Value )
		_locals[_local0+index]=value
	End
End

Class GlobalVar Extends ValueExpr
	Field index
	
	Method New( index )
		Self.index=index
	End
	
	Method Eval:Value()
		Return _globals[index]
	End
	
	Method SetValue:Void( value:Value )
		_globals[index]=value
	End
End

Class FieldVar Extends ValueExpr
	Field index
	
	Method New( index )
		Self.index=index
	End
	
	Method Eval:Value()
		Return _self.fields[index]
	End
	
	Method Eval:Value( inst:ObjectValue )
		Return inst.fields[index]
	End
	
	Method SetValue:Void( value:Value )
		_self.fields[index]=value
	End
	
	Method SetValue:Void( inst:ObjectValue,value:Value )
		inst.fields[index]=value
	End
End

Class IndexExpr Extends ValueExpr
	Field arry:Expr
	Field index:Expr
	
	Method New( arry:Expr,index:Expr )
		Self.arry=arry
		Self.index=index
	End
	
	Method Eval:Value()
		Return arry.EvalArray()[ index.EvalInt() ]
	End
	
	Method SetValue:Void( value:Value )
		arry.EvalArray()[ index.EvalInt() ]=value
	End

End

Class IndexStringExpr Extends Expr
	Field str:Expr
	Field index:Expr
	
	Method New( str:Expr,index:Expr )
		Self.str=str
		Self.index=index
	End
	
	Method EvalInt:Int()
		Return str.EvalString()[ index.EvalInt() ]
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
	
	Method EvalArray:Value[]()
		Return expr.EvalArray()[from.EvalInt()..term.EvalInt()]
	End
End

Class SliceStringExpr Extends Expr
	Field expr:Expr
	Field from:Expr
	Field term:Expr
	
	Method New( expr:Expr,from:Expr,term:Expr )
		Self.expr=expr
		Self.from=from
		Self.term=term
	End
	
	Method EvalString:String()
		Return expr.EvalString()[from.EvalInt()..term.EvalInt()]
	End
End

Class NewObjectExpr Extends ValueExpr
	Field clas:ClassDecl
	Field ctor:FuncDecl
	Field args:ValueExpr[]
	Field vargs:Value[]
	
	Method New( clas:ClassDecl,ctor:FuncDecl,args:ValueExpr[] )
		Self.clas=clas
		Self.ctor=ctor
		Self.args=args
		Self.vargs=New Value[args.Length]
	End
	
	Method Eval:Value()
		For Local i=0 Until args.Length
			vargs[i]=args[i].Eval()
		Next
		Local inst:=New ObjectValue( clas )
		Local t:=_self
		_self=inst
		ctor.Exec( vargs )
		_self=t
		Return inst
	End

End

Class NewArrayExpr Extends Expr
	Field length:Expr
	Field init:Value

	Method New( length:Expr,init:Value )
		Self.length=length
		Self.init=init
	End
	
	Method EvalArray:Value[]()
		Local n:=length.EvalInt()
		Local t:=New Value[n]
		For Local i=0 Until n
			t[i]=init
		Next
		Return t
	End

End

Class ArrayExpr Extends Expr
	Field exprs:ValueExpr[]
	
	Method New( exprs:ValueExpr[] )
		Self.exprs=exprs
	End
	
	Method EvalArray:Value[]()
		Local n:=exprs.Length
		Local t:=New Value[n]
		For Local i=0 Until n
			t[i]=exprs[i].Eval()
		Next
		Return t
	End
End


Class MemberExpr Extends ValueExpr
	Field inst:ValueExpr
	Field member:ValueExpr
	
	Method New( inst:ValueExpr,member:ValueExpr )
		Self.inst=inst
		Self.member=member
	End
	
	Method Eval:Value()
		Return member.Eval( inst.EvalObject() )
	End
	
	Method SetValue:Void( value:Value )
		member.SetValue inst.EvalObject(),value
	End
End

Class InvokeFuncExpr Extends ValueExpr
	Field func:FuncDecl
	Field args:ValueExpr[]
	Field vargs:Value[]
	
	Method New( func:FuncDecl,args:ValueExpr[] )
		Self.func=func
		Self.args=args
		Self.vargs=New Value[args.Length]
	End
	
	Method Eval:Value()
		For Local i=0 Until args.Length
			vargs[i]=args[i].Eval()
		Next
		Return func.Exec( vargs )
	End
End

Class InvokeMethodExpr Extends ValueExpr
	Field index
	Field args:ValueExpr[]
	Field vargs:Value[]
	
	Method New( index,args:ValueExpr[] )
		Self.index=index
		Self.args=args
		Self.vargs=New Value[args.Length]
	End
	
	Method Eval:Value()
		For Local i=0 Until args.Length
			vargs[i]=args[i].Eval()
		Next
		Return _self.clas.methods[index].Exec( vargs )
	End
	
	Method Eval:Value( inst:ObjectValue )
		For Local i=0 Until args.Length
			vargs[i]=args[i].Eval()
		Next
		Local t:=_self
		_self=inst
		Local v:=_self.clas.methods[index].Exec( vargs )
		_self=t
		Return v
	End
	
End

Class UnaryExpr Extends Expr
	Field expr:Expr
End

Class NegExpr Extends UnaryExpr
	Method New( expr:Expr )
		Self.expr=expr
	End
	Method EvalInt:Int()
		Return -expr.EvalInt()
	End
	Method EvalFloat:Float()
		Return -expr.EvalFloat()
	End
End

Class NotExpr Extends UnaryExpr
	Method New( expr:Expr )
		Self.expr=expr
	End
	Method EvalBool:Bool()
		Return Not expr.EvalBool()
	End
End

Class BNotExpr Extends UnaryExpr
	Method New( expr:Expr )
		Self.expr=expr
	End
	Method EvalInt:Int()
		Return ~expr.EvalBool()
	End
End

Class BinaryExpr Extends Expr
	Field lhs:Expr
	Field rhs:Expr
End

Class MulExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt()*rhs.EvalInt()
	End
	Method EvalFloat:Float()
		Return lhs.EvalFloat()*rhs.EvalFloat()
	End
End

Class DivExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt()/rhs.EvalInt()
	End
	Method EvalFloat:Float()
		Return lhs.EvalFloat()/rhs.EvalFloat()
	End
End

Class ModExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt() Mod rhs.EvalInt()
	End
	Method EvalFloat:Float()
		Return lhs.EvalFloat() Mod rhs.EvalFloat()
	End
End

Class AddExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt()+rhs.EvalInt()
	End
	Method EvalFloat:Float()
		Return lhs.EvalFloat()+rhs.EvalFloat()
	End
	Method EvalString:String()
		Return lhs.EvalString()+rhs.EvalString()
	End
End

Class SubExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt()-rhs.EvalInt()
	End
	Method EvalFloat:Float()
		Return lhs.EvalFloat()-rhs.EvalFloat()
	End
End

Class ShlExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt() Shl rhs.EvalInt()
	End
End

Class ShrExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt() Shr rhs.EvalInt()
	End
End

Class AndExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalBool:Bool()
		Return lhs.EvalBool() And rhs.EvalBool()
	End
End

Class OrExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalBool:Bool()
		Return lhs.EvalBool() Or rhs.EvalBool()
	End
End

Class BAndExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt() & rhs.EvalInt()
	End

End

Class BOrExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt() | rhs.EvalInt()
	End

End

Class BXorExpr Extends BinaryExpr
	Method New( lhs:Expr,rhs:Expr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalInt:Int()
		Return lhs.EvalInt() ~ rhs.EvalInt()
	End

End

Class CompareExpr Extends Expr
	Field lhs:ValueExpr
	Field rhs:ValueExpr
End

Class EqExpr Extends CompareExpr
	Method New( lhs:ValueExpr,rhs:ValueExpr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalBool:Bool()
		Return lhs.Eval().Equals( rhs.Eval() )
	End
End

Class NeExpr Extends CompareExpr
	Method New( lhs:ValueExpr,rhs:ValueExpr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalBool:Bool()
		Return Not lhs.Eval().Equals( rhs.Eval() )
	End
End

Class LtExpr Extends CompareExpr
	Method New( lhs:ValueExpr,rhs:ValueExpr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalBool:Bool()
		Return lhs.Eval().Compare( rhs.Eval() )<0
	End
End

Class LeExpr Extends CompareExpr
	Method New( lhs:ValueExpr,rhs:ValueExpr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalBool:Bool()
		Return lhs.Eval().Compare( rhs.Eval() )<=0
	End
End

Class GtExpr Extends CompareExpr
	Method New( lhs:ValueExpr,rhs:ValueExpr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalBool:Bool()
		Return lhs.Eval().Compare( rhs.Eval() )>0
	End
End

Class GeExpr Extends CompareExpr
	Method New( lhs:ValueExpr,rhs:ValueExpr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	Method EvalBool:Bool()
		Return lhs.Eval().Compare( rhs.Eval() )>=0
	End
End

Class SelfExpr Extends ValueExpr
	Method Eval:Value()
		Return _self
	End
End

'***** Stmt classes *****

'EXEC return values
'
Const STMT_NEXT=0
Const STMT_CONTINUE=1
Const STMT_EXIT=2
Const STMT_RETURN=3

Class Stmt
	Field errinfo$
	
	Method New()
		errinfo=_errinfo
	End
	
	Method Exec:Int() Abstract
End

Class Block Extends Stmt
	Field stmts:=New Stack<Stmt>
	
	Method Exec:Int()
		For Local stmt:=Eachin stmts
			_errinfo=errinfo
			Local t:=stmt.Exec()
			If t Return t
		Next
		Return 0
	End
End

Class ExprStmt Extends Stmt
	Field expr:ValueExpr
	
	Method New( expr:ValueExpr )	
		Self.expr=expr
	End
	
	Method Exec:Int()
		expr.Eval()
		Return 0
	End
End

Class LocalDeclStmt Extends Stmt
	Field index
	Field init:ValueExpr
	
	Method New( index,init:ValueExpr )
		Self.index=index
		Self.init=init
	End

	Method Exec:Int()
		_locals[_local0+index]=init.Eval()
		Return 0
	End
End

Class AssignStmt Extends Stmt
	Field lhs:ValueExpr
	Field rhs:ValueExpr
	
	Method New( lhs:ValueExpr,rhs:ValueExpr )
		Self.lhs=lhs
		Self.rhs=rhs
	End
	
	Method Exec:Int()
		lhs.SetValue rhs.Eval()
		Return 0
	End
End

Class IfStmt Extends Stmt
	Field expr:Expr
	Field thenBlock:Block
	Field elseBlock:Block
	
	Method New( expr:Expr,thenBlock:Block,elseBlock:Block )
		Self.expr=expr
		Self.thenBlock=thenBlock
		Self.elseBlock=elseBlock
	End
	
	Method Exec:Int()
		If expr.EvalBool()
			Return thenBlock.Exec()
		Else If elseBlock
			Return elseBlock.Exec()
		Endif
	End
		
End

Class WhileStmt Extends Stmt
	Field expr:Expr
	Field block:Block
	
	Method New( expr:Expr,block:Block )
		Self.expr=expr
		Self.block=block
	End
	
	Method Exec:Int()
		While expr.EvalBool()
			Select block.Exec()
			Case STMT_EXIT Return 0
			Case STMT_RETURN Return STMT_RETURN
			End
		Wend
		Return 0
	End
End

Class RepeatStmt Extends Stmt
	Field block:Block
	Field expr:Expr
	
	Method New( block:Block,expr:Expr )
		Self.block=block
		Self.expr=expr
	End
	
	Method Exec:Int()
		Repeat
			Select block.Exec()
			Case STMT_EXIT Return 0
			Case STMT_RETURN Return STMT_RETURN
			End
		Until expr.EvalBool()
	End
End

Class ForStmt Extends Stmt
	Field init:Stmt
	Field expr:Expr
	Field incr:Stmt
	Field block:Block
	
	Method New( init:Stmt,expr:Expr,incr:Stmt,block:Block )
		Self.init=init
		Self.expr=expr
		Self.incr=incr
		Self.block=block
	End
	
	Method Exec:Int()
		init.Exec
		While expr.EvalBool()
			Select block.Exec()
			Case STMT_EXIT Return 0
			Case STMT_RETURN Return STMT_RETURN
			End
			incr.Exec
		Wend
	End
End

Class ContinueStmt Extends Stmt

	Method Exec:Int()
		Return STMT_CONTINUE
	End
End

Class ExitStmt Extends Stmt

	Method Exec:Int()
		Return STMT_EXIT
	End
End	

Class ReturnStmt Extends Stmt
	Field expr:ValueExpr
	
	Method New( expr:ValueExpr )
		Self.expr=expr
	End
	
	Method Exec:Int()
		If expr _retval=expr.Eval()
		Return STMT_RETURN
	End
End

'***** Decl classes *****

Class FuncDecl
	Field block:Block
	Field locals
	
	Method Exec:Value( args:Value[] )
		Local t:=_locals
		_locals=New Value[locals]
		For Local i=0 Until args.Length
			_locals[i]=args[i]
		Next
		block.Exec
		_locals=t
		Return _retval
	End
End

Class ClassDecl
	Field superclass:ClassDecl
	Field fields:ValueExpr[]
	Field methods:FuncDecl[]

End
