
Import target

Class Html5Target Extends Target

	Function IsValid()
		Return True
	End

	Method Begin()
		ENV_TARGET="html5"
		ENV_LANG="js"
		_trans=New JsTranslator
	End
	
	Method MakeTarget()
	
		'app data
		CreateDataDir "data"

		Local meta$="var META_DATA=~q"+metaData+"~q;~n"
		
		'app code
		Local main$=LoadString( "main.js" )
		
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",app.transCode )
		main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
		main=ReplaceBlock( main,"${METADATA_BEGIN}","${METADATA_END}",meta )
		
		SaveString main,"main.js"
		
		If OPT_RUN
			Execute HTML_PLAYER+" ~q"+RealPath( "MonkeyGame.html" )+"~q",False
		Endif

	End
	
End
