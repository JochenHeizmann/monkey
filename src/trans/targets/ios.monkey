
Import target

Class IosTarget Extends Target

	Function IsValid()
		If FileType( "ios" )<>FILETYPE_DIR Return False
		Select HostOS
		Case "macos"
			Return True
		End
	End

	Method Begin()
		ENV_TARGET="ios"
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
	
		CreateDataDir "data"

		Local main$=LoadString( "main.mm" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"main.mm"
		
		If OPT_ACTION>=ACTION_BUILD

			Execute "xcodebuild -configuration "+CASED_CONFIG+" -sdk iphonesimulator"

			If OPT_ACTION>=ACTION_RUN
			
				Local home$=GetEnv( "HOME" )

				'Woah, freaky, got this from: http://www.somacon.com/p113.php
				Local uuid$="00C69C9A-C9DE-11DF-B3BE-5540E0D72085"
				
				Local src$="build/"+CASED_CONFIG+"-iphonesimulator/MonkeyGame.app"

				'fixme!				
				Local dst$=home+"/Library/Application Support/iPhone Simulator/4.3.2"
				If FileType( dst )=FILETYPE_NONE
					dst=home+"/Library/Application Support/iPhone Simulator/4.3"
					If FileType( dst )=FILETYPE_NONE
						dst=home+"/Library/Application Support/iPhone Simulator/4.2"
					Endif
				Endif
				
				CreateDir dst

				dst+="/Applications"

				CreateDir dst

				dst+="/"+uuid

				If Not DeleteDir( dst,True ) Die "Failed to delete dir:"+dst
				
				If Not CreateDir( dst ) Die "Failed to create dir:"+dst
				
				'Need to use this 'coz it does the permissions thang - fix CopyDir!
				'
				Execute "cp -r ~q"+src+"~q ~q"+dst+"/MonkeyGame.app~q"

				're-start emulator
				'
				Execute "killall ~qiPhone Simulator~q",False
				Execute "open ~q/Developer/Platforms/iPhoneSimulator.platform/Developer/Applications/iPhone Simulator.app~q"
			Endif
		Endif
	End
End

