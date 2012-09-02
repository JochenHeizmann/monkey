
Import target

Class StdcppTarget Extends Target

	Function IsValid()
		If FileType( "stdcpp" )<>FILETYPE_DIR Return False
		Select HostOS
		Case "winnt"
			If MINGW_PATH Return True
		Case "macos"
			Return True
		Case "linux"
			Return True
		End
	End

	Method Begin()
		ENV_TARGET="stdcpp"
		ENV_LANG="cpp"
		_trans=New CppTranslator
	End
	
	Method Config$()
		Local config:=New StringStack
		For Local kv:=Eachin Env
			config.Push "#define CFG_"+kv.Key+" "+kv.Value
		Next
		Return config.Join( "~n" )
	End
	
	Method MakeTarget()
	
		Select ENV_CONFIG
		Case "debug" Env.Set "DEBUG","1"
		Case "release" Env.Set "RELEASE","1"
		Case "profile" Env.Set "PROFILE","1"
		End
		
		Local main$=LoadString( "main.cpp" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )

		SaveString main,"main.cpp"

		If OPT_ACTION>=ACTION_BUILD

			Local out$="main_"+HostOS
			DeleteFile out
			
			Select ENV_HOST
			Case "macos"
				Select ENV_CONFIG
				Case "release"
					Execute "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -O3 -o "+out+" main.cpp"
				Case "debug"
					Execute "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -o "+out+" main.cpp"
				End
			Default
				Select ENV_CONFIG
				Case "release"
					Execute "g++ -O3 -DNDEBUG -o "+out+" main.cpp"
				Case "profile"
					Execute "g++ -O3 -DNDEBUG -o "+out+" main.cpp -lwinmm"
				Case "debug"
					Execute "g++ -o "+out+" main.cpp"
				End
			End

			If OPT_ACTION>=ACTION_RUN
				Execute "~q"+RealPath( out )+"~q"
			Endif
		Endif
	End
	
End

