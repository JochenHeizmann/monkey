
Import target

Class FlashTarget Extends Target

	Function IsValid()
		If FileType( "flash" )<>FILETYPE_DIR Return False
		Return FLEX_PATH<>"" And (FLASH_PLAYER<>"" Or HTML_PLAYER<>"")
	End
	
	Method Begin()
		ENV_TARGET="flash"
		ENV_LANG="as"
		_trans=New AsTranslator
	End
	
	Method Config$()
		Local config:=New StringStack
		For Local kv:=Eachin _cfgVars
			config.Push "internal static var "+kv.Key+":String="+Enquote( kv.Value,"as" )
		Next
		Return config.Join( "~n" )
	End
	
	Method Assets$()
		Local assets:=New StringStack
		For Local kv:=Eachin dataFiles
			
			Local ext:=ExtractExt( kv.Value )
			
			Local munged$="_"
			For Local q:=Eachin StripExt( kv.Value ).Split( "/" )
				For Local i=0 Until q.Length
					If IsAlpha( q[i] ) Or IsDigit( q[i] ) Or q[i]=95 Continue
					Die "Invalid character in flash filename: "+kv.Value+"."
				Next
				munged+=q.Length+q
			Next
			munged+=ext.Length+ext
			
			Select ext.ToLower()
			Case "png","jpg","mp3"
				assets.Push "[Embed(source=~qdata/"+kv.Value+"~q)]"
				assets.Push "public static var "+munged+":Class;"
			Default
				assets.Push "[Embed(source=~qdata/"+kv.Value+"~q,mimeType=~qapplication/octet-stream~q)]"
				assets.Push "public static var "+munged+":Class;"
			End
			
		Next
		Return assets.Join( "~n" )
	End
	
	
	Method MakeTarget()

		CreateDataDir "data"
		
		'app code
		Local main$=LoadString( "MonkeyGame.as" )

		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"ASSETS",Assets() )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"MonkeyGame.as"
		
		If OPT_ACTION>=ACTION_BUILD
		
			DeleteFile "main.swf"

			Execute "mxmlc -static-link-runtime-shared-libraries=true MonkeyGame.as"
			
			If OPT_ACTION>=ACTION_RUN
				If FLASH_PLAYER
					Execute FLASH_PLAYER+" ~q"+RealPath( "MonkeyGame.swf" )+"~q",False
				Else If HTML_PLAYER
					Execute HTML_PLAYER+" ~q"+RealPath( "MonkeyGame.html" )+"~q",False
				Endif
			Endif
		Endif
	End
End
