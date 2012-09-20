
Import target

Import "makemeta.cpp"

Extern

Global info_width
Global info_height

Function get_info_png( path$ )
Function get_info_jpg( path$ )
Function get_info_gif( path$ )

Public

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
	
	Method Config$()
		Local config:=New StringStack
		For Local kv:=Eachin _cfgVars
			config.Push "CFG_"+kv.Key+"="+Enquote( kv.Value,"js" )+";"
		Next
		Return config.Join( "~n" )
	End
	
	Method MetaData$()
		Local meta:=New StringStack
		For Local kv:=Eachin dataFiles
			Local src:=kv.Key
			Local ext:=ExtractExt( src ).ToLower()
			Select ext
			Case "png","jpg","gif"
				info_width=0
				info_height=0
				Select ext
				Case "png" get_info_png( src )
				Case "jpg" get_info_jpg( src )
				Case "gif" get_info_gif( src )
				End
				If info_width=0 Or info_height=0 Die "Unable to load image file '"+src+"'."
				meta.Push "["+kv.Value+"];type=image/"+ext+";"
				meta.Push "width="+info_width+";"
				meta.Push "height="+info_height+";"
				meta.Push "\n"
			End
		Next
		Return meta.Join("")
	End
	
	Method MakeTarget()
	
		'app data
		CreateDataDir "data"

		Local meta$="var META_DATA=~q"+MetaData()+"~q;~n"
		
		'app code
		Local main$=LoadString( "main.js" )
		
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"METADATA",meta )
		main=ReplaceBlock( main,"CONFIG",Config() )
		
		SaveString main,"main.js"
		
		If OPT_ACTION>=ACTION_RUN
			Local p$=RealPath( "MonkeyGame.html" )
			
			Local t$=HTML_PLAYER+" ~q"+p+"~q"

			Execute t,False
			
		Endif

	End
	
End
