
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
		For Local kv:=Eachin _cfgVars
			config.Push "#define CFG_"+kv.Key+" "+kv.Value
		Next
		Return config.Join( "~n" )
	End
	
	Method MakeTarget()
	
		Select ENV_CONFIG
		Case "debug" SetCfgVar "DEBUG","1"
		Case "release" SetCfgVar "RELEASE","1"
		Case "profile" SetCfgVar "PROFILE","1"
		End
		
		Local main$=LoadString( "main.cpp" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )

		SaveString main,"main.cpp"

		If OPT_ACTION>=ACTION_BUILD

			Local out$="main_"+HostOS
			DeleteFile out
			
			Local OPTS:="",LIBS:=""
			
			Select ENV_HOST
			Case "macos"
				OPTS+=" -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3"
			Case "winnt"
				LIBS+=" -lwinmm -lws2_32"
			End
			
			Select ENV_CONFIG
			Case "release"
				OPTS+=" -O3 -DNDEBUG"
			End
			
			Execute "g++"+OPTS+" -o "+out+" main.cpp"+LIBS

			If OPT_ACTION>=ACTION_RUN
				Execute "~q"+RealPath( out )+"~q"
			Endif
		Endif
	End
	
End

