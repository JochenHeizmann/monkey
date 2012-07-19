
' Module monkey.list
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Private

Import boxes

Public

Class List<T>

	Method Equals?( lhs:T,rhs:T )
		Return lhs=rhs
	End
	
	Method Compare( lhs:T,rhs:T )
		Error "Unable to compare items"
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

	'Deprecating, use RemoveEach
	Method Remove( value:T )
		RemoveEach value
	End
	
	Method RemoveEach( value:T )
		Local node:=_head._succ
		While node<>_head
			Local succ:=node._succ
			If Equals( node._data,value ) node.Remove
			node=succ
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
	End
	
	Method Backwards:BackwardsList<T>()
		Return New BackwardsList<T>( Self )
	End
	
	Method Sort( ascending=True )
		Local ccsgn=-1
		If ascending ccsgn=1
		Local insize=1
		
		Repeat
			Local merges
			Local tail:=_head
			Local p:=_head._succ

			While p<>_head
				merges+=1
				Local q:=p._succ,qsize=insize,psize=1
				
				While psize<insize And q<>_head
					psize+=1
					q=q._succ
				Wend

				Repeat
					Local t:Node<T>
					If psize And qsize And q<>_head
						Local cc=Compare( p._data,q._data ) * ccsgn
						If cc<=0
							t=p
							p=p._succ
							psize-=1
						Else
							t=q
							q=q._succ
							qsize-=1
						Endif
					Else If psize
						t=p
						p=p._succ
						psize-=1
					Else If qsize And q<>_head
						t=q
						q=q._succ
						qsize-=1
					Else
						Exit
					Endif
					t._pred=tail
					tail._succ=t
					tail=t
				Forever
				p=q
			Wend
			tail._succ=_head
			_head._pred=tail

			If merges<=1 Return

			insize*=2
		Forever

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

Class BackwardsList<T>

	Method New( list:List<T> )
		_list=list
	End

	Method ObjectEnumerator:BackwardsEnumerator<T>()
		Return New BackwardsEnumerator<T>( _list )
	End Method
	
Private

	Field _list:List<T>

End

Class BackwardsEnumerator<T>

	Method New( list:List<T> )
		_list=list
		_curr=list._head._pred
	End Method

	Method HasNext:Bool()
		Return _curr<>_list._head
	End 

	Method NextObject:T()
		Local data:T=_curr._data
		_curr=_curr._pred
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
	
	Method Equals?( lhs:IntObject,rhs:IntObject )
		Return lhs.value=rhs.value
	End
	
	Method Compare( lhs:IntObject,rhs:IntObject )
		Return lhs.value-rhs.value
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
	
	Method Equals?( lhs:FloatObject,rhs:FloatObject )
		Return lhs.value=rhs.value
	End
	
	Method Compare( lhs:FloatObject,rhs:FloatObject )
		If lhs.value<rhs.value Return -1
		Return lhs.value>rhs.value
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
	
	Method Equals?( lhs:StringObject,rhs:StringObject )
		Return lhs.value=rhs.value
	End

	Method Compare( lhs:StringObject,rhs:StringObject )
		Return lhs.value.Compare( rhs.value )
	End

End
