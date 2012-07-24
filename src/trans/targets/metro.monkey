
Import target

Class MetroTarget Extends Target

	Function IsValid()
		If FileType( "metro" )<>FILETYPE_DIR Or HostOS<>"winnt" Or Not MSBUILD_PATH Return False
		Return True
	End
	
	Method Begin()
		ENV_TARGET="metro"
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
	
		Local vcpath:=""

		CreateDir vcpath+CASED_CONFIG
		
		CreateDataDir vcpath+CASED_CONFIG+"/data"
		
		Local main$=LoadString( "main.cpp" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"main.cpp"
		
		If OPT_ACTION>=ACTION_BUILD

			If vcpath ChangeDir vcpath
			
			Execute MSBUILD_PATH+" /p:Configuration="+CASED_CONFIG+";Platform=~qwin32~q MonkeyGame.sln"
			
			If OPT_ACTION>=ACTION_RUN
				ChangeDir CASED_CONFIG
				Execute "MonkeyGame"
			Endif
		Endif

	End
End
