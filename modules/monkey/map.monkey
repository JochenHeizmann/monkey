
' Module monkey.map
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Private

Import boxes

Public

Class Map<K,V>

	Method Clear()
		root=Null
	End
	
	Method Count()
		If root Return root.Count( 0 )
	End

	Method IsEmpty?()
		Return root=Null
	End

	Method Contains?( key:K )
		Return FindNode( key )<>Null
	End

	Method Set( key:K,value:V )
		Local node:Node<K,V>=root
		Local parent:Node<K,V>,cmp

		While node
			parent=node
			cmp=Compare( key,node.key )
			If cmp>0
				node=node.right
			Else If cmp<0
				node=node.left
			Else
				node.value=value
				Return
			EndIf
		Wend
		
		node=New Node<K,V>( key,value,RED,parent )
		
		If Not parent
			root=node
			Return
		Endif

		If cmp>0
			parent.right=node
		Else
			parent.left=node
		Endif
		
		InsertFixup node
	End
	
	Method Get:V( key:K )
		Local node:=FindNode( key )
		If node Return node.value
	End
	
	Method Remove( key:K )
		Local node:=FindNode( key )
		If Not node Return 0
		RemoveNode node
		Return 1
	End
	
	Method Keys:MapKeys<K,V>()
		Return New MapKeys<K,V>( Self )
	End
	
	Method Values:MapValues<K,V>()
		Return New MapValues<K,V>( Self )
	End
	
	Method ObjectEnumerator:NodeEnumerator<K,V>()
		Return New NodeEnumerator<K,V>( FirstNode() )
	End

	'Deprecated - use Set
	Method Insert( key:K,value:V )
		Return Set( key,value )
	End

	'Deprecated - use Get
	Method ValueForKey:V( key:K )
		Return Get( key )
	End

	'This is how key comparisons are currently implemented....
	Method Compare( lhs:Object,rhs:Object ) Abstract
	
Private

	Method FirstNode:Node<K,V>()
		If Not root Return

		Local node:=root
		While node.left
			node=node.left
		Wend
		Return node
	End
	
	Method LastNode:Node<K,V>()
		If Not root Return

		Local node:=root
		While node.right
			node=node.right
		Wend
		Return node
	End
	
	Method FindNode:Node<K,V>( key:K )
		Local node:=root

		While node
			Local cmp=Compare( key,node.key )
			If cmp>0
				node=node.right
			Else If cmp<0
				node=node.left
			Else
				Return node
			EndIf
		Wend
		Return node
	End
	
	Method RemoveNode( node:Node<K,V> )
		Local splice:Node<K,V>,child:Node<K,V>
		If Not node.left
			splice=node
			child=node.right
		Else If Not node.right
			splice=node
			child=node.left
		Else
			splice=node.left
			While splice.right
				splice=splice.right
			Wend
			child=splice.left
			node.key=splice.key
			node.value=splice.value
		EndIf
		Local parent:=splice.parent
		If child
			child.parent=parent
		EndIf
		If Not parent
			root=child
			Return
		EndIf
		If splice=parent.left
			parent.left=child
		Else
			parent.right=child
		EndIf
		If splice.color=BLACK DeleteFixup child,parent
	End
	
	Method InsertFixup( node:Node<K,V> )
		While node.parent And node.parent.color=RED And node.parent.parent
			If node.parent=node.parent.parent.left
				Local uncle:=node.parent.parent.right
				If uncle And uncle.color=RED
					node.parent.color=BLACK
					uncle.color=BLACK
					uncle.parent.color=RED
					node=uncle.parent
				Else
					If node=node.parent.right
						node=node.parent
						RotateLeft node
					EndIf
					node.parent.color=BLACK
					node.parent.parent.color=RED
					RotateRight node.parent.parent
				EndIf
			Else
				Local uncle:=node.parent.parent.left
				If uncle And uncle.color=RED
					node.parent.color=BLACK
					uncle.color=BLACK
					uncle.parent.color=RED
					node=uncle.parent
				Else
					If node=node.parent.left
						node=node.parent
						RotateRight node
					EndIf
					node.parent.color=BLACK
					node.parent.parent.color=RED
					RotateLeft node.parent.parent
				EndIf
			EndIf
		Wend
		root.color=BLACK
	End
	
	Method RotateLeft( node:Node<K,V> )
		Local child:=node.right
		node.right=child.left
		If child.left
			child.left.parent=node
		EndIf
		child.parent=node.parent
		If node.parent
			If node=node.parent.left
				node.parent.left=child
			Else
				node.parent.right=child
			EndIf
		Else
			root=child
		EndIf
		child.left=node
		node.parent=child
	End
	
	Method RotateRight( node:Node<K,V> )
		Local child:=node.left
		node.left=child.right
		If child.right
			child.right.parent=node
		EndIf
		child.parent=node.parent
		If node.parent
			If node=node.parent.right
				node.parent.right=child
			Else
				node.parent.left=child
			EndIf
		Else
			root=child
		EndIf
		child.right=node
		node.parent=child
	End
	
	Method DeleteFixup( node:Node<K,V>,parent:Node<K,V> )
		While node And node<>root And node.color=BLACK
			If node=parent.left
				Local sib:=parent.right
				If sib.color=RED
					sib.color=BLACK
					parent.color=RED
					RotateLeft parent
					sib=parent.right
				EndIf
				If sib.left.color=BLACK And sib.right.color=BLACK
					sib.color=RED
					node=parent
					parent=parent.parent
				Else
					If sib.right.color=BLACK
						sib.left.color=BLACK
						sib.color=RED
						RotateRight sib
						sib=parent.right
					Endif
					sib.color=parent.color
					parent.color=BLACK
					sib.right.color=BLACK
					RotateLeft parent
					node=root
				EndIf
			Else	
				Local sib:=parent.left
				If sib.color=RED
					sib.color=BLACK
					parent.color=RED
					RotateRight parent
					sib=parent.left
				EndIf
				If sib.right.color=BLACK And sib.left.color=BLACK
					sib.color=RED
					node=parent
					parent=parent.parent
				Else
					If sib.left.color=BLACK
						sib.right.color=BLACK
						sib.color=RED
						RotateLeft sib
						sib=parent.left
					EndIf
					sib.color=parent.color
					parent.color=BLACK
					sib.left.color=BLACK
					RotateRight parent
					node=root
				EndIf
			EndIf
		Wend
		If node node.color=BLACK
	End
	
	Const RED=-1
	Const BLACK=1
	
	Field root:Node<K,V>
	
End

Class Node<K,V>

	Method New( key:K,value:V,color,parent:Node<K,V> )
		Self.key=key
		Self.value=value
		Self.color=color
		Self.parent=parent
	End
	
	Method Count( n )
		If left n=left.Count( n )
		If right n=right.Count( n )
		Return n+1
	End

	Method Key:K()
		Return key
	End
	
	Method Value:V()
		Return value
	End

	Method NextNode:Node()
		Local node:Node
		If right
			node=right
			While node.left
				node=node.left
			Wend
			Return node
		Endif
		node=Self
		Local parent:=Self.parent
		While parent And node=parent.right
			node=parent
			parent=parent.parent
		Wend
		Return parent
	End
	
	Method PrevNode:Node()
		Local node:Node
		If left
			node=left
			While node.right
				node=node.right
			Wend
			Return node
		EndIf
		node=Self
		Local parent:Node=Self.parent
		While parent And node=parent.left
			node=parent
			parent=parent.parent
		Wend
		Return parent
	End
	
	Method Copy:Node( parent:Node )
		Local t:Node=New Node( key,value,color,parent )
		If left t.left=left.Copy( t )
		If right t.right=right.Copy( t )
		Return t
	End
	
Private
	
	Field key:K,value:V
	Field color,parent:Node,left:Node,right:Node

End

Class NodeEnumerator<K,V>

	Method New( node:Node<K,V> )
		Self.node=node
	End
	
	Method HasNext()
		Return node<>Null
	End
	
	Method NextObject:Node<K,V>()
		Local t:=node
		node=node.NextNode()
		Return t
	End

Private

	Field node:Node<K,V>
	
End

Class KeyEnumerator<K,V>

	Method New( node:Node<K,V> )
		Self.node=node
	End
	
	Method HasNext()
		Return node<>Null
	End
	
	Method NextObject:K()
		Local t:=node
		node=node.NextNode()
		Return t.key
	End

Private

	Field node:Node<K,V>
	
End

Class ValueEnumerator<K,V>

	Method New( node:Node<K,V> )
		Self.node=node
	End
	
	Method HasNext()
		Return node<>Null
	End
	
	Method NextObject:V()
		Local t:=node
		node=node.NextNode()
		Return t.value
	End

Private

	Field node:Node<K,V>
	
End

Class MapKeys<K,V>

	Method New( map:Map<K,V> )
		Self.map=map
	End

	Method ObjectEnumerator:KeyEnumerator<K,V>()
		Return New KeyEnumerator<K,V>( map.FirstNode() )
	End
	
Private

	Field map:Map<K,V>
		
End

Class MapValues<K,V>

	Method New( map:Map<K,V> )
		Self.map=map
	End

	Method ObjectEnumerator:ValueEnumerator<K,V>()
		Return New ValueEnumerator<K,V>( map.FirstNode() )
	End
	
Private

	Field map:Map<K,V>
	
End

'***** Box object versions *****

Class IntMap<V> Extends Map<IntObject,V>

	Method Compare( lhs:Object,rhs:Object )
		Local l:=IntObject( lhs ).value
		Local r:=IntObject( rhs ).value
		If l<r Return -1
		Return l>r
	End

End

Class FloatMap<V> Extends Map<FloatObject,V>

	Method Compare( lhs:Object,rhs:Object )
		Local l:=FloatObject( lhs ).value
		Local r:=FloatObject( rhs ).value
		If l<r Return -1
		Return l>r
	End

End

Class StringMap<V> Extends Map<StringObject,V>

	Method Compare( lhs:Object,rhs:Object )
		Local l:=StringObject( lhs ).value
		Local r:=StringObject( rhs ).value
		If l<r Return -1
		Return l>r
	End

End
