
Import target

Class AndroidTarget Extends Target

	Function IsValid()
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

	Method MakeTarget()
	
		'create data dir
		CreateDataDir "assets/monkey",False

		'load config
		Local tags:=LoadTags( "CONFIG.TXT" )
		Local app_label$=tags.Get( "APP_LABEL" )
		Local app_package$=tags.Get( "APP_PACKAGE" )
		tags.Set "ANDROID_SDK_DIR",ANDROID_PATH.Replace( "\","\\" )
		
		'template files
		For Local file$=Eachin LoadDir( "templates",True )
			Local str$=LoadString( "templates/"+file )
			str=ReplaceTags( str,tags )
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
		main=ReplaceBlock( main,"${PACKAGE_BEGIN}","${PACKAGE_END}","package "+app_package+";" )
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",transCode )
		SaveString main,jpath
		
		If OPT_BUILD
		
			Execute "adb start-server"
			
			'Don't die yet...
			Local r=Execute( "ant clean",False ) And Execute( "ant install",False )
			
			'...always execute this or project dir can remain locked by ADB!
			Execute "adb kill-server",False

			If Not r
				Die "Android build failed."
			Endif
	
		Endif
	End
End
