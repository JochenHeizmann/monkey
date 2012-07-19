
Import runtime

Class PrintExpr Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method EvalInt:Int()
		Print expr.EvalString()
	End
End

Class ErrorExpr Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method Eval:Value()
		Error expr.EvalString()
	End
End


'***** String properties *****

Class StringLength Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method EvalInt:Int()
		Return expr.EvalString().Length
	End
End

'***** String methods *****

Class StringCompare Extends Expr
	Field expr:Expr
	Field arg0:Expr
	
	Method New( expr:Expr,arg0:Expr )
		Self.expr=expr
		Self.arg0=arg0
	End
	
	Method EvalInit:Int()
		Return expr.EvalString().Compare( arg0.EvalString() )
	End
End

Class StringContains Extends Expr
	Field expr:Expr
	Field arg0:Expr
	
	Method New( expr:Expr,arg0:Expr )
		Self.expr=expr
		Self.arg0=arg0
	End
	
	Method EvalBool:Bool()
		Return expr.EvalString().Contains( arg0.EvalString() )
	End
End

Class StringEndsWith Extends Expr
	Field expr:Expr
	Field arg0:Expr
	
	Method New( expr:Expr,arg0:Expr )
		Self.expr=expr
		Self.arg0=arg0
	End
	
	Method EvalBool:Bool()
		Return expr.EvalString().EndsWith( arg0.EvalString() )
	End
End

Class StringFind Extends Expr
	Field expr:Expr
	Field arg0:Expr
	Field arg1:Expr
	
	Method New( expr:Expr,arg0:Expr,arg1:Expr )
		Self.expr=expr
		Self.arg0=arg0
		Self.arg1=arg1
	End
	
	Method EvalInt:Int()
		Return expr.EvalString().Find( arg0.EvalString(),arg1.EvalInt() )
	End
End

Class StringFindLast Extends Expr
	Field expr:Expr
	Field arg0:Expr
	
	Method New( expr:Expr,arg0:Expr )
		Self.expr=expr
		Self.arg0=arg0
	End
	
	Method EvalInt:Int()
		Return expr.EvalString().FindLast( arg0.EvalString() )
	End
End

Class StringFindLast2 Extends Expr
	Field expr:Expr
	Field arg0:Expr
	Field arg1:Expr
	
	Method New( expr:Expr,arg0:Expr,arg1:Expr )
		Self.expr=expr
		Self.arg0=arg0
		Self.arg1=arg1
	End
	
	Method EvalInt:Int()
		Return expr.EvalString().FindLast( arg0.EvalString(),arg1.EvalInt() )
	End
End

Class StringJoin Extends Expr
	Field expr:Expr
	Field arg0:Expr

	Method New( expr:Expr,arg0:Expr )
		Self.expr=expr
		Self.arg0=arg0
	End
	
	Method EvalString:String()
		Return expr.EvalString().Join( ArrayValue.ToStringArray( arg0.EvalArray() ) )
	End
End

Class StringReplace Extends Expr
	Field expr:Expr
	Field arg0:Expr
	Field arg1:Expr
	
	Method New( expr:Expr,arg0:Expr,arg1:Expr )
		Self.expr=expr
		Self.arg0=arg0
		Self.arg1=arg1
	End
	
	Method EvalString:String()
		Return expr.EvalString().Replace( arg0.EvalString(),arg1.EvalString() )
	End
End

Class StringSplit Extends Expr
	Field expr:Expr
	Field arg0:Expr
	
	Method New( expr:Expr,arg0:Expr )
		Self.expr=expr
		Self.arg0=arg0
	End
	
	Method EvalArray:Value[]()
		Return ArrayValue.ToValueArray( expr.EvalString().Split( arg0.EvalString() ) )
	End
End

Class StringStartsWith Extends Expr
	Field expr:Expr
	Field arg0:Expr
	
	Method New( expr:Expr,arg0:Expr )
		Self.expr=expr
		Self.arg0=arg0
	End
	
	Method EvalBool:Bool()
		Return expr.EvalString().StartsWith( arg0.EvalString() )
	End
End

Class StringToLower Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method EvalString:String()
		Return expr.EvalString().ToLower()
	End
End

Class StringToUpper Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method EvalString:String()
		Return expr.EvalString().ToUpper()
	End
End

Class StringTrim Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method EvalString:String()
		Return expr.EvalString().Trim()
	End
End

'***** String functions *****

Class StringFromChar Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method EvalString:String()
		Return String.FromChar( expr.EvalInt() )
	End
End

Class StringFromChars Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End
	
	Method EvalString:String()
		Return String.FromChars( ArrayValue.ToIntArray( expr.EvalArray() ) )
	End
End

'***** Array properties *****

Class ArrayLength Extends Expr
	Field expr:Expr
	
	Method New( expr:Expr )
		Self.expr=expr
	End

	Method EvalInt:Int()
		Return expr.EvalArray().Length
	End
End

'***** Array methods *****

Class ArrayResize Extends Expr
	Field expr:Expr
	Field newlen:Expr
	Field init:Value
	
	Method New( expr:Expr,newlen:Expr,init:Value )
		Self.expr=expr
		Self.newlen=newlen
		Self.init=init
	End
	
	Method EvalArray:Value[]()
		Local t:=expr.EvalArray()
		Local l:=newlen.EvalInt()
		If l<=0 Return []
		Local n:=Min( l,t.Length )
		Local p:=New Value[l]
		For Local i=0 Until n
			p[i]=t[i]
		Next
		For Local i=n Until l
			p[i]=init
		Next
		Return p
	End
End

'***** Math functions *****

Class MathSin Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Sin( arg0.EvalFloat() )
	End
End

Class MathCos Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Cos( arg0.EvalFloat() )
	End
End

Class MathTan Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Tan( arg0.EvalFloat() )
	End
End

Class MathASin Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return ASin( arg0.EvalFloat() )
	End
End

Class MathACos Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return ACos( arg0.EvalFloat() )
	End
End

Class MathATan Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return ATan( arg0.EvalFloat() )
	End
End

Class MathATan2 Extends Expr
	Field arg0:Expr
	Field arg1:Expr
	
	Method New( arg0:Expr,arg1:Expr )
		Self.arg0=arg0
		Self.arg1=arg1
	End
	
	Method EvalFloat:Float()
		Return ATan2( arg0.EvalFloat(),arg1.EvalFloat() )
	End
End

Class MathSinr Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Sinr( arg0.EvalFloat() )
	End
End

Class MathCosr Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Cosr( arg0.EvalFloat() )
	End
End

Class MathTanr Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Tanr( arg0.EvalFloat() )
	End
End

Class MathASinr Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return ASinr( arg0.EvalFloat() )
	End
End

Class MathACosr Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return ACosr( arg0.EvalFloat() )
	End
End

Class MathATanr Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return ATanr( arg0.EvalFloat() )
	End
End

Class MathATan2r Extends Expr
	Field arg0:Expr
	Field arg1:Expr
	
	Method New( arg0:Expr,arg1:Expr )
		Self.arg0=arg0
		Self.arg1=arg1
	End
	
	Method EvalFloat:Float()
		Return ATan2r( arg0.EvalFloat(),arg1.EvalFloat() )
	End
End

Class MathSqrt Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Sqrt( arg0.EvalFloat() )
	End
End

Class MathFloor Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Floor( arg0.EvalFloat() )
	End
End

Class MathCeil Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Ceil( arg0.EvalFloat() )
	End
End

Class MathLog Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Log( arg0.EvalFloat() )
	End
End

Class MathExp Extends Expr
	Field arg0:Expr
	
	Method New( arg0:Expr )
		Self.arg0=arg0
	End
	
	Method EvalFloat:Float()
		Return Exp( arg0.EvalFloat() )
	End
End

Class MathPow Extends Expr
	Field arg0:Expr
	Field arg1:Expr
	
	Method New( arg0:Expr,arg1:Expr )
		Self.arg0=arg0
		Self.arg1=arg1
	End
	
	Method EvalFloat:Float()
		Return Pow( arg0.EvalFloat(),arg1.EvalFloat() )
	End
End


