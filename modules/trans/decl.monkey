
' Module trans.decl
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Const DECL_EXTERN=		$010000
Const DECL_PRIVATE=	$020000
Const DECL_ABSTRACT=	$040000
Const DECL_FINAL=		$080000

Const DECL_SEMANTED=	$100000
Const DECL_SEMANTING=	$200000

Global _env:ScopeDecl
Global _envStack:=New List<ScopeDecl>

Global _loopnest

Function PushEnv( env:ScopeDecl )
	If _env _envStack.AddLast( _env )
	_env=env
End Function

Function PopEnv()
	_env=ScopeDecl( _envStack.RemoveLast() )
End Function

Class FuncDeclList Extends List<FuncDecl>
End

Class Decl
	Field ident$
	Field munged$
	Field errInfo$
	Field actual:Decl
	Field scope:ScopeDecl
	Field attrs
	
	Method New()
		errInfo=_errInfo
		actual=Self
	End
	
	Method ToString$()
		If ClassDecl( scope ) Return scope.ToString()+"."+ident
		Return ident
	End
	
	Method IsExtern()
		Return (attrs & DECL_EXTERN)<>0
	End
	
	Method IsPrivate()
		Return (attrs & DECL_PRIVATE)<>0
	End
	
	Method IsAbstract()
		Return (attrs & DECL_ABSTRACT)<>0
	End
	
	Method IsSemanted()
		Return (attrs & DECL_SEMANTED)<>0
	End
	
	Method IsSemanting()
		Return (attrs & DECL_SEMANTING)<>0
	End
	
	Method FuncScope:FuncDecl()
		If FuncDecl( Self ) Return FuncDecl( Self )
		If scope Return scope.FuncScope()
	End

	Method ClassScope:ClassDecl()
		If ClassDecl( Self ) Return ClassDecl( Self )
		If scope Return scope.ClassScope()
	End
	
	Method ModuleScope:ModuleDecl()
		If ModuleDecl( Self ) Return ModuleDecl( Self )
		If scope Return scope.ModuleScope()
	End
	
	Method AppScope:AppDecl()
		If AppDecl( Self ) Return AppDecl( Self )
		If scope Return scope.AppScope()
	End
	
	Method CheckAccess()
		If IsPrivate() And ModuleScope()<>_env.ModuleScope() Return False
		Return True
	End
	
	Method AssertAccess()
		If Not CheckAccess()
			Err ToString() +" is private."
		Endif
	End
	
	Method Semant()
		If IsSemanted() Return
		
		If IsSemanting() Err "Cyclic declaration of '"+ident+"'."
		
		If actual<>Self
			actual.Semant
		Endif

		PushErr errInfo
		
		If scope
			PushEnv scope
		Endif
		
		attrs|=DECL_SEMANTING

		'If ident And ClassScope() Print "Semanting "+ToString()

		OnSemant
		
		attrs&=~DECL_SEMANTING

		attrs|=DECL_SEMANTED

		If scope 
			If Not IsExtern()

				scope.semanted.AddLast Self
				
				If GlobalDecl( Self )
					AppScope.semantedGlobals.AddLast GlobalDecl( Self )
				Endif
				
				If ModuleDecl( scope )
					AppScope.semanted.AddLast Self
				Endif
			
			Endif
			
			PopEnv
		Endif
		
		PopErr
	End
	
	Method InitInstance:Decl( decl:Decl )
		decl.ident=ident
		decl.munged=munged
		decl.errInfo=errInfo
		decl.actual=actual
		decl.scope=Null
		decl.attrs=attrs & ~(DECL_SEMANTED|DECL_SEMANTING)
		Return decl
	End
	
	Method GenInstance:Decl()
		InternalErr
	End
	
	Method OnSemant() Abstract

End

Class ValDecl Extends Decl
	'pre-semant
	Field declTy:Type
	Field declInit:Expr
	'post-semant
	Field ty:Type
	Field init:Expr
	
	Method ToString$()
		Local t$=Super.ToString()
		If ty Return t+":"+ty.ToString()
		If declTy Return t+":"+declTy.ToString()
		Return t+":?"
	End
	
	Method OnSemant()
		If declTy
			ty=declTy.Semant()
			If declInit init=declInit.Copy().Semant(ty)
		Else If declInit
			init=declInit.Copy().Semant()
			ty=init.exprType
		Else
			InternalErr
		Endif
	End
	
End

Class ConstDecl Extends ValDecl
	Field value$
	
	Method New( ident$,ty:Type,init:Expr,attrs )
		Self.ident=ident
		Self.munged=ident
		Self.declTy=ty
		Self.declInit=init
		Self.attrs=attrs
	End
	
	Method GenInstance:Decl()
		Local inst:=New ConstDecl
		InitInstance inst
		inst.declTy=declTy
		inst.declInit=declInit
		Return inst
	End
	
	Method OnSemant()
		Super.OnSemant()
		If Not IsExtern() value=init.Eval()
	End
	
End

Class VarDecl Extends ValDecl

End

Class LocalDecl Extends VarDecl

	Method New( ident$,ty:Type,init:Expr,attrs=0 )
		Self.ident=ident
		Self.declTy=ty
		Self.declInit=init
		Self.attrs=attrs
	End
	
	Method ToString$()
		Return "Local "+Super.ToString()
	End

End

Class ArgDecl Extends LocalDecl
	
	Method New( ident$,ty:Type,init:Expr,attrs=0 )
		Self.ident=ident
		Self.declTy=ty
		Self.declInit=init
		Self.attrs=attrs
	End
	
	Method GenInstance:Decl()
		Local inst:=New ArgDecl
		InitInstance inst
		inst.declTy=declTy
		inst.declInit=declInit
		Return inst
	End
	
	Method ToString$()
		Return Super.ToString()
	End

	
End

Class GlobalDecl Extends VarDecl
	
	Method New( ident$,ty:Type,init:Expr,attrs=0 )
		Self.ident=ident
		Self.declTy=ty
		Self.declInit=init
		Self.attrs=attrs
	End

	Method ToString$()
		Return "Global "+Super.ToString()
	End

	Method GenInstance:Decl()
'		PushErr errInfo
'		Err "Global variables cannot be used inside generic classes."
		Local inst:=New GlobalDecl
		InitInstance inst
		inst.declTy=declTy
		inst.declInit=declInit
		Return inst
	End
	
End

Class FieldDecl Extends VarDecl

	Method New( ident$,ty:Type,init:Expr,attrs=0 )
		Self.ident=ident
		Self.declTy=ty
		Self.declInit=init
		Self.attrs=attrs
	End

	Method ToString$()
		Return "Field "+Super.ToString()
	End
	
	Method GenInstance:Decl()
		Local inst:=New FieldDecl
		InitInstance inst
		inst.declTy=declTy
		inst.declInit=declInit
		Return inst
	End
	
End

Class AliasDecl Extends Decl

	Field decl:Object
	
	Method New( ident$,decl:Object,attrs=0 )
		Self.ident=ident
		Self.decl=decl
		Self.attrs=attrs
	End
	
	Method OnSemant()
	End
	
End

Class ScopeDecl Extends Decl

Private

	Field decls:=New List<Decl>
	Field semanted:=New List<Decl>

	Field declsMap:=New StringMap<Object>

Public

	Method Decls:List<Decl>()
		Return decls
	End
	
	Method Semanted:List<Decl>()
		Return semanted
	End
	
	Method FuncDecls:List<FuncDecl>( id$="" )
		Local fdecls:=New List<FuncDecl>
		For Local decl:Decl=Eachin decls
			If id And decl.ident<>id Continue
			Local fdecl:=FuncDecl( decl )
			If fdecl fdecls.AddLast fdecl
		Next
		Return fdecls
	End
	
	Method MethodDecls:List<FuncDecl>( id$="" )
		Local fdecls:=New List<FuncDecl>
		For Local decl:Decl=Eachin decls
			If id And decl.ident<>id Continue
			Local fdecl:=FuncDecl( decl )
			If fdecl And fdecl.IsMethod() fdecls.AddLast fdecl
		Next
		Return fdecls
	End
	
	Method SemantedFuncs:List<FuncDecl>( id$="" )
		Local fdecls:=New List<FuncDecl>
		For Local decl:Decl=Eachin semanted
			If id And decl.ident<>id Continue
			Local fdecl:=FuncDecl( decl )
			If fdecl fdecls.AddLast fdecl
		Next
		Return fdecls
	End
	
	Method SemantedMethods:List<FuncDecl>( id$="" )
		Local fdecls:=New List<FuncDecl>
		For Local decl:Decl=Eachin semanted
			If id And decl.ident<>id Continue
			Local fdecl:=FuncDecl( decl )
			If fdecl And fdecl.IsMethod() fdecls.AddLast fdecl
		Next
		Return fdecls
	End
	
	Method InsertDecl( decl:Decl )
		If decl.scope InternalErr
		
		Local ident$=decl.ident
		If Not ident Return
		
		decl.scope=Self
		decls.AddLast decl
		
		Local decls:StringMap<Object>
		Local tdecl:=declsMap.Get( ident )
		
		If FuncDecl( decl )
			Local funcs:=FuncDeclList( tdecl )
			If funcs Or Not tdecl
				If Not funcs
					funcs=New FuncDeclList
					declsMap.Insert ident,funcs
				Endif
				funcs.AddLast FuncDecl( decl )
			Else
				Err "Duplicate identifier '"+ident+"'."
			Endif
		Else If Not tdecl
			declsMap.Insert ident,decl
		Else
			Err "Duplicate identifier '"+ident+"'."
		Endif

	End

	Method InsertDecls( decls:List<Decl> )
		For Local decl:Decl=Eachin decls
			InsertDecl decl
		Next
	End
	
	'This is overridden by ClassDecl and ModuleDecl
	Method GetDecl:Object( ident$ )
		Local decl:=declsMap.Get( ident )
		If Not decl Return
		
		Local adecl:=AliasDecl( decl )
		If Not adecl Return decl
		
		If adecl.CheckAccess() Return adecl.decl
	End
	
	Method FindDecl:Object( ident$ )
		Local decl:Object=GetDecl( ident )
		If decl Return decl
		If scope Return scope.FindDecl( ident )
	End
	
	Method FindValDecl:ValDecl( ident$ )
		Local decl:ValDecl=ValDecl( FindDecl( ident ) )
		If Not decl Return
		decl.AssertAccess
		decl.Semant
		Return decl
	End
	
	Method FindScopeDecl:ScopeDecl( ident$ )
		Local decl:=ScopeDecl( FindDecl( ident ) )
		If Not decl Return
		decl.AssertAccess
		decl.Semant
		Return decl
	End
	
	Method FindClassDecl:ClassDecl( ident$,args:ClassDecl[] )
		Local decl:=ClassDecl( GetDecl( ident ) )
		If Not decl
			If scope Return scope.FindClassDecl( ident,args )
			Return
		Endif
		decl.AssertAccess
		decl.Semant
		Return decl.GenClassInstance( args )
	End
	
	Method FindModuleDecl:ModuleDecl( ident$ )
		Local decl:ModuleDecl=ModuleDecl( GetDecl( ident ) )
		If Not decl
			If scope Return scope.FindModuleDecl( ident )
			Return
		Endif
		decl.AssertAccess
		decl.Semant
		Return decl
	End
	
	Method FindFuncDecl:FuncDecl( ident$,argExprs:Expr[],explicit=False )
	
		Local funcs:=FuncDeclList( FindDecl( ident ) )
		If Not funcs Return
	
		For Local func:=Eachin funcs
			func.Semant()
		Next
		
		Local match:FuncDecl,isexact,err$

		For Local func:FuncDecl=Eachin funcs
			If Not func.CheckAccess() Continue
			
			Local argDecls:ArgDecl[]=func.argDecls
			
			If argExprs.Length>argDecls.Length Continue
			
			Local exact=True
			Local possible=True
			
			For Local i=0 Until argDecls.Length

				If i<argExprs.Length And argExprs[i]
				
					Local declTy:Type=argDecls[i].ty
					Local exprTy:Type=argExprs[i].exprType
					
					If exprTy.EqualsType( declTy ) Continue
					
					exact=False
					
					If Not explicit And exprTy.ExtendsType( declTy ) Continue

				Else If argDecls[i].init

					exact=False

					If Not explicit Continue
				Endif
			
				possible=False
				Exit
			Next
			
			If Not possible Continue
			
			If exact
				If isexact
					Err "Unable to determine overload to use: "+match.ToString()+" or "+func.ToString()+"."
				Else
					err=""
					match=func
					isexact=True
				Endif
			Else
				If Not isexact
					If match 
						err="Unable to determine overload to use: "+match.ToString()+" or "+func.ToString()+"."
					Else
						match=func
					Endif
				Endif
			Endif
			
		Next
		
		If Not isexact
			If err Err err
			If explicit Return
		Endif
		
		If Not match
			Local t$
			For Local i=0 Until argExprs.Length
				If t t+=","
				If argExprs[i] t+=argExprs[i].exprType.ToString()
			Next
			Err "Unable to find overload for "+ident+"("+t+")."
		Endif
		
		match.AssertAccess

		Return match
	End
	
	Method OnSemant()
	End
	
End

Class BlockDecl Extends ScopeDecl
	Field stmts:=New List<Stmt>
	
	Method New( scope:ScopeDecl )
		Self.scope=scope
		
	End
	
	Method AddStmt( stmt:Stmt )
		stmts.AddLast stmt
	End
	
	Method OnSemant()
		PushEnv Self
		For Local stmt:Stmt=Eachin stmts
			stmt.Semant
		Next
		PopEnv
	End
	
End

Const FUNC_METHOD=1			'mutually exclusive with ctor
Const FUNC_CTOR=2
Const FUNC_PROPERTY=4

'Fix! A func is NOT a block/scope!
'
Class FuncDecl Extends BlockDecl

	Field retType:Type
	Field retTypeExpr:Type
	Field argDecls:ArgDecl[]

	Field overrides:FuncDecl
	Field superCtor:InvokeSuperExpr
	
	Method New( ident$,ty:Type,argDecls:ArgDecl[],attrs )
		Self.ident=ident
		Self.retTypeExpr=ty
		Self.argDecls=argDecls
		Self.attrs=attrs
	End
	
	Method GenInstance:Decl()
		Local inst:=New FuncDecl
		InitInstance inst
		inst.retTypeExpr=retTypeExpr
		inst.argDecls=argDecls[..]
		For Local i=0 Until argDecls.Length
			inst.argDecls[i]=ArgDecl( argDecls[i].GenInstance() )
		Next
		Return inst
	End
	
	Method ToString$()
		Local t$
		For Local decl:ArgDecl=Eachin argDecls
			If t t+=","
			t+=decl.ToString()
		Next
		Local q$
		If IsCtor()
			q="Method "+Super.ToString()
		Else
			If IsMethod() q="Method " Else q="Function "
			q+=Super.ToString()+":"
			If retType
				q+=retType.ToString()
			Else If retTypeExpr 
				q+=retTypeExpr.ToString()
			Else
				q+="?"
			Endif
		Endif
		Return q+"("+t+")"
	End
	
	Method IsCtor()
		Return (attrs & FUNC_CTOR)<>0
	End

	Method IsMethod()
		Return (attrs & FUNC_METHOD)<>0
	End
	
	Method IsStatic()
		Return (attrs & (FUNC_METHOD|FUNC_CTOR))=0
	End
	
	Method IsProperty()
		Return (attrs & FUNC_PROPERTY)<>0
	End
	
	Method EqualsArgs( decl:FuncDecl )
		If argDecls.Length<>decl.argDecls.Length Return False
		For Local i=0 Until argDecls.Length
			If Not argDecls[i].ty.EqualsType( decl.argDecls[i].ty ) Return False
		Next
		Return True
	End

	Method EqualsFunc( decl:FuncDecl )
		Return retType.EqualsType( decl.retType ) And EqualsArgs( decl )
	End

	Method OnSemant()

		'semant ret type
		retType=retTypeExpr.Semant()
		If ArrayType( retType ) And Not retType.EqualsType( retType.ActualType() )
'			Err "Return type cannot be an array of generic objects."
		Endif
		
		'semant args
		For Local arg:ArgDecl=Eachin argDecls
			InsertDecl arg
			arg.Semant
		Next
		
		If actual<>Self Return
		
		'check for duplicate decl
		For Local decl:=Eachin scope.SemantedFuncs( ident )
			If decl<>Self And EqualsArgs( decl )
				Err "Duplicate declaration "+ToString()
			Endif
		Next
		
		'get cdecl, sclasss
		Local cdecl:=ClassScope(),sclass:ClassDecl
		If cdecl sclass=ClassDecl( cdecl.superClass )
		
		'prefix call to super ctor if necessary
		If IsCtor() And superCtor=Null 
			If sclass.FindFuncDecl( "new",[] )
				superCtor=New InvokeSuperExpr( "new",[] )
				stmts.AddFirst New ExprStmt( superCtor )
			Endif
		Endif
		
		'append a return statement if necessary
		If Not IsExtern() And Not VoidType( retType ) And Not ReturnStmt( stmts.Last() )
			Local stmt:ReturnStmt
			If IsCtor()
				stmt=New ReturnStmt( Null )
			Else
				stmt=New ReturnStmt( New ConstExpr( retType,"" ) )
			Endif
			stmt.errInfo=errInfo
			stmts.AddLast stmt
		Endif

		'check we exactly match an override
		If sclass And IsMethod()
			While sclass
				Local found
				For Local decl:=Eachin sclass.MethodDecls( ident )
					found=True
					decl.Semant
					If EqualsFunc( decl ) 
						overrides=FuncDecl( decl.actual )
						If overrides.munged
							If munged And munged<>overrides.munged
								InternalErr
							Endif
							munged=overrides.munged
						Endif
					Endif
				Next
				If found
					If Not overrides Err "Overriding method does not match any overridden method."
					Exit
				Endif
				sclass=sclass.superClass
			Wend
		Endif
		
		attrs|=DECL_SEMANTED
		
		Super.OnSemant()
	End
	
End

Const CLASS_INTERFACE=1
Const CLASS_TEMPLATEARG=2
Const CLASS_TEMPLATEINST=4
Const CLASS_INSTANCED=8		'class used in New?

Class ClassDecl Extends ScopeDecl

	Field args:ClassDecl[]
	Field superTy:IdentType
	Field impltys:IdentType[]

	Field superClass:ClassDecl
	
	Field implments:ClassDecl[]			'interfaces immediately implemented
	Field implmentsAll:ClassDecl[]		'all interfaces implemented
	
	Field instanceof:ClassDecl			'for instances
	Field instances:List<ClassDecl>		'for actual (non-arg, non-instance)
	
	Global nullObjectClass:=New ClassDecl( "{NULL}",[],Null,[],DECL_ABSTRACT|DECL_EXTERN )
	
	Method New( ident$,args:ClassDecl[],superTy:IdentType,impls:IdentType[],attrs )
		Self.ident=ident
		Self.args=args
		Self.superTy=superTy
		Self.impltys=impls
		Self.attrs=attrs
		If args
			instances=New List<ClassDecl>
			instances.AddLast Self
		Endif
	End
	
	Method ToString$()
		Local t$
		For Local i=0 Until args.Length
			If i t+=","
			t+=args[i].ToString()
		Next
		If t t="<"+t+">"
		Return ident+t
	End
	
	Method GenClassInstance:ClassDecl( instArgs:ClassDecl[] )
		If Not IsSemanted() InternalErr
		
		'no args
		If Not instArgs
			If Not args Return Self
			If instanceof Return Self
			For Local inst:=Eachin instances
				If _env.ClassScope()=inst Return inst
			Next
		Endif
		
		'If Not instanceof And Not instArgs Return Self
		
		'check number of args
		If instanceof Or args.Length<>instArgs.Length
			Err "Wrong number of class arguments for "+ToString()
		Endif
		
		'look for existing instance
		For Local inst:=Eachin instances
			Local equal=True
			For Local i=0 Until args.Length
				If inst.args[i]=instArgs[i] Continue
				equal=False
				Exit
			Next
			If equal Return inst
		Next
		
		Local inst:ClassDecl=New ClassDecl

		InitInstance inst

		inst.scope=scope
		inst.attrs|=CLASS_TEMPLATEINST
		inst.args=instArgs
		inst.superTy=superTy
		inst.instanceof=Self
		instances.AddLast inst
		
		For Local i=0 Until args.Length
			inst.InsertDecl New AliasDecl( args[i].ident,instArgs[i] )
		Next
		
		For Local decl:Decl=Eachin decls
			If ClassDecl( decl ) Continue
			inst.InsertDecl decl.GenInstance()
		Next

		'inst.Semant
		'A bit cheeky...
		inst.OnSemant
		inst.attrs|=DECL_SEMANTED
		
		Return inst
	End

	Method IsInterface()
		Return (attrs & CLASS_INTERFACE)<>0
	End
	
	Method IsTemplateArg()
		Return (attrs & CLASS_TEMPLATEARG)<>0
	End
	
	Method IsTemplateInst()
		Return (attrs & CLASS_TEMPLATEINST)<>0
	End
	
	Method IsInstanced()
		Return (attrs & CLASS_INSTANCED)<>0
	End
	
	Method GetDecl:Object( ident$ )
	
		Local cdecl:ClassDecl=Self
		While cdecl
			Local decl:=cdecl.GetDecl2( ident )
			If decl Return decl
			
			cdecl=cdecl.superClass
		Wend

	End
	
	'needs this 'coz you can't go blah.Super.GetDecl()...
	Method GetDecl2:Object( ident$ )
		Return Super.GetDecl( ident )
	End
	
	Method FindFuncDecl:FuncDecl( ident$,args:Expr[],explicit=False )
	
		If Not IsInterface()
			Return FindFuncDecl2( ident,args,explicit )
		Endif
		
		Local fdecl:=FindFuncDecl2( ident,args,True )
		
		For Local iface:=Eachin implmentsAll
			Local decl:=iface.FindFuncDecl2( ident,args,True )
			If Not decl Continue
			
			If fdecl
				If fdecl.EqualsFunc( decl ) Continue
				Err "Unable to determine overload to use: "+fdecl.ToString()+" or "+decl.ToString()+"."
			Endif
			fdecl=decl
		Next
		
		If fdecl Or explicit Return fdecl
		
		fdecl=FindFuncDecl2( ident,args,False )
		
		For Local iface:=Eachin implmentsAll
			Local decl:=iface.FindFuncDecl2( ident,args,False )
			If Not decl Continue
			
			If fdecl
				If fdecl.EqualsFunc( decl ) Continue
				Err "Unable to determine overload to use: "+fdecl.ToString()+" or "+decl.ToString()+"."
			Endif
			fdecl=decl
		Next
		
		Return fdecl
	End
	
	Method FindFuncDecl2:FuncDecl( ident$,args:Expr[],explicit )
		Return Super.FindFuncDecl( ident,args,explicit )
	End
	
	Method ExtendsClass( cdecl:ClassDecl )
		If Self=nullObjectClass Return True
		
		If cdecl.IsTemplateArg()
			cdecl=Type.objectType.FindClass()
		Endif
		
		Local tdecl:=Self
		While tdecl
			If tdecl=cdecl Return True
			If cdecl.IsInterface()
				For Local iface:=Eachin tdecl.implmentsAll
					If iface=cdecl Return True
				Next
			Endif
			tdecl=tdecl.superClass
		Wend
		
		Return False
	End
	
	Method OnSemant()

		'Print "Semanting "+ToString()
		
		PushEnv Self

		If Not IsTemplateInst()
			For Local i=0 Until args.Length
				InsertDecl args[i]
				args[i].Semant
			Next
		Endif
		
		'Semant superclass		
		If superTy
			superClass=superTy.FindClass()
			If superClass.IsInterface() Err superClass.ToString()+" is an interface, not a class."
		Endif
		
		'Semant implemented interfaces
		Local impls:=New ClassDecl[impltys.Length]
		Local implsall:=New Stack<ClassDecl>
		For Local i=0 Until impltys.Length
			Local cdecl:=impltys[i].FindClass()
			If Not cdecl.IsInterface()
				Err cdecl.ToString()+" is a class, not an interface."
			Endif
			For Local j=0 Until i
				If impls[j]=cdecl
					Err "Duplicate interface "+cdecl.ToString()+"."
				Endif
			Next
			impls[i]=cdecl
			implsall.Push cdecl
			For Local tdecl:=Eachin cdecl.implmentsAll
				implsall.Push tdecl
			Next
		Next
		implmentsAll=New ClassDecl[implsall.Length]
		For Local i=0 Until implsall.Length
			implmentsAll[i]=implsall.Get(i)
		Next
		implments=impls

		#rem
		If IsInterface()
			'add implemented methods to our methods
			For Local iface:=Eachin implmentsAll
				For Local decl:=Eachin iface.FuncDecls
					InsertAlias decl.ident,decl
				Next
			Next
		Endif
		#end
		
'		attrs|=DECL_SEMANTED
		
		PopEnv
		
		If IsTemplateArg()
			actual=Type.objectType.FindClass()
			Return
		Endif
		
		If IsTemplateInst()
			Return
		Endif
		
		'Are we abstract?
		If Not IsAbstract()
			For Local decl:Decl=Eachin decls
				Local fdecl:=FuncDecl( decl )
				If fdecl And fdecl.IsAbstract()
					attrs|=DECL_ABSTRACT
					Exit
				Endif
			Next
		Endif

		If Not IsExtern() And Not IsInterface()
			Local fdecl:FuncDecl
			For Local decl:FuncDecl=Eachin FuncDecls
				If Not decl.IsCtor() Continue
				Local nargs
				For Local arg:=Eachin decl.argDecls
					If Not arg.init nargs+=1
				Next
				If nargs Continue
				fdecl=decl
				Exit
			Next
			If Not fdecl
				fdecl=New FuncDecl( "new",New ObjectType( Self ),[],FUNC_CTOR )
				fdecl.AddStmt New ReturnStmt( Null )
				InsertDecl fdecl
			Endif
		Endif

		'NOTE: do this AFTER super semant so UpdateAttrs order is cool.
		AppScope.semantedClasses.AddLast Self
	End
	
	'Ok, this dodgy looking beast 'resurrects' methods that may not currently be alive, but override methods that ARE.
	Method UpdateLiveMethods()
	
		If IsInterface() Return

		If Not superClass Return

		Local n
		For Local decl:=Eachin MethodDecls
			If decl.IsSemanted() Continue
			
			Local live
			Local unsem:=New List<FuncDecl>
			
			unsem.AddLast decl
			
			Local sclass:=superClass
			While sclass
				For Local decl2:=Eachin sclass.MethodDecls( decl.ident )
					If decl2.IsSemanted()
						live=True
					Else
						unsem.AddLast decl2
						If decl2.IsExtern() live=True
						If decl2.actual.IsSemanted() live=True
					Endif
				Next
				sclass=sclass.superClass
			Wend
			
			If Not live
				Local cdecl:=Self
				While cdecl
					For Local iface:=Eachin cdecl.implmentsAll
						For Local decl2:=Eachin iface.MethodDecls( decl.ident )
							If decl2.IsSemanted()
								live=True
							Else
								unsem.AddLast decl2
								If decl2.IsExtern() live=True
								If decl2.actual.IsSemanted() live=True
							Endif
						Next
					Next
					cdecl=cdecl.superClass
				Wend
			Endif
			
			If Not live Continue
			
			For Local decl:=Eachin unsem
				decl.Semant
				n+=1
			Next
		Next
		
		Return n
	End
	
	Method FinalizeClass()

		PushErr errInfo
		
		If Not IsInterface()
			'
			'check for duplicate fields!
			'
			For Local decl:=Eachin Semanted
				Local fdecl:=FieldDecl( decl )
				If Not fdecl Continue
				Local cdecl:=superClass
				While cdecl
					For Local decl:=Eachin cdecl.Semanted
						If decl.ident=fdecl.ident Err "Field '"+fdecl.ident+"' in class "+ToString()+" overrides existing declaration in class "+cdecl.ToString()
					Next
					cdecl=cdecl.superClass
				Wend
			Next
			'
			'Check we implement all abstract methods!
			'
			If IsInstanced()
				Local cdecl:=Self
				Local impls:=New List<FuncDecl>
				While cdecl
					For Local decl:=Eachin cdecl.SemantedMethods()
						If decl.IsAbstract()
							Local found
							For Local decl2:=Eachin impls
								If decl.EqualsFunc( decl2 )
									found=True
									Exit
								Endif
							Next
							If Not found
								Err "Can't create instance of class "+ToString()+" due to abstract method "+decl.ToString()+"."
							Endif
						Else
							impls.AddLast decl
						Endif
					Next
					cdecl=cdecl.superClass
				Wend
			Endif
			'
			'Check we implement all interface methods!
			'
			For Local iface:=Eachin implmentsAll
				For Local decl:=Eachin iface.SemantedMethods()
					Local found
					For Local decl2:=Eachin SemantedMethods( decl.ident )
						If decl.EqualsFunc( decl2 )
							If decl2.munged
								Err "Extern methods cannot be used to implement interface methods."
							Endif
							found=True
						Endif
					Next
					If Not found
						Err decl.ToString()+" must be implemented by class "+ToString()
					Endif
				Next
			Next
		Endif
		
		PopErr
		
	End
	
End

Const MODULE_STRICT=1

Class ModuleDecl Extends ScopeDecl

	Field filepath$
	Field imported:=New StringMap<ModuleDecl>		'Maps filepath to modules
	Field pubImported:=New StringMap<ModuleDecl>	'Ditto for publicly imported modules

	Method ToString$()
		Return "Module "+munged
	End
	
	Method New( ident$,munged$,filepath$,attrs )
		Self.ident=ident
		Self.munged=munged
		Self.filepath=filepath
		Self.attrs=attrs
	End
	
	Method IsStrict()
		Return (attrs & MODULE_STRICT)<>0
	End
	
	Method GetDecl:Object( ident$ )
	
		Local todo:=New List<ModuleDecl>
		Local done:=New StringMap<ModuleDecl>
		
		todo.AddLast Self
		done.Insert filepath,Self
		
		Local decl:Object,declmod$
		
		While Not todo.IsEmpty()
	
			Local mdecl:ModuleDecl=todo.RemoveLast()
			Local tdecl:=mdecl.GetDecl2( ident )
			
			If tdecl And tdecl<>decl
				If mdecl=Self Return tdecl
				If decl
					Err "Duplicate identifier '"+ident+"' found in module '"+declmod+"' and module '"+mdecl.ident+"'."
				Endif
				decl=tdecl
				declmod=mdecl.ident
			Endif
			
			If Not _env Exit
			
			Local imps:=mdecl.imported
			If mdecl<>_env.ModuleScope() imps=mdecl.pubImported

			For Local mdecl2:=Eachin imps.Values()
				If Not done.Contains( mdecl2.filepath )
					todo.AddLast mdecl2
					done.Insert mdecl2.filepath,mdecl2
				Endif
			Next

		Wend
		
		Return decl
	End
	
	Method GetDecl2:Object( ident$ )
		Return Super.GetDecl( ident )
	End

	Method OnSemant()
	End

End

Class AppDecl Extends ScopeDecl

	Field imported:=New StringMap<ModuleDecl>			'maps modpath->mdecl
	
	Field mainModule:ModuleDecl
	Field mainFunc:FuncDecl	
		
	Field semantedClasses:=New List<ClassDecl>			'in-order (ie: base before derived) list of semanted classes
	Field semantedGlobals:=New List<GlobalDecl>			'in-order (ie: dependancy sorted) list of semanted globals

	Field fileImports:=New StringList
	
	Method InsertModule( mdecl:ModuleDecl )
		mdecl.scope=Self
		imported.Insert mdecl.filepath,mdecl
		If Not mainModule
			mainModule=mdecl
		Endif
	End
	
	Method OnSemant()
	
		_env=Null
		
		mainFunc=mainModule.FindFuncDecl( "Main",[] )
		
		If Not mainFunc Err "Function 'Main' not found."
		
		If Not IntType( mainFunc.retType ) Or mainFunc.argDecls.Length
			Err "Main function must be of type Main:Int()"
		Endif

		Repeat
			Local more
			For Local cdecl:=Eachin semantedClasses
				more+=cdecl.UpdateLiveMethods()
			Next
			If Not more Exit
		Forever
		
		For Local cdecl:=Eachin semantedClasses
			cdecl.FinalizeClass
		Next
	End
	
End
