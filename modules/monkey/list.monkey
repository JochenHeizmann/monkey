
' Module monkey.list
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Private

Import boxes

Public

Class List<T>

	Method Equals( lhs:Object,rhs:Object )
		Return lhs=rhs
	End
	
	Method Clear()
		_head=New Node<T>()
	End

	Method Count()
		Local n,node:=_head._succ
		While node<>_head
			node=node._succ
			n+=1
		Wend
		Return n
	End
	
	Method IsEmpty?()
		Return _head._succ=_head
	End
	
	Method First:T()
		Return _head._succ._data
	End

	Method Last:T()
		Return _head._pred._data
	End
	
	Method AddFirst:Node<T>( data:T )
		Return New Node<T>( _head._succ,_head,data )
	End

	Method AddLast:Node<T>( data:T )
		Return New Node<T>( _head,_head._pred,data )
	End

	'I think this should GO!
	Method Remove( value:T )
		RemoveEach value
	End
	
	Method RemoveEach( value:T )
		Local node:=_head._succ
		While node<>_head
			node=node._succ
			If Equals( node._pred._data,value ) node._pred.Remove
		Wend
	End

	Method RemoveFirst:T()
		Local data:T=_head._succ._data
		_head._succ.Remove
		Return data
	End

	Method RemoveLast:T()
		Local data:T=_head._pred._data
		_head._pred.Remove
		Return data
	End

	Method ObjectEnumerator:Enumerator<T>()
		Return New Enumerator<T>( Self )
	End Method

Private

	Field _head:=New Node<T>
	
End

Class Node<T>

	'create a _head node
	Method New()
		_succ=Self
		_pred=Self
	End

	'create a link node
	Method New( succ:Node,pred:Node,data:T )
		_succ=succ
		_pred=pred
		_succ._pred=Self
		_pred._succ=Self
		_data=data
	End
	
	Method Value:T()
		Return _data
	End

	Method Remove()
		_succ._pred=_pred
		_pred._succ=_succ
	End Method

Private
	
	Field _succ:Node
	Field _pred:Node
	Field _data:T

End

Class Enumerator<T>

	Method New( list:List<T> )
		_list=list
		_curr=list._head._succ
	End Method

	Method HasNext:Bool()
		Return _curr<>_list._head
	End 

	Method NextObject:T()
		Local data:T=_curr._data
		_curr=_curr._succ
		Return data
	End

Private
	
	Field _list:List<T>
	Field _curr:Node<T>

End

'***** Box object versions *****

Class IntList Extends List<IntObject>

	Method ToArray[]()
		Local arr[Count],i
		For Local t=Eachin Self
			arr[i]=t
			i+=1
		Next
		Return arr
	End
	
	Method Equals( lhs:Object,rhs:Object )
		Return IntObject(lhs).value=IntObject(rhs).value
	End

End

Class FloatList Extends List<FloatObject>

	Method ToArray#[]()
		Local arr#[Count],i
		For Local t#=Eachin Self
			arr[i]=t
			i+=1
		Next
		Return arr
	End
	
	Method Equals( lhs:Object,rhs:Object )
		Return FloatObject(lhs).value=FloatObject(rhs).value
	End
	
End

Class StringList Extends List<StringObject>

	Method ToArray$[]()
		Local arr$[Count],i
		For Local t$=Eachin Self
			arr[i]=t
			i+=1
		Next
		Return arr
	End
	
	Method Join$( separator$ )
		Local bits$[Count],i
		For Local t$=Eachin Self
			bits[i]=t
			i+=1
		Next
		Return separator.Join( bits )
	End
	
	Method Equals( lhs:Object,rhs:Object )
		Return StringObject(lhs).value=StringObject(rhs).value
	End

End
