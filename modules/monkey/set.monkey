
' Module monkey.set
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Private

Import map
Import boxes

Public

Class Set<T>

	Method Clear()
		map.Clear
	End
	
	Method Count()
		Return map.Count()
	End
	
	Method IsEmpty?()
		Return map.IsEmpty()
	End

	Method Contains?( value:T )
		Return map.Contains( value )
	End
	
	Method Insert( value:T )
		map.Insert value,Null
	End
	
	Method Remove( value:T )
		map.Remove value
	End
	
	Method ObjectEnumerator:KeyEnumerator<T,Object>()
		Return map.Keys().ObjectEnumerator()
	End
	
	
	Method New( map:Map<T,Object> )
		Self.map=map
	End
	
Private
	Field map:Map<T,Object>

End

Class IntSet Extends Set<IntObject>
	Method New()
		Super.New( New IntMap<Object> )
	End
End

Class FloatSet Extends Set<FloatObject>
	Method New()
		Super.New( New FloatMap<Object> )
	End
End

Class StringSet Extends Set<StringObject>
	Method New()
		Super.New( New StringMap<Object> )
	End
End