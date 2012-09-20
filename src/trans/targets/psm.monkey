
Import target

Class PsmTarget Extends Target

	Function IsValid()
		If FileType( "psm" )<>FILETYPE_DIR Return False
		Select HostOS
		Case "winnt"
			Return PSM_PATH And FileType( PSM_PATH+"/tools/PsmStudio/bin/mdtool.exe" )=FILETYPE_FILE
		End
	End

	Method Begin()
		ENV_TARGET="psm"
		ENV_LANG="cs"
		_trans=New CsTranslator
	End

	Method Config$()
		Local config:=New StringStack
		For Local kv:=Eachin _cfgVars
			config.Push "public const String "+kv.Key+"="+Enquote( kv.Value,"cs" )+";"
		Next
		Return config.Join( "~n" )
	End
	
	Method Content$()
		Local cont:=New StringStack

		For Local kv:=Eachin dataFiles
		
			Local p:=kv.Key
			Local r:=kv.Value
			Local f:=StripDir( r )
			Local t:=("data/"+r).Replace( "/","\" )
			
			Local ext:=ExtractExt( r ).ToLower()
			
			If MatchPath( r,TEXT_FILES )
				Select ext
				Case "txt","xml","json"
				    cont.Push "    <Content Include=~q"+t+"~q>"
					cont.Push "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>"
					cont.Push "    </Content>"
				Default
					Die "Invalid text file type"
				End
			Else If MatchPath( r,IMAGE_FILES )
				Select ext
				Case "png","jpg","bmp","gif"
				    cont.Push "    <Content Include=~q"+t+"~q>"
					cont.Push "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>"
					cont.Push "    </Content>"
				Default
					Die "Invalid image file type"
				End
			Else If MatchPath( r,SOUND_FILES )
				Select ext
				Case "wav"
				    cont.Push "    <Content Include=~q"+t+"~q>"
					cont.Push "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>"
					cont.Push "    </Content>"
				Default
					Die "Invalid sound file type"
				End
			Else If MatchPath( r,MUSIC_FILES )
				Select ext
				Case "mp3"
				    cont.Push "    <Content Include=~q"+t+"~q>"
					cont.Push "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>"
					cont.Push "    </Content>"
				Default
					Die "Invalid music file type"
				End
			Else If MatchPath( r,BINARY_FILES )
				    cont.Push "    <Content Include=~q"+t+"~q>"
					cont.Push "      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>"
					cont.Push "    </Content>"
			Endif

		Next
		
		Return cont.Join( "~n" )
	
	End
	
	Method MakeTarget()
	
		CreateDataDir "data"
		
		Local proj:=LoadString( "MonkeyGame.csproj" )
		proj=ReplaceBlock( proj,"CONTENT",Content(),"~n<!-- " )
		SaveString proj,"MonkeyGame.csproj"

		Local main:=LoadString( "MonkeyGame.cs" )
		main=ReplaceBlock( main,"TRANSCODE",transCode )
		main=ReplaceBlock( main,"CONFIG",Config() )
		SaveString main,"MonkeyGame.cs"
	
		If OPT_ACTION>=ACTION_BUILD
		
			If OPT_ACTION>=ACTION_RUN
			
				Execute "~q"+PSM_PATH+"/tools/PsmStudio/bin/mdtool~q psm windows run-project MonkeyGame.sln"
		
			Else	'just build...
			
			Endif
		End
	
	End

End
