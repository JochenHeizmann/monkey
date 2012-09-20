
Import target

Class GlfwTarget Extends Target

	Function IsValid()
		If FileType( "glfw" )<>FILETYPE_DIR Return False
		Select HostOS
		Case "winnt"
			If MINGW_PATH Return True
			If MSBUILD_PATH Return True
		Case "macos"
			Return True
		End
	End
	
	Method Begin()
		ENV_TARGET="glfw"
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
	
	Method MakeMingw()

		CreateDir "mingw/"+CASED_CONFIG

		CreateDataDir "mingw/"+CASED_CONFIG+"/data"
		
		Local main$=LoadString( "main.cpp" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"main.cpp"
		
		If OPT_ACTION>=ACTION_BUILD

			ChangeDir "mingw"

			Local ccopts:=""
			Select ENV_CONFIG
			Case "release"
				ccopts="-O3 -DNDEBUG"
			End
			
			Execute "mingw32-make CCOPTS=~q"+ccopts+"~q OUT=~q"+CASED_CONFIG+"/MonkeyGame~q"
			
			If OPT_ACTION>=ACTION_RUN
				ChangeDir CASED_CONFIG
				Execute "MonkeyGame"
			Endif
			
		Endif
	End
	
	Method MakeVc2010()
		CreateDir "vc2010/"+CASED_CONFIG
		
		CreateDataDir "vc2010/"+CASED_CONFIG+"/data"
		
		Local main$=LoadString( "main.cpp" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"main.cpp"
		
		If OPT_ACTION>=ACTION_BUILD

			ChangeDir "vc2010"
			
			Execute MSBUILD_PATH+" /p:Configuration="+CASED_CONFIG+";Platform=~qwin32~q MonkeyGame.sln"
			
			If OPT_ACTION>=ACTION_RUN
				ChangeDir CASED_CONFIG
				Execute "MonkeyGame"
			Endif
			
		Endif
	End
	
	Method MakeXcode()
		CreateDataDir "xcode/data"

		Local main$=LoadString( "main.cpp" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"main.cpp"
		
		If OPT_ACTION>=ACTION_BUILD

			ChangeDir "xcode"
			
			Execute "xcodebuild -configuration "+CASED_CONFIG
			
			If OPT_ACTION>=ACTION_RUN
				ChangeDir "build/"+CASED_CONFIG
				ChangeDir "MonkeyGame.app/Contents/MacOS"
				Execute "./MonkeyGame"
			Endif
			
		Endif
	End
	
	Method MakeTarget()
		Select ENV_HOST
		Case "winnt"
			If MINGW_PATH And MSBUILD_PATH
				If GetCfgVar( "GLFW_USE_MINGW" )="1"
					MakeMingw
				Else
					MakeVc2010
				Endif
			Else If MINGW_PATH
				MakeMingw
			Else If MSBUILD_PATH
				MakeVc2010
			Endif
		Case "macos"
			MakeXcode
		End
	End
End
