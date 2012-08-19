
Import target

Class IosTarget Extends Target
	Field dotAppFile$

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

	Method ReplaceIosScreenOrientation()
		Local plist$=LoadString ( "MonkeyGame-Info.plist" )
		Local orientation$=Env.Get("IOS_SCREEN_ORIENTATION")

		If plist.Find("IOS_SCREEN_ORIENTATION_") = -1 Then Return

		If orientation = "portrait"
			plist=ReplaceBlock( plist,"IOS_SCREEN_ORIENTATION_LANDSCAPE","","~n<!--" )
		ElseIf orientation = "landscape"
			plist=ReplaceBlock( plist,"IOS_SCREEN_ORIENTATION_PORTRAIT","","~n<!--" )
		ElseIf orientation = "all"
			' Return early to skip the useless SaveString
			Return
		Else
			Die "Invalid IOS_SCREEN_ORIENTATION specified. Use one of: portrait, landscape, all"
		End

		SaveString plist,"MonkeyGame-Info.plist"
	End

	Method HandleInAppBillingConfig()
		Local flag$=Env.Get("IN_APP_BILLING")

		If flag="true" Then Return

		Local plist$=LoadString ( "MonkeyGame.xcodeproj/project.pbxproj" )
		plist=ReplaceBlock( plist,"IN_APP_BILLING_1","","~n/* " )
		plist=ReplaceBlock( plist,"IN_APP_BILLING_2","","~n/* " )
		plist=ReplaceBlock( plist,"IN_APP_BILLING_3","","~n/* " )
		plist=ReplaceBlock( plist,"IN_APP_BILLING_4","","~n/* " )
		SaveString plist, "MonkeyGame.xcodeproj/project.pbxproj"
	End

	Method UpdateBundleId()
		Local bundle_label$=Env.Get( "IOS_BUNDLE_LABEL" )
		If bundle_label = "" Then bundle_label = "${PRODUCT_NAME}"

		Local bundle_id$=Env.Get( "IOS_BUNDLE_ID" ).ToLower()
		If bundle_id = "" Then bundle_id = "com.yourcompany.MonkeyGame"

		Local bundle_id_parts$[]=bundle_id.Split(".")
		If bundle_id_parts.Length() < 3
			Print "Error:   IOS_BUNDLE_ID contains less then three segments."
			Print "Ignored: Both IOS_BUNDLE_ID and IOS_BUNDLE_LABLE"
			Return
		End

		Local bundle_id_prefix$=".".Join(bundle_id_parts[..2])
		Local bundle_id_last_part$=".".Join(bundle_id_parts[2..])
		dotAppFile=bundle_id_last_part+".app"

		Local files$[]=["MonkeyGame.xcodeproj/project.pbxproj", "MonkeyGame-Info.plist"]
		For Local file$ = EachIn files
			Local str$=LoadString ( file )
			str=str.Replace( "${IOS_BUNDLE_ID_PREFIX}",bundle_id_prefix )
			str=str.Replace( "${IOS_BUNDLE_ID_LAST_PART}",bundle_id_last_part )
			str=str.Replace( "${IOS_BUNDLE_LABEL}",bundle_label )
			SaveString str, file
		End
	End
	
	Method MakeTarget()
	
		CreateDataDir "data"

		Local main$=LoadString( "main.mm" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"main.mm"

		HandleInAppBillingConfig()
		ReplaceIosScreenOrientation()
		UpdateBundleId()
		
		If OPT_ACTION>=ACTION_BUILD

			Execute "xcodebuild -configuration "+CASED_CONFIG+" -sdk iphonesimulator"

			If OPT_ACTION>=ACTION_RUN
			
				Local home$=GetEnv( "HOME" )

				'Woah, freaky, got this from: http://www.somacon.com/p113.php
				Local uuid$="00C69C9A-C9DE-11DF-B3BE-5540E0D72085"
				
				Local src$="build/"+CASED_CONFIG+"-iphonesimulator/"+dotAppFile
				
				Const p1:="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/Applications/iPhone Simulator.app"
				Const p2:="/Developer/Platforms/iPhoneSimulator.platform/Developer/Applications/iPhone Simulator.app"
				
				'New XCode in /Applications?
				If FileType( p1 )=FILETYPE_DIR
				
					Local dst:=home+"/Library/Application Support/iPhone Simulator/5.1"
					CreateDir dst
					dst+="/Applications"
					CreateDir dst
					dst+="/"+uuid
					If Not DeleteDir( dst,True ) Die "Failed to delete dir:"+dst
					If Not CreateDir( dst ) Die "Failed to create dir:"+dst
					
					'Need to use this 'coz it does the permissions thang
					'
					Execute "cp -r ~q"+src+"~q ~q"+dst+"/"+dotAppFile+"~q"
	
					're-start emulator
					'
					Execute "killall ~qiPhone Simulator~q 2>/dev/null",False
					Execute "open ~q"+p1+"~q"
				
				'Old XCode in /Developer?
				Else If FileType( p2 )=FILETYPE_DIR
				
					Local dst:=home+"/Library/Application Support/iPhone Simulator/4.3.2"
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
					
					'Need to use this 'coz it does the permissions thang
					'
					Execute "cp -r ~q"+src+"~q ~q"+dst+"/"+dotAppFile+"~q"
	
					're-start emulator
					'
					Execute "killall ~qiPhone Simulator~q 2>/dev/null",False
					Execute "open ~q"+p2+"~q"
				
				Endif
			Endif
		Endif
	End
End

