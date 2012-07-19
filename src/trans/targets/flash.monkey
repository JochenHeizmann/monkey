
Import target

Class FlashTarget Extends Target

	Function IsValid()
		If FLEX_PATH Return true
	End
	
	Method Begin()
		ENV_TARGET="flash"
		ENV_LANG="as"
		_trans=New AsTranslator
	End
	
	Method MakeTarget()
	
		Local embedded=True	'set to false for 'non-embedded' builds'
	
		CreateDataDir "data"
	
		Local assets$
		
		If embedded
			Local stk:=New StringStack
			For Local t$=Eachin LoadDir( "data",True )
				If t.StartsWith( "." ) Continue
				Select ExtractExt( t )
				Case "png","jpg","mp3"
					Local munged$="_"
					For Local t$=EachIn StripExt( t ).Split( "/" )
						For Local i=0 Until t.Length
							If IsAlpha( t[i] ) Or IsDigit( t[i] ) Or t[i]=95 Continue
							Die "Invalid character in flash filename"
						Next
						munged+=t.Length+t
					Next
					stk.Push "[Embed(source=~qdata/"+t+"~q)]"
					stk.Push "public static var "+munged+":Class;"
				End

			Next
			assets=stk.Join( "~n" )
		Endif

		'app code
		Local main$=LoadString( "MonkeyGame.as" )
		
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",app.transCode )
		main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
		main=ReplaceBlock( main,"${ASSETS_BEGIN}","${ASSETS_END}",assets )
		
		SaveString main,"MonkeyGame.as"
		
		If OPT_BUILD
		
			DeleteFile "main.swf"
			Execute "mxmlc -static-link-runtime-shared-libraries=true MonkeyGame.as"
			
			If embedded
				DeleteDir "data",True
			Endif
			
			If OPT_RUN
				Execute FLASH_PLAYER+" ~q"+RealPath( "MonkeyGame.swf" )+"~q",False
			Endif
		Endif
	End
End
