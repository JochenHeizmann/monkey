
' Module trans.decl
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Const DECL_NATIVE=		$010000
Const DECL_EXTERN=		DECL_NATIVE
Const DECL_PRIVATE=		$020000
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
		Return ident
	End
	
	Method IsNative()
		Return (attrs & DECL_NATIVE)<>0
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
		If IsPrivate()
			If ModuleScope()=_env.ModuleScope()
				Return
			Endif
			Err "Cannot access private declaration '"+ident+"'."
		Endif
	End
	
	Method Semant()
		If IsSemanted() Return
		
		If IsSemanting() Err "Cyclic declaration of '"+ident+"'."

		PushErr errInfo
		
'		If ident Print "Semanting:"+ident+errInfo

		If scope
			PushEnv scope
		Endif
		
		attrs|=DECL_SEMANTING

		'If ident Print "Semanting: "+ToString()
		
		OnSemant
		
		attrs&=~DECL_SEMANTING
		attrs|=DECL_SEMANTED

		If scope
			scope.semanted.AddLast Self
			
			If Not IsExtern And GlobalDecl( Self )
				AppScope.semantedGlobals.AddLast GlobalDecl( Self )
			Endif
			
			If Not IsExtern And ModuleDecl( scope )
				AppScope.semanted.AddLast Self
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
	
	Method GenInstance:Decl( inst:ClassDecl )
		Return Self
	End
	
	Method OnSemant() Abstract

End

Class ValDecl Extends Decl
	Field ty:Type
	Field init:Expr
	
	Method ToString$()
		If ty Return ident+":"+ty.ToString()
		Return ident+":???"
	End
	
	Method OnSemant()

		If actual<>Self
			actual.Semant
			ty=ValDecl( actual ).ty
			init=ValDecl( actual ).init
			ty=ty.GenInstance( ClassScope ).Semant()
			Return
		Endif
		
		If ty
			ty=ty.Semant()
			If init init=init.Semant().CastTo( ty )
		Else If init
			init=init.Semant()
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
		Self.ty=ty
		Self.init=init
		Self.attrs=attrs
	End
	
	Method OnSemant()
		Super.OnSemant()
		value=init.Eval()
	End
	
	Method GenInstance:Decl( inst:ClassDecl )
		Return InitInstance( New ConstDecl )
	End
	
End

Class VarDecl Extends ValDecl

End

Class LocalDecl Extends VarDecl

	Method New( ident$,ty:Type,init:Expr,attrs=0 )
		Self.ident=ident
		Self.ty=ty
		Self.init=init
		Self.attrs=attrs
	End

End

Class ArgDecl Extends LocalDecl
	
	Method New( ident$,ty:Type,init:Expr,attrs=0 )
		Self.ident=ident
		Self.ty=ty
		Self.init=init
		Self.attrs=attrs
	End

	Method GenInstance:Decl( inst:ClassDecl )
		Return InitInstance( New ArgDecl )
	End

End

Class GlobalDecl Extends VarDecl
	Field value$

	Method ToString$()
		Return "Global "+Super.ToString()
	End
	
	Method New( ident$,ty:Type,init:Expr,attrs=0 )
		Self.ident=ident
		Self.ty=ty
		Self.init=init
		Self.attrs=attrs
	End
	
End

Class FieldDecl Extends VarDecl
	Field index

	Method ToString$()
		Return "Field "+Super.ToString()
	End
	
	Method New( ident$,ty:Type,init:Expr,attrs=0 )
		Self.ident=ident
		Self.ty=ty
		Self.init=init
		Self.attrs=attrs
	End

	Method GenInstance:Decl( inst:ClassDecl )
		Return InitInstance( New FieldDecl )
	End
	
End

Class ScopeDecl Extends Decl

Private

	Field decls:=New List<Decl>
	Field semanted:=New List<Decl>
	
	Field vdecls:=New StringMap<Decl>
	Field fdecls:=New StringMap<FuncDeclList>

Public

	Method Decls:List<Decl>()
		Return decls
	End
	
	Method FieldDecls:List<FieldDecl>()
		Local fdecls:=New List<FieldDecl>
		For Local decl:Decl=Eachin decls
			Local fdecl:=FieldDecl( decl )
			If fdecl fdecls.AddLast fdecl
		Next
		Return fdecls
	End
	
	Method FuncDecls:List<FuncDecl>()
		Local fdecls:=New List<FuncDecl>
		For Local decl:Decl=Eachin decls
			Local fdecl:=FuncDecl( decl )
			If fdecl fdecls.AddLast fdecl
		Next
		Return fdecls
	End
	
	Method Semanted:List<Decl>()
		Return semanted
	End

	Method InsertDecl( decl:Decl )
		If decl.scope InternalErr
		
		If Not decl.ident Return
		
		decl.scope=Self
		decls.AddLast decl
		
		InsertAlias decl.ident,decl
	End
	
	Method InsertAlias( id$,decl:Decl )
		If ValDecl( decl ) Or ClassDecl( decl ) Or ModuleDecl( decl )
			If vdecls.Contains( id ) Or fdecls.Contains( id ) Err "Duplicate identifier '"+decl.ident+"'."
			vdecls.Insert id,decl
		Else If FuncDecl( decl )
			If vdecls.Contains( id ) Err "Duplicate identifier '"+decl.ident+"'."
			Local funcs:=fdecls.ValueForKey( id )
			If Not funcs
				funcs=New FuncDeclList
				fdecls.Insert id,funcs
			Endif
			funcs.AddLast FuncDecl( decl )
		Else
			InternalErr
		Endif
	End

	Method InsertDecls( decls:List<Decl> )
		For Local decl:Decl=Eachin decls
			InsertDecl decl
		Next
	End
	
	'This is overridden by ClassDecl and ModuleDecl
	Method GetDecl:Object( ident$ )
		Local decl:Object=vdecls.ValueForKey( ident )
		If decl Return decl
		Return fdecls.ValueForKey( ident )
	End
	
	Method FindDecl:Object( ident$ )
		Local decl:Object=GetDecl( ident )
		If decl Return decl
		If scope Return scope.FindDecl( ident )
	End
	
	Method FindValDecl:ValDecl( ident$ )
		Local decl:ValDecl=ValDecl( FindDecl( ident ) )
		If Not decl Return
		decl.Semant
		decl.CheckAccess
		Return decl
	End
	
	Method FindScopeDecl:ScopeDecl( ident$ )
		Local decl:ScopeDecl=ScopeDecl( FindDecl( ident ) )
		If Not decl Return
		decl.Semant
		decl.CheckAccess
		Return decl
	End
	
	Method FindClassDecl:ClassDecl( ident$,args:ClassDecl[] )
		Local decl:ClassDecl=ClassDecl( GetDecl( ident ) )
		If Not decl
			If scope Return scope.FindClassDecl( ident,args )
			Return
		Endif
		decl.Semant
		decl.CheckAccess
		Return decl.GenClassInstance( args )
	End
	
	Method FindModuleDecl:ModuleDecl( ident$ )
		Local decl:ModuleDecl=ModuleDecl( GetDecl( ident ) )
		If Not decl
			If scope Return scope.FindModuleDecl( ident )
			Return
		Endif
		decl.Semant
		decl.CheckAccess
		Return decl
	End
		
	Method FindFuncDecl:FuncDecl( ident$,argExprs:Expr[],explicit=False )
	
		Local funcs:=FuncDeclList( FindDecl( ident ) )
		If Not funcs Return
	
		For Local func:FuncDecl=Eachin funcs
			func.Semant
		Next
		
		Local match:FuncDecl,isexact,err$

		For Local func:FuncDecl=Eachin funcs

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
				If argExprs[i] t+=argExprs[i].ToString()
			Next
			Err "Unable to find overload for "+ident+"("+t+")."
		Endif
		
		match.CheckAccess
		
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
	
	Method GenInstance:Decl( inst:ClassDecl )
		InternalErr
	End
	
	Method OnSemant()
		PushEnv Self
		For Local stmt:Stmt=Eachin stmts
			stmt.Semant
		Next
		PopEnv
	End
	
End

Const FUNC_METHOD=1
Const FUNC_CTOR=2
Const FUNC_PROPERTY=4

Class FuncDecl Extends BlockDecl

	Field retType:Type
	Field argDecls:ArgDecl[]

	Field overrides:FuncDecl
	Field superCtor:InvokeSuperExpr
	Field index
	
	Method New( ident$,retType:Type,argDecls:ArgDecl[],attrs )
		Self.ident=ident
		Self.retType=retType
		Self.argDecls=argDecls
		Self.attrs=attrs
		
	End
	
	Method ToString$()
		Local t$
		For Local decl:ArgDecl=Eachin argDecls
			If t t+=","
			t+=decl.ToString()
		Next
		Local q$
		If IsCtor()
			q="Method New"
		Else
			If IsMethod() q="Method " Else q="Function "
			q+=ident+":"
			If retType q+=retType.ToString() Else q+="???"
		Endif
		Return q+"("+t+")"
	End
	
	Method IsMethod()
		Return (attrs & FUNC_METHOD)<>0
	End
	
	Method IsCtor()
		Return (attrs & FUNC_CTOR)<>0
	End

	Method IsStatic()
		Return (attrs & (FUNC_METHOD|FUNC_CTOR))=0
	End
	
	Method IsProperty()
		Return (attrs & FUNC_PROPERTY)<>0
	End
	
	Method EqualsType( decl:FuncDecl )
		If argDecls.Length=decl.argDecls.Length
			If retType.EqualsType( decl.retType )
				For Local i=0 Until argDecls.Length
					If Not argDecls[i].ty.EqualsType( decl.argDecls[i].ty ) Return False
				Next
				Return True
			Endif
		Endif
		Return False
	End

	Method GenInstance:Decl( inst:ClassDecl )
		Return InitInstance( New FuncDecl )
	End
	
	Method OnSemant()
	
		'template version?
		If actual<>Self
			actual.Semant
			Local decl:=FuncDecl( actual )

			retType=decl.retType.GenInstance( ClassScope ).Semant()

			argDecls=decl.argDecls[..]
			For Local i=0 Until argDecls.Length
				argDecls[i]=ArgDecl( argDecls[i].GenInstance( ClassScope ) )
				InsertDecl argDecls[i]
				argDecls[i].Semant
			Next

			Return
		Endif
		
		'get cdecl, sclasss
		Local cdecl:=ClassDecl( scope )
		Local sclass:ClassDecl
		If cdecl sclass=ClassDecl( cdecl.superClass )
		
		'semant ret type
		retType=retType.Semant()
		
		'semant args
		For Local arg:ArgDecl=Eachin argDecls
			InsertDecl arg
			arg.Semant
		Next
		
		'prefix call to super ctor if necessary
		If IsCtor() And superCtor=Null 
			If sclass.FindFuncDecl( "new",[] )
'			If _env.ClassScope().superClass.FindFuncDecl( "new",[] )
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
			stmt.errInfo=errInfo	'""
			stmts.AddLast stmt
		Endif

		'check we exactly match an override
		If sclass And Not IsCtor()
			While sclass
				Local found
				For Local decl:=Eachin sclass.FuncDecls
					If decl.ident<>ident Continue
					found=True
					decl.Semant
					If IsMethod()<>decl.IsMethod()
						overrides=Null
						Exit
					Endif
					If EqualsType( decl )
						overrides=FuncDecl( decl.actual )
					Endif
				Next
				If found And Not overrides
					Err "Overriding method does not match any overridden method."
				Endif
				If found Exit
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

	Field superTy:IdentType
	Field args:ClassDecl[]
	
	Field superClass:ClassDecl
	
	Field argindex=-1					'for args
	Field instances:List<ClassDecl>		'for actual (non-arg, non-instance)
	Field instanceof:ClassDecl			'for instances
	
	Field fields:FieldDecl[]			'ALL fields - length=instance size
	Field methods:FuncDecl[]			'ALL methods - length=vtable size
	
	Global nullObjectClass:=New ClassDecl( "{NULL}",Null,[],DECL_ABSTRACT|DECL_EXTERN )
	
	Method New( ident$,superTy:IdentType,args:ClassDecl[],attrs )
		Self.ident=ident
		Self.superTy=superTy
		Self.args=args
		Self.attrs=attrs
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
	
	Method GenInstance:Decl( inst:ClassDecl )
		If Not IsSemanted() InternalErr
		
		If IsTemplateArg()
			For Local arg:ClassDecl=Eachin inst.instanceof.args
				If arg=Self Return inst.args[argindex]
			Next
			InternalErr
		Endif
		
		Local instArgs:ClassDecl[args.Length]

		For Local i=0 Until args.Length
		
			Local arg:ClassDecl=args[i]
			
			If arg.IsTemplateArg()
			
'				Print "Self.ident="+Self.ident+" inst.ident="+inst.ident+
'				" arg.ident="+arg.ident+" arg.argindex="+arg.argindex+", inst.args.Length="+inst.args.Length
				
				instArgs[i]=inst.args[arg.argindex]
			Else
				instArgs[i]=arg
			Endif
	
		Next
		
		Local cdecl:=GenClassInstance( instArgs )
		Return cdecl
	End
	
	Method GenClassInstance:ClassDecl( instArgs:ClassDecl[] )
		If Not IsSemanted InternalErr
		
		If _env.ClassScope()=Self And Not instArgs.Length
			instArgs=args
		Endif
		
		If IsTemplateInst() 
			Return instanceof.GenClassInstance( instArgs )
		Endif
		
		'check number of args
		If args.Length<>instArgs.Length
			Return Null
			'Err "incorrect number of template arguments"
		Endif

		If Not args.Length Return Self

		'can't create instance of arg
		If IsTemplateArg() InternalErr
		
		'check arg types and use self if possible - eg: ref. to Blah<T> inside Blah<T>
		Local equal=True
		For Local i=0 Until args.Length
			If Not instArgs[i].ExtendsClass( args[i] )
				Err "Invalid template argument type."
			Endif
		
			If Not (instArgs[i]=args[i]) equal=False
		Next
		If equal Return Self
		
		'look for existing instance
		If instances
			For Local decl:ClassDecl=Eachin instances
				Local equal=True
				For Local i=0 Until args.Length
					If Not (instArgs[i]=decl.args[i])
						equal=False
						Exit
					Endif
				Next
				If equal Return decl
			Next
		Else
			instances=New List<ClassDecl>
		Endif
		
		'have to create new instance!
		Local inst:ClassDecl=New ClassDecl
		InitInstance inst
		inst.scope=scope
		'
		inst.superTy=superTy
		inst.args=instArgs
		inst.attrs|=CLASS_TEMPLATEINST|DECL_SEMANTED
		inst.superClass=ClassDecl( superClass.GenInstance( inst ) )
		inst.instanceof=Self
		
		For Local i=0 Until args.Length
			inst.InsertAlias args[i].ident,instArgs[i]
		Next
		
		For Local decl:Decl=Eachin decls
			If Not ClassDecl( decl ) inst.InsertDecl decl.GenInstance( inst )
		Next
		
		instances.AddLast inst

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
	
	Method GetDecl:Object( ident$ )
		Local cdecl:ClassDecl=Self
		While cdecl
			Local decl:Object=cdecl.GetDecl2( ident )
			If decl Return decl
			cdecl=cdecl.superClass
		Wend
	End

	'needs this 'coz you can't go blah.Super.GetDecl()...
	Method GetDecl2:Object( ident$ )
		Return Super.GetDecl( ident )
	End
	
	Method EqualsClass( cdecl:ClassDecl )
		Local tdecl:=Self
		If tdecl.IsTemplateArg() tdecl=tdecl.superClass
		If cdecl.IsTemplateArg() cdecl=cdecl.superClass
		Return tdecl=cdecl
	End
	
	Method ExtendsClass( cdecl:ClassDecl )
		If Self=nullObjectClass Return True
		
		If cdecl.IsTemplateArg()
			cdecl=cdecl.superClass
			If Not cdecl Return True
		Endif
		
		Local tdecl:=Self
		While tdecl
			If tdecl=cdecl Return True
			tdecl=tdecl.superClass
		Wend
		
		Return False
	End
	
	Method OnSemant()
	
		'Semant arg classes
		For Local i=0 Until args.Length
			Local arg:ClassDecl=args[i]
			arg.argindex=i
			arg.superClass=arg.superTy.FindClass()
			arg.actual=arg.superClass.actual
			arg.attrs|=DECL_SEMANTED
			InsertDecl arg
		Next
		
		'Semant superclass		
		If superTy
			PushEnv Self
			superClass=superTy.FindClass()
			PopEnv
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
		
		If Not IsExtern()
		
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
	
		If Not superClass Return
		
		'Print "Update Live: "+ToString()
		
		Local n
		For Local decl:=Eachin FuncDecls
			If Not decl.IsMethod() Continue
			If decl.IsSemanted() Continue
			
			Local live
			Local unsem:=New List<FuncDecl>
			unsem.AddLast decl
			
			Local sclass:=ClassDecl( superClass.actual )

			While sclass
				For Local decl2:=Eachin sclass.FuncDecls
					If Not decl2.IsMethod() Continue
					If decl.ident<>decl2.ident Continue

					If decl2.IsSemanted()
						live=True
					Else
						unsem.AddLast decl2
						If decl2.IsExtern() live=True
					Endif
				Next
				sclass=sclass.superClass
			Wend
			
			If Not live Continue
			
			For Local decl:=Eachin unsem
				decl.Semant
				n+=1
			Next
		Next
		Return n
	End
	
	'Check for duplicate method decls
	Method CheckDuplicateDecls()

		For Local decl:=Eachin FuncDecls
			If Not decl.IsSemanted Continue
			
			For Local decl2:=Eachin FuncDecls
				If decl=decl2 Continue
				If decl.ident<>decl2.ident Continue
				If decl.argDecls.Length<>decl2.argDecls.Length Continue
				
				If Not decl2.IsSemanted InternalErr

				Local match=True
				For Local i=0 Until decl.argDecls.Length
					If decl.argDecls[i].ty.EqualsType( decl2.argDecls[i].ty ) Continue
					match=False
					Exit
				Next

				If match
					PushErr decl2.errInfo
					Err "Duplicate declaration: "+decl2.ToString()+"."
				Endif
			Next
		Next
	End
	
	Method UpdateAttrs()
	
		PushErr errInfo
		
		Local nfields,nmethods
		
		For Local decl:Decl=Eachin decls
			If FieldDecl( decl )
				nfields+=1
			Else If FuncDecl( decl ) And FuncDecl( decl ).IsMethod() 
				nmethods+=1
			Endif
		Next
		
		If Not superClass
		
			Local j=0
			methods=New FuncDecl[nmethods]
			For Local decl:FuncDecl=Eachin FuncDecls
				If Not decl.IsMethod() Continue
				decl.index=j
				methods[j]=decl
				j+=1
			Next

			Local i=0
			fields=New FieldDecl[nfields]
			For Local decl:FieldDecl=Eachin FieldDecls
				decl.index=i
				fields[i]=decl
				i+=1
			Next
			
		Else
		
			Local sclass:=superClass
			
			superClass=ClassDecl( superClass.actual )
			
			'Print "Update attrs: "+ToString()+", Super="+sclass.ToString()+", Actual="+superClass.ToString()

			Local j=superClass.methods.Length

			methods=superClass.methods.Resize( j+nmethods )
			
			For Local decl:FuncDecl=Eachin FuncDecls
				If Not decl.IsMethod() Continue
				If Not decl.IsSemanted() Continue
				
				If decl.overrides
					decl.index=decl.overrides.index
					If decl.overrides.munged decl.munged=decl.overrides.munged
					methods[decl.index]=decl
					Continue
				Endif
				
				#rem
				decl.index=-1
				Local match
				For Local decl2:FuncDecl=Eachin superClass.methods
					If Not decl2.IsMethod() Continue
					If decl.ident<>decl2.ident Continue
					
					match=True
					If decl.EqualsType( decl2 )
						decl.overrides=decl2
						decl.index=decl2.index
						methods[decl.index]=decl
						If decl2.munged decl.munged=decl2.munged
						Exit
					EndIf
				Next
				If decl.index<>-1 Continue
				If match
					PushErr decl.errInfo
					Print ":"
					
					Print decl.ToString()+", candidates:"
					For Local decl2:FuncDecl=Eachin superClass.methods
						If Not decl2.IsMethod() Continue
						If decl.ident<>decl2.ident Continue
						Print decl2.ToString()
					Next
					Err "Overriding method does not match any overridden method."
				Endif
				#end
				
				decl.index=j
				methods[j]=decl
				j+=1
			Next
			
			methods=methods.Resize( j )
			
			'Do this second because semanting methods above may have activated more fields
			'		
			Local i=superClass.fields.Length
			fields=superClass.fields.Resize( i+nfields )
			'
			For Local decl:FieldDecl=Eachin FieldDecls
				decl.index=i
				fields[i]=decl
				i+=1
			Next
			
		Endif
		
		If attrs & CLASS_INSTANCED
			For Local decl:=Eachin methods
				If decl.IsAbstract()
					Err "Cannot create instance of class "+ToString()+" due to abstract method "+decl.ToString()+"."
				Endif
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
	
	Method GenInstance:Decl( inst:ClassDecl )
		InternalErr
	End
	
	'Ok, this gets a bit fun!
	Method GetDecl:Object( ident$ )
		Local todo:=New List<ModuleDecl>
		Local done:=New StringMap<ModuleDecl>
		
		todo.AddLast Self
		done.Insert filepath,Self
		
		Local decl:Object,declmod$
		
		While Not todo.IsEmpty()
	
			Local mdecl:ModuleDecl=todo.RemoveLast()
			Local decl2:Object=mdecl.GetDecl2( ident )
			If decl2 And decl2<>decl
				If mdecl=Self Return decl2
				If decl
					Err "Duplicate identifier '"+ident+"' found in module '"+declmod+"' and module '"+mdecl.ident+"'."
				Endif
				decl=decl2
				declmod=mdecl.ident
			Endif
			
			Local imps:=mdecl.imported
			If mdecl<>Self imps=mdecl.pubImported

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
	
	Field fileImports:=New StringList
	
	Field mainModule:ModuleDecl							'main module
	Field mainFunc:FuncDecl
		
	Field semantedClasses:=New List<ClassDecl>			'list of semanted classes
	Field semantedGlobals:=New List<GlobalDecl>			'in-order list of semanted globals

	Field transCode$									'translated code!
	
	Method InsertModule( mdecl:ModuleDecl )
		mdecl.scope=Self
		imported.Insert mdecl.filepath,mdecl
		If Not mainModule
'			mdecl.munged="bb_"
			mainModule=mdecl
		Endif
	End
	
	Method AddTransCode( tcode$ )
		If transCode.Contains( "${CODE}" )
			transCode=transCode.Replace( "${CODE}",tcode )
		Else
			transCode+=tcode
		Endif
	End
	
	Method GenInstance:Decl( inst:ClassDecl )
		InternalErr
	End
	
	Method OnSemant()
		
		mainFunc=mainModule.FindFuncDecl( "Main",[] )
		If Not mainFunc Err "Function 'Main' not found."
		
		If Not IntType( mainFunc.retType ) Or mainFunc.argDecls.Length
			Err "Main function must be of type Main:Int()"
		Endif

		Repeat
			Local n
			For Local cdecl:=Eachin semantedClasses
				n+=cdecl.UpdateLiveMethods()
			Next
			If Not n Exit
		Forever
		
		For Local cdecl:=Eachin semantedClasses
			cdecl.CheckDuplicateDecls
		Next

		For Local cdecl:=Eachin semantedClasses
			cdecl.UpdateAttrs
		Next
	End
	
End
