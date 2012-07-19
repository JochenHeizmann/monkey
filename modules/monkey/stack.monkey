
' Module monkey.Stack
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Private

Import boxes

Public

Class Stack<T>

	Method Equals?( lhs:T,rhs:T )
		Return lhs=rhs
	End
	
	Method Compare( lhs:T,rhs:T )
		Error "Unable to compare items"
	End
	
	Method Clear()
		length=0
	End

	Method Length()
		Return length
	End

	Method IsEmpty?()
		Return length=0
	End
	
	Method Contains?( value:T )
		For Local i=0 Until length
			If Equals( data[i],value ) Return True
		Next
	End Method
	
	Method Push( value:T )
		If length=data.Length
			data=data.Resize( length*2+10 )
		Endif
		data[length]=value
		length+=1
	End

	Method Pop:T()
		length-=1
		Return data[length]
	End
	
	Method Top:T()
		Return data[length-1]
	End

	Method Set( index,value:T )
		data[index]=value
	End

	Method Get:T( index )
		Return data[index]
	End

	Method Insert( index,value:T )
		If length=data.Length
			data=data.Resize( length*2+10 )
		Endif
		For Local i=length Until index Step -1
			data[i]=data[i-1]
		Next
		data[index]=value
		length+=1
	End

	Method Remove( index )
		For Local i=index Until length-1
			data[i]=data[i+1]
		Next
		length-=1
	End
	
	Method RemoveEach( value:T )
		Local i
		While i<length
			If Not Equals( data[i],value )
				i+=1
				Continue
			Endif
			Local b=i,e=i+1
			While e<length And Equals( data[e],value )
				e+=1
			Wend
			While e<length
				data[b]=data[e]
				b+=1
				e+=1
			Wend
			length-=e-b
			i+=1
		Wend
	End
	
	Method ObjectEnumerator:Enumerator<T>()
		Return New Enumerator<T>( Self )
	End
	
	Method Backwards:BackwardsStack<T>()
		Return New BackwardsStack<T>( Self )
	End
	
Private

	Field data:T[]
	Field length

End

Class Enumerator<T>

	Method New( stack:Stack<T> )
		Self.stack=stack
	End

	Method HasNext:Bool()
		Return index<stack.Length
	End

	Method NextObject:T()
		index+=1
		Return stack.Get( index-1 )
	End

Private

	Field stack:Stack<T>
	Field index

End

Class BackwardsStack<T>

	Method New( stack:Stack<T> )
		Self.stack=stack
	End

	Method ObjectEnumerator:BackwardsEnumerator<T>()
		Return New BackwardsEnumerator<T>( stack )
	End Method
	
Private

	Field stack:Stack<T>

End

Class BackwardsEnumerator<T>

	Method New( stack:Stack<T> )
		Self.stack=stack
		index=stack.length
	End Method

	Method HasNext:Bool()
		Return index>0
	End 

	Method NextObject:T()
		index-=1
		Return stack.Get( index )
	End

Private
	
	Field stack:Stack<T>
	Field index

End

'***** Box object versions *****

Class IntStack Extends Stack<IntObject>

	Method ToArray[]()
		Local arr[Length],i
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

Class FloatStack Extends Stack<FloatObject>

	Method ToArray#[]()
		Local arr#[Length],i
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

Class StringStack Extends Stack<StringObject>

	Method ToArray$[]()
		Local arr$[Length],i
		For Local t$=Eachin Self
			arr[i]=t
			i+=1
		Next
		Return arr
	End
	
	Method Join$( separator$ )
		Local bits$[Length],i
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
