
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
	
	Method MakeTarget()
	
		Local embedded=True	'set to false for 'non-embedded' builds'
	
		CreateDataDir "data",False
	
		Local assets$
		
		If embedded
			Local stk:=New StringStack
			For Local t$=Eachin LoadDir( "data",True )
				If t.StartsWith( "." ) Continue
				Local ext$=ExtractExt( t )
				Select ext.ToLower()
				Case "png","jpg","mp3","txt","xml","json"
					'
					Local munged$="_"
					For Local q$=Eachin StripExt( t ).Split( "/" )
						For Local i=0 Until q.Length
							If IsAlpha( q[i] ) Or IsDigit( q[i] ) Or q[i]=95 Continue
							Die "Invalid character in flash filename: "+t+"."
						Next
						munged+=q.Length+q
					Next
					munged+=ext.Length+ext
					'
					Select ext.ToLower()
					Case "png","jpg","mp3"
						stk.Push "[Embed(source=~qdata/"+t+"~q)]"
						stk.Push "public static var "+munged+":Class;"
					Case "txt","xml","json"
						stk.Push "[Embed(source=~qdata/"+t+"~q,mimeType=~qapplication/octet-stream~q)]"
						stk.Push "public static var "+munged+":Class;"
					End
				End

			Next
			assets=stk.Join( "~n" )
		Endif

		'app code
		Local main$=LoadString( "MonkeyGame.as" )
		
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",transCode )
		main=ReplaceBlock( main,"${ASSETS_BEGIN}","${ASSETS_END}",assets )
		
		SaveString main,"MonkeyGame.as"
		
		If OPT_ACTION>=ACTION_BUILD
		
			DeleteFile "main.swf"
			Execute "mxmlc -static-link-runtime-shared-libraries=true MonkeyGame.as"
			
			If embedded
				DeleteDir "data",True
			Endif
			
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
