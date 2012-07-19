
Import target

Class GlfwTarget Extends Target

	Function IsValid()
		If FileType( "glfw" )<>FILETYPE_DIR Return False
		Select HostOS
		Case "winnt"
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
		For Local kv:=Eachin Env
			config.Push "#define CFG_"+kv.Key+" "+kv.Value
		Next
		Return config.Join( "~n" )
	End
	
	Method MakeTarget()

		Select ENV_HOST

		Case "winnt"
		
'			Local CASED_CONFIG:="Release"
		
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
		
		Case "macos"
		
'			Local CASED_CONFIG:="Release"
		
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
	End
End
