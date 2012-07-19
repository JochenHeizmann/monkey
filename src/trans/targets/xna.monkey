
Import target

Class XnaTarget Extends Target

	Function IsValid()
		Select HostOS
		Case "winnt"
			If MSBUILD_PATH Return True
		End
	End

	Method Begin()
		ENV_TARGET="xna"
		ENV_LANG="cs"
		_trans=New CsTranslator
	End

	Method MakeTarget()
	
		CreateDataDir "data"
		
		Local t$,dirs:=New StringList
		dirs.AddLast ""
		While Not dirs.IsEmpty()
		
			Local dir$=dirs.RemoveFirst()
			
			For Local file$=Eachin LoadDir( "data/"+dir )
				If file.StartsWith(".") Continue
			
				Local path$=file
				If dir path=dir+"/"+file
				'Print "Processing:"+path
				
				Select FileType( "data/"+path )
				Case FILETYPE_FILE
					Local ps$
					Select ExtractExt( file )
					Case "png","jpg","jpeg"
						ps+="      <Importer>TextureImporter</Importer>~n"
						ps+="      <Processor>TextureProcessor</Processor>~n"
						ps+="      <ProcessorParameters_ColorKeyEnabled>False</ProcessorParameters_ColorKeyEnabled>~n"
						ps+="      <ProcessorParameters_PremultiplyAlpha>False</ProcessorParameters_PremultiplyAlpha>~n"
					Case "wav"
						ps+="      <Importer>WavImporter</Importer>~n"
						ps+="      <Processor>SoundEffectProcessor</Processor>~n"
					Case "mp3"
						ps+="      <Importer>Mp3Importer</Importer>~n"
						ps+="      <Processor>SoundEffectProcessor</Processor>~n"
					End
					If ps
						t+="  <ItemGroup>~r~n"
						t+="    <Compile Include=~q"+path+"~q>~r~n"
						t+="      <Name>"+file+"</Name>~r~n"
						t+=ps
						t+="    </Compile>~r~n"
						t+="  </ItemGroup>~r~n"
						CopyFile "data/"+path,"MonkeyGame/MonkeyGameContent/"+path
					Endif
				Case FILETYPE_DIR
					CreateDir "MonkeyGame/MonkeyGameContent/"+path
					dirs.AddLast path
				End
			Next
		Wend
		
		'app data
		Local cont$=LoadString( "MonkeyGame/MonkeyGameContent/MonkeyGameContent.contentproj" )
		cont=ReplaceBlock( cont,"${CONTENT_BEGIN}","${CONTENT_END}",t )
		SaveString cont,"MonkeyGame/MonkeyGameContent/MonkeyGameContent.contentproj"

		'app code
		Local main$=LoadString( "MonkeyGame/MonkeyGame/Program.cs" )
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",_trans.PostProcess( app.transCode ) )
		main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
		SaveString main,"MonkeyGame/MonkeyGame/Program.cs"
			
		If OPT_BUILD
		
			Execute MSBUILD_PATH+" /p:Configuration="+CASED_CONFIG+" MonkeyGame.sln"
			
			If OPT_RUN
				ChangeDir "MonkeyGame/MonkeyGame/bin/x86/"+CASED_CONFIG
				Execute "MonkeyGame",False
			Endif
		Endif
		
	End

End

