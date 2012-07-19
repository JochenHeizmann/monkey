
Import target

Class Html5Target Extends Target

	Function IsValid()
		If FileType( "html5" )<>FILETYPE_DIR Return False
		Return HTML_PLAYER<>""
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
		
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",transCode )
		main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
		main=ReplaceBlock( main,"${METADATA_BEGIN}","${METADATA_END}",meta )
		
		SaveString main,"main.js"
		
		If OPT_RUN
			Local p$=RealPath( "MonkeyGame.html" )
			
			Local t$=HTML_PLAYER+" ~q"+p+"~q"

			Execute t,False
			
		Endif

	End
	
End
