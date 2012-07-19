
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
	
	Method MakeTarget()

		Select ENV_HOST
		Case "winnt"
		
			CreateDir "vc2010/"+CASED_CONFIG
			
			CreateDataDir "vc2010/"+CASED_CONFIG+"/data",False
			
			Local main$=LoadString( "main.cpp" )
			main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",transCode )
			SaveString main,"main.cpp"
			
			If OPT_BUILD

				ChangeDir "vc2010"
				Execute MSBUILD_PATH+" /p:Configuration="+CASED_CONFIG+";Platform=~qwin32~q MonkeyGame.sln"
				
				If OPT_RUN
					ChangeDir CASED_CONFIG
					Execute "MonkeyGame"
				Endif
			Endif
		
		Case "macos"
		
			CreateDataDir "xcode/data",False

			Local main$=LoadString( "main.cpp" )
			main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",transCode )
			SaveString main,"main.cpp"
			
			If OPT_BUILD

				ChangeDir "xcode"
				Execute "xcodebuild -configuration "+CASED_CONFIG
				
				If OPT_RUN
					ChangeDir "build/"+CASED_CONFIG
					Execute "open MonkeyGame.app"
				Endif
			Endif
			
		End
	End
End
