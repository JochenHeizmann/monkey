
Import target

Class Win8Target Extends Target

	Function IsValid()
		If FileType( "win8" )<>FILETYPE_DIR Or HostOS<>"winnt" Or Not MSBUILD_PATH Return False
		Return True
	End
	
	Method Begin()
		ENV_TARGET="win8"
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
	
		CreateDataDir "MonkeyGame/MonkeyGam/Assets/monkey"

		'app data
'		Local contproj:=LoadString( "MonkeyGame/MonkeyGameContent/MonkeyGameContent.contentproj" )
'		contproj=ReplaceBlock( contproj,"CONTENT",Content(),"~n<!-- " )
'		SaveString contproj,"MonkeyGame/MonkeyGameContent/MonkeyGameContent.contentproj"
		
		'app code
		Local main:=LoadString( "MonkeyGame/MonkeyGame/MonkeyGame.cpp" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"MonkeyGame/MonkeyGame/MonkeyGame.cpp"
			
		If OPT_ACTION>=ACTION_BUILD
		
			Execute MSBUILD_PATH+" /t:MonkeyGame /p:Configuration="+CASED_CONFIG+" /p:Platform=Win32 MonkeyGame\MonkeyGame.sln"
			
'			If OPT_ACTION>=ACTION_RUN
'				ChangeDir "MonkeyGame/MonkeyGame/bin/x86/"+CASED_CONFIG
'				Execute "MonkeyGame",False
'			Endif

		Endif
		
	End
	
End
