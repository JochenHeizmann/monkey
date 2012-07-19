
' Module monkey.resource
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Private

Import map
Import list

Global resources:=New StringMap< List<Resource> >

Public

Class Resource

	Method Retain()
		refs+=1
	End
	
	Method Release()
		refs-=1
		If refs Return
		Destroy
	End
	
	Method Destroy()
		If Not node Return
		node.Remove
		node=Null
		OnDestroy
	End

	'Protected use only...
	Method Register( type$ )
		Local list:=resources.ValueForKey( type )
		If Not list
			list=New List<Resource>
			resources.Insert type,list
		Endif
		node=list.AddLast( Self )
	End
	
	Private
	
	Field refs=1
	Field node:list.Node<Resource>

	Method OnDestroy() Abstract

End

Function DestroyAll( type$ )
	Local list:=resources.ValueForKey( type )
	While Not list.IsEmpty()
		list.First.Destroy
	Wend
End

Function DestroyAll()
	For Local type:=Eachin resources.Keys
		DestroyAll type
	Next
End
