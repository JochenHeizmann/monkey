
Import target

Class AndroidTarget Extends Target

	Function IsValid()
		If FileType( "android" )<>FILETYPE_DIR Return False
		Select HostOS
		Case "winnt"
			If ANDROID_PATH And JDK_PATH And ANT_PATH Return True
		Case "macos"
			If ANDROID_PATH Return True
		End
	End
	
	Method Begin()
		ENV_TARGET="android"
		ENV_LANG="java"
		_trans=New JavaTranslator
	End
	
	Method Config$()
		Local config:=New StringStack
		For Local kv:=Eachin Env
			config.Push "static final String "+kv.Key+"="+LangEnquote( kv.Value )+";"
		Next
		Return config.Join( "~n" )
	End

	Method MakeTarget()
	
		'create data dir
		CreateDataDir "assets/monkey"

		Local app_label$=Env.Get( "ANDROID_APP_LABEL" )
		Local app_package$=Env.Get( "ANDROID_APP_PACKAGE" )
		
		Env.Set "ANDROID_SDK_DIR",ANDROID_PATH.Replace( "\","\\" )
		
		'template files
		For Local file$=Eachin LoadDir( "templates",True )
			Local str$=LoadString( "templates/"+file )
			str=ReplaceEnv( str )
			SaveString str,file
		Next
		
		'create package
		Local jpath$="src"
		DeleteDir jpath,True
		CreateDir jpath
		For Local t$=Eachin app_package.Split(".")
			jpath+="/"+t
			CreateDir jpath
		Next
		jpath+="/MonkeyGame.java"
		
		'create main source file
		Local main$=LoadString( "MonkeyGame.java" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		'extract all imports
		Local imps:=New StringStack
		Local done:=New StringSet
		Local out:=New StringStack
		For Local line:=Eachin main.Split( "~n" )
			If line.StartsWith( "import " )
				Local i:=line.Find( ";",7 )
				If i<>-1
					Local id:=line[7..i+1]
					If Not done.Contains( id )
						done.Insert id
						imps.Push "import "+id
					Endif
				Endif
			Else
				out.Push line
			Endif
		End
		main=out.Join( "~n" )

		main=ReplaceBlock( main,"IMPORTS",imps.Join( "~n" ) )
		main=ReplaceBlock( main,"PACKAGE","package "+app_package+";" )
		
		SaveString main,jpath
		
		If OPT_ACTION>=ACTION_BUILD
		
			Execute "adb start-server"
			
			'Don't die yet...
			Local r=Execute( "ant clean",False ) And Execute( "ant debug install",False )
			
			'...always execute this or project dir can remain locked by ADB!
			Execute "adb kill-server",False

			If Not r
			
				Die "Android build failed."
				
			Else
				If OPT_ACTION>=ACTION_RUN
					Execute "adb shell am start -n "+app_package+"/"+app_package+".MonkeyGame",False
					Execute "adb kill-server",False
				End				
			Endif
	
		Endif
	End
End
