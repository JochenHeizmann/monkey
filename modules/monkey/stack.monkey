
' Module monkey.Stack
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Private

Import boxes

Public

Class Stack<T>

	Method Clear()
		length=0
	End

	Method Length()
		Return length
	End

	Method IsEmpty?()
		Return length=0
	End
	
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
		SetCapacity length+1
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
			If Compare( data[i],value )<>0
				i+=1
				Continue
			Endif
			Local b=i,e=i+1
			While e<length And Compare( data[e],value )=0
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
	End Method
	
	Method Compare( lhs:Object,rhs:Object )
		Error "Stack elements cannot be compared"
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

	Method Compare( lhs:Object,rhs:Object )
		Local l:=IntObject( lhs ).value
		Local r:=IntObject( rhs ).value
		If l<r Return -1
		Return l>r
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
	
	Method Compare( lhs:Object,rhs:Object )
		Local l:=FloatObject( lhs ).value
		Local r:=FloatObject( rhs ).value
		If l<r Return -1
		Return l>r
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
	
	Method Compare( lhs:Object,rhs:Object )
		Local l:=StringObject( lhs ).value
		Local r:=StringObject( rhs ).value
		If l<r Return -1
		Return l>r
	End
	
End
