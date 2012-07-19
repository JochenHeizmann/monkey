
' stdcpp app 'trans' - driver program for the Monkey translator.
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans.trans

Const VERSION$="1.01"

'from config file
Global ANDROID_PATH$
Global ANT_PATH$
Global JDK_PATH$
Global FLEX_PATH$
Global MINGW_PATH$
Global MSBUILD_PATH$
Global HTML_PLAYER$
Global FLASH_PLAYER$

'from trans options
Global OPT_CLEAN=False
Global OPT_UPDATE=False
Global OPT_BUILD=False
Global OPT_RUN=False
Global OPT_OUTPUT$
Global CASED_CONFIG$="Debug"

Class Target

	Method Make( path$ )
		Begin
		SetSourcePath path
		Translate
		CreateTargetDir
		Local cd$=CurrentDir
		ChangeDir targetPath
		MakeTarget
		ChangeDir cd
	End
	
Private

	Field srcPath$		'Main .monkey file
	Field dataPath$		'The app .data dir
	Field buildPath$	'The app .build dir
	Field targetPath$	'The app .build/target dir
	
	Field metaData$
	Field textFiles$
	
	Field app:AppDecl
	
	Method Begin() Abstract
	
	Method MakeTarget() Abstract

	Method SetSourcePath( path$ )
		srcPath=path
		dataPath=StripExt( srcPath )+".data"
		buildPath=StripExt( srcPath )+".build"
		targetPath=buildPath+"/"+ENV_TARGET
	End
	
	Method Translate()

		Print "Parsing..."
		app=parser.ParseApp( srcPath )

		Print "Semanting..."
		app.Semant
		
		For Local file$=Eachin app.fileImports
			If ExtractExt( file ).ToLower()=ENV_LANG
				app.AddTransCode LoadString( file )
			Endif
		Next

		Print "Translating..."
		app.AddTransCode _trans.TransApp( app )
		
		Print "Building..."
	End
	
	Method CreateTargetDir()
		If OPT_CLEAN
			DeleteDir targetPath,True
			If FileType( targetPath )<>FILETYPE_NONE Fail "Failed to clean target dir"
		Endif

		If FileType( targetPath )=FILETYPE_NONE
			If FileType( buildPath )=FILETYPE_NONE CreateDir buildPath
			If FileType( buildPath )<>FILETYPE_DIR Fail "Failed to create build dir: "+buildPath
			If Not CopyDir( ExtractDir( AppPath )+"/../targets/"+ENV_TARGET,targetPath ) Fail "Failed to copy target dir"
		Endif

		If FileType( targetPath )<>FILETYPE_DIR Fail "Failed to create target dir: "+targetPath
	End
	
	Method Enquote$( str$ )
		str=str.Replace( "\","\\" )
		str=str.Replace( "~t","\t" )
		str=str.Replace( "~n","\n" )
		str=str.Replace( "~r","\r" )
		str=str.Replace( "~q","\~q" )
		str="~q"+str+"~q"
		If ENV_LANG="cpp" str="L"+str
		Return str
	End

	Method CreateDataDir( dir$ )
		dir=RealPath( dir )
		
		DeleteDir dir,True
		CreateDir dir
		If FileType( dataPath )=FILETYPE_DIR
			CopyDir dataPath,dir,False		'false=don't copy hidden files...
		Endif
		
		For Local file$=Eachin app.fileImports
			Select ExtractExt( file ).ToLower()
			Case "png","jpg","bmp","wav","mp3","ogg"
				CopyFile file,dir+"/"+StripDir( file )
			End Select
		Next
		
		Local mfile$=ExtractDir( AppPath )+"/meta.txt",meta$
		Execute "~q"+ExtractDir( AppPath )+"/makemeta_"+HostOS+"~q ~q"+dir+"~q ~q"+mfile+"~q"
		
		metaData=LoadString( mfile )
		metaData=metaData.Replace( "~n","\n" )
		
		textFiles=""
		
		For Local f$=Eachin LoadDir( dir,True )

			Local p$=dir+"/"+f
			If FileType(p)<>FILETYPE_FILE Continue
			
			Local ext$=ExtractExt(p).ToLower()
			Select ext
			Case "txt","xml","json"
				Local text$=LoadString( p )
				If text.Length>1024
					Local bits:=New StringList
					While text.Length>1024
						bits.AddLast Enquote( text[..1024] )
						text=text[1024..]
					Wend
					bits.AddLast Enquote( text )
					If ENV_LANG="cpp"
						text=bits.Join( "~n" )
					Else
						text=bits.Join( "+~n" )
					Endif
				Else
					text=Enquote( text )
				Endif
				
				If ENV_LANG="java"
					textFiles+="~t~telse if( path.compareTo(~q"+f+"~q)==0 ) return "+text+";~n"
				Else
					textFiles+="~t~telse if( path=="+Enquote(f)+" ) return "+text+";~n"
				Endif

				DeleteFile p
			End
		Next

		textFiles+="~t~treturn "+Enquote( "" )+";~n"
		
	End
	
End

Class Html5Target Extends Target

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
		main=ReplaceBlock( main,"${METADATA_BEGIN}","${METADATA_END}",meta )
		main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",app.transCode )
		SaveString main,"main.js"
		
		If OPT_RUN
			Execute HTML_PLAYER+" ~q"+RealPath( "MonkeyGame.html" )+"~q",False
		Endif

	End
	
End

Class FlashTarget Extends Target

	Method Begin()
		ENV_TARGET="flash"
		ENV_LANG="as"
		_trans=New AsTranslator
	End
	
	Method MakeTarget()
	
		CreateDataDir "data"

		Local meta$="var META_DATA:String=~q"+metaData+"~q;~n"
		
		'app code
		Local main$=LoadString( "MonkeyGame.as" )
		main=ReplaceBlock( main,"${BEGIN_CODE}","${END_CODE}",app.transCode )
		main=ReplaceBlock( main,"${METADATA_BEGIN}","${METADATA_END}",meta )
		main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
		SaveString main,"MonkeyGame.as"
		
		If OPT_BUILD
		
			DeleteFile "main.swf"
			Execute "mxmlc -static-link-runtime-shared-libraries=true MonkeyGame.as"
			
			If OPT_RUN
				Execute FLASH_PLAYER+" ~q"+RealPath( "MonkeyGame.swf" )+"~q",False
			Endif
		Endif
	End
End

Class XnaTarget Extends Target

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
		cont=ReplaceBlock( cont,"${BEGIN_ITEMS}","${END_ITEMS}",t )
		SaveString cont,"MonkeyGame/MonkeyGameContent/MonkeyGameContent.contentproj"

		'app code
		Local main$=LoadString( "MonkeyGame/MonkeyGame/Program.cs" )
		main=ReplaceBlock( main,"${BEGIN_CODE}","${END_CODE}",_trans.PostProcess( app.transCode ) )
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

Class AndroidTarget Extends Target

	Method Begin()
		ENV_TARGET="android"
		ENV_LANG="java"
		_trans=New JavaTranslator
	End

	Method RenameBuild( oldname$,newname$,newlabel$ )
		Local str$
		
		str=LoadString( "res/layout/main.xml" )
		str=str.Replace( "~qcom."+oldname+".","~q"+newname+".")
		str=str.Replace( "~qcom."+oldname+"~q","~qcom."+newname+"~q")
		SaveString str,"res/layout/main.xml"
		
		str=LoadString( "AndroidManifest.xml" )
		str=str.Replace( "~qcom."+oldname+".","~q"+newname+".")
		str=str.Replace( "~qcom."+oldname+"~q","~qcom."+newname+"~q")
		SaveString str,"AndroidManifest.xml"
		
		str=LoadString( "values/strings.xml" )
		'...replace...?
		SaveString str,"values/strings.xml"
		
		CopyDir "src/com/"+oldname,"src/com/"+newname
		DeleteDir "src/com/"+oldname
	End
	
	Method MakeTarget()
	
		Local output$="MonkeyGame"
		
		CreateDataDir "assets/monkey"

		'props file (points ant at android tools)
		Local props$=LoadString( "local.properties.template" )
		props=props.Replace( "${ANDROID_PATH}",ANDROID_PATH.Replace( "\","\\" ) )
		SaveString props,"local.properties"

		'app code
		Local main$=LoadString( "src/com/monkey/"+output+".java" )
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",_trans.PostProcess( app.transCode ) )
		main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
		SaveString main,"src/com/monkey/"+output+".java"
		
		If OPT_BUILD
			Execute "adb kill-server",False
			Execute "adb start-server"
			Execute "ant clean"
			Execute "ant install"
			Execute "adb kill-server"
		Endif
	End
End

Class IosTarget Extends Target

	Method Begin()
		ENV_TARGET="ios"
		ENV_LANG="cpp"
		_trans=New CppTranslator
	End
	
	Method MakeTarget()
	
		CreateDataDir "data"

		Local main$=LoadString( "main.mm" )
		main=ReplaceBlock( main,"${BEGIN_CODE}","${END_CODE}",_trans.PostProcess( app.transCode ) )
		main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
		SaveString main,"main.mm"
		
		If OPT_BUILD

			Execute "xcodebuild -configuration "+CASED_CONFIG+" -sdk iphonesimulator"

			If OPT_RUN

				Local ver$="4.2"	'put output app here…
				
				Local src$="build/"+CASED_CONFIG+"-iphonesimulator/MonkeyGame.app"

				'Woah, freaky, got this from: http://www.somacon.com/p113.php
				'
				Local uuid$="00C69C9A-C9DE-11DF-B3BE-5540E0D72085"
				
				Local home$=GetEnv( "HOME" )
				
				Local dst$=home+"/Library/Application Support/iPhone Simulator/"+ver+"/Applications"
				CreateDir dst
				
				dst+="/"+uuid		
		
				If Not DeleteDir( dst,True ) Err "Failed to delete dir:"+dst
				
				If Not CreateDir( dst ) Err "Failed to create dir:"+dst
				
'				SaveString "(version 1)~n(debug any)~n(allow default)~n",dst+"/MonkeyGame.sb"
				
				'Need to use this 'coz it does the permissions thang - fix CopyDir!
				'
				Execute "cp -r ~q"+src+"~q ~q"+dst+"/MonkeyGame.app~q"

				're-start emulator
				'
				Execute "killall ~qiPhone Simulator~q",False
				Execute "open ~q/Developer/Platforms/iPhoneSimulator.platform/Developer/Applications/iPhone Simulator.app~q"
			Endif
		Endif
	End
End

Class GlfwTarget Extends Target

	Method Begin()
		ENV_TARGET="glfw"
		ENV_LANG="cpp"
		_trans=New CppTranslator
	End
	
	Method MakeTarget()

		Select ENV_HOST
		Case "winnt"
		
			CreateDir "vc2010/"+CASED_CONFIG
			CreateDataDir "vc2010/"+CASED_CONFIG+"/data"
			
			Local main$=LoadString( "main.cpp" )
			main=ReplaceBlock( main,"${BEGIN_CODE}","${END_CODE}",_trans.PostProcess( app.transCode ) )
			main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
			SaveString main,"main.cpp"
			
			If OPT_BUILD

				ChangeDir "vc2010"
				Execute MSBUILD_PATH+" /p:Configuration="+CASED_CONFIG+" MonkeyGame.sln"
				
				If OPT_RUN
					ChangeDir CASED_CONFIG
					Execute "MonkeyGame"
				Endif

			Endif
		
		Case "macos"
		
			CreateDataDir "xcode/data"

			Local main$=LoadString( "main.cpp" )
			main=ReplaceBlock( main,"${BEGIN_CODE}","${END_CODE}",_trans.PostProcess( app.transCode ) )
			main=ReplaceBlock( main,"${TEXTFILES_BEGIN}","${TEXTFILES_END}",textFiles )
			SaveString main,"main.cpp"
			
			If OPT_BUILD

				ChangeDir "xcode"
				Execute "xcodebuild -configuration "+CASED_CONFIG
				
				If OPT_RUN
					ChangeDir "build/"+CASED_CONFIG
					Execute "open MonkeyGame.app"
				Endif
			Endif
			
		End
	End
End

Class StdcppTarget Extends Target

	Method Begin()
		ENV_TARGET="stdcpp"
		ENV_LANG="cpp"
		_trans=New CppTranslator
	End

	Method MakeTarget()
	
		Local main$=LoadString( "main.cpp" )
		main=ReplaceBlock( main,"${BEGIN_CODE}","${END_CODE}",app.transCode )
		SaveString main,"main.cpp"

		If OPT_BUILD

			Local out$="main_"+HostOS
			DeleteFile out
			
			Select ENV_HOST
			Case "macos"
				Execute "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -o "+out+" main.cpp"
			Default
				Execute "g++ -o "+out+" main.cpp"
			End

			If OPT_RUN
				Execute "~q"+RealPath( out )+"~q"
			Endif
		Endif
	End
	
End

Function Fail( msg$ )
	Print msg
	ExitApp -1
End

Function StripQuotes$( rhs$ )
	If rhs.StartsWith( "~q" )
		Local i2=rhs.Find( "~q",1 )
		If i2=-1 Fail "bad string in config file"
		rhs=rhs[1..i2]
	Endif
	Return rhs
End

Function Execute( cmd$,failHard=True )
	Local r=system.Execute( cmd )
	If r
		Print "TRANS Failed to execute '"+cmd+"', return code="+r
		If failHard ExitApp -1
	Endif
	Return True
End

'replace block of lines starting with startTag and ending with endTtag.
'Leaves startTag and endTag lines in place.
Function ReplaceBlock$( text$,startTag$,endTag$,repText$ )

	'Find *first* start tag
	Local i=text.Find( startTag )
	If i=-1
		Fail "Error replacing block, can't find startTag: "+startTag
	Endif
	i+=startTag.Length
	While i<text.Length And text[i-1]<>10
		i+=1
	Wend

	'Find *last* end tag
	Local i2=text.Find( endTag,i )
	If i2=-1 
		Fail "Error replacing block, can't find endTag: "+endTag
	Endif
	Repeat
		Local i3=text.Find( endTag,i2+endTag.Length )
		If i3=-1 Exit
		i2=i3
	Forever
	While i2>0 And text[i2]<>10
		i2-=1
	Wend

	'replace text!
	Return text[..i]+repText+text[i2..]
End

Global CONFIG_FILE$

'external...
Function LoadConfig()

	Local cfgpath$=ExtractDir( AppPath )+"/"
	If CONFIG_FILE
		cfgpath+=CONFIG_FILE
	Else
		cfgpath+="config."+HostOS+".txt"
	Endif
	
	If FileType( cfgpath )<>FILETYPE_FILE Fail "Failed to open config file"

	Local cfg$=LoadString( cfgpath )
	
	For Local line$=Eachin cfg.Split( "~n" )
	
		line=line.Trim()
		If line.StartsWith( "'" ) Continue
		
		Local i=line.Find( "=" )
		If i=-1 Continue
		
		Local lhs$=line[..i],rhs$=line[i+1..]
		
		Repeat
			Local i=rhs.Find( "${" )
			If i=-1 Exit
			Local e=rhs.Find( "}",i+2 )
			If e=-1 Exit
			Local k$=rhs[i+2..e]
			Local v$=GetEnv( k )
			If v
				rhs=rhs.Replace( "${"+k+"}",v )
			Else
				rhs=""
				Exit
			Endif
		Forever
		
		If Not rhs Continue
		
		Local unq$=StripQuotes( rhs )
		Local path$=unq
		
		'James-proof it!
		While path.EndsWith( "/" ) Or path.EndsWith( "\" )
			path=path[..-1]
		Wend
		
		Select lhs
		Case "ANDROID_PATH"
			If Not ANDROID_PATH And FileType( path )=FILETYPE_DIR
				ANDROID_PATH=path
			Endif
		Case "JDK_PATH" 
			If Not JDK_PATH And FileType( path )=FILETYPE_DIR
				JDK_PATH=path
			Endif
		Case "ANT_PATH"
			If Not ANT_PATH And FileType( path )=FILETYPE_DIR
				ANT_PATH=path
			Endif
		Case "FLEX_PATH"
			If Not FLEX_PATH And FileType( path )=FILETYPE_DIR
				FLEX_PATH=path
			Endif
		Case "MINGW_PATH"
			If Not MINGW_PATH And FileType( path )=FILETYPE_DIR
				MINGW_PATH=path
			Endif
		Case "MSBUILD_PATH"
			If Not MSBUILD_PATH And FileType( path )=FILETYPE_FILE
				MSBUILD_PATH=path
			Endif
		Case "HTML_PLAYER" 
			HTML_PLAYER=rhs
		Case "FLASH_PLAYER" 
			FLASH_PLAYER=rhs
		Default 
			Fail "Unrecognized config var: "+lhs
		End

	Next
	
	Select HostOS
	Case "winnt"
		Local path$=GetEnv( "PATH" )
		
		If ANDROID_PATH path+=";"+ANDROID_PATH+"/tools"
		If ANDROID_PATH path+=";"+ANDROID_PATH+"/platform-tools"
		If JDK_PATH path+=";"+JDK_PATH+"/bin"
		If ANT_PATH path+=";"+ANT_PATH+"/bin"
		If FLEX_PATH path+=";"+FLEX_PATH+"/bin"
		If MINGW_PATH path+=";"+MINGW_PATH+"/bin"

		SetEnv "PATH",path
		
		If JDK_PATH SetEnv "JAVA_HOME",JDK_PATH

	Case "macos"
		Local path$=GetEnv( "PATH" )
		
		If ANDROID_PATH path+=":"+ANDROID_PATH+"/tools"
		If ANDROID_PATH path+=":"+ANDROID_PATH+"/platform-tools"
		If FLEX_PATH path+=":"+FLEX_PATH+"/bin"
		
		SetEnv "PATH",path
		
		If Not HTML_PLAYER HTML_PLAYER="open "
		If Not FLASH_PLAYER FLASH_PLAYER="open "
		
	End

	Return True
End

Function ValidTargets$()
	Local valid:=New StringList
	valid.AddLast "html5"
	Select HostOS
	Case "winnt"
		If FLEX_PATH 'And JDK_PATH
			valid.AddLast "flash"
		Endif
		If ANDROID_PATH And JDK_PATH And ANT_PATH
			valid.AddLast "android"
		Endif
		If MSBUILD_PATH
			valid.AddLast "xna"
			valid.AddLast "glfw"
		Endif
		If MINGW_PATH
			valid.AddLast "stdcpp"
		Endif
	Case "macos"
		If FLEX_PATH
			valid.AddLast "flash"
		Endif
		If ANDROID_PATH
			valid.AddLast "android"
		Endif
		valid.AddLast "ios"
		valid.AddLast "glfw"
		valid.AddLast "stdcpp"
	End
	Local v$
	For Local t$=Eachin valid
		If FileType( RealPath( ExtractDir( AppPath )+"/../targets/"+t ) )=FILETYPE_DIR
			If v v+=" "
			v+=t
		Endif
	Next
	Return v
End

Function Main()

	Print "TRANS monkey compiler V"+VERSION
	
	For Local i=1 Until AppArgs.Length
		If AppArgs[i].ToLower().StartsWith( "-cfgfile=" )
			CONFIG_FILE=AppArgs[i][9..]
			Exit
		Endif
	Next

	LoadConfig
	
	If AppArgs.Length<2 Or AppArgs.Length=2 And CONFIG_FILE
		Print "TRANS Usage: trans [-update] [-build] [-run] [-clean] [-config=...] [-target=...] [-cfgfile=...] [-modpath=...] <main_monkey_source_file>"
		Print "Valid targets: "+ValidTargets()
		Print "Valid configs: debug release"
		If AppArgs.Length=1 ExitApp 0
		ExitApp -1
	Endif

	Local srcpath$=StripQuotes( AppArgs[AppArgs.Length-1].Trim() )
	If FileType( srcpath )<>FILETYPE_FILE Fail "Invalid source file"
	srcpath=RealPath( srcpath )
	
	ENV_HOST=HostOS
	ENV_MODPATH=".;"+ExtractDir( srcpath )+";"+RealPath( ExtractDir( AppPath )+"/../modules" )

	Local target:Target

	For Local i=1 Until AppArgs.Length-1

		Local arg$=AppArgs[i].Trim()

		If Not arg.StartsWith( "-" )
			Fail "Command line error"
		Endif
		
		Local j=arg.Find( "=" )

		If j=-1
			Local lhs$=arg[1..]
			Select lhs.ToLower()
			Case "safe"
				ENV_SAFEMODE=True
			Case "clean"
				OPT_CLEAN=True
			Case "update"
				OPT_UPDATE=True
			Case "build"
				OPT_BUILD=True
			Case "run"
				OPT_RUN=True
			Default
				Fail "Command line error"
			End
		Else
			Local lhs$=arg[1..j]
			Local rhs$=arg[j+1..]
			Select lhs.ToLower()
			Case "cfgfile"
			Case "output"
				OPT_OUTPUT=rhs
			Case "config"
				Select rhs.ToLower()
				Case "debug"
					CASED_CONFIG="Debug"
				Case "release"
					CASED_CONFIG="Release"
				Default
					Fail "Unrecognized config: "+rhs
				End
			Case "target"
				Local t$=rhs.ToLower()
				Select t
				Case "html5"
					target=New Html5Target
				Case "flash"
					target=New FlashTarget
				Case "xna"
					target=New XnaTarget
				Case "android"
					target=New AndroidTarget
				Case "ios"
					target=New IosTarget
				Case "glfw"
					target=New GlfwTarget
				Case "stdcpp"
					target=New StdcppTarget
				Default
					Fail "Unrecognizd target: "+t
				End
				If Not (" "+ValidTargets()+" ").Contains( " "+t+" " )
					Fail "SDK for target: "+t+" does not appear to be correctly installed"
				Endif
			Case "modpath"
				ENV_MODPATH=StripQuotes( rhs )
			Default
				Fail "Command line error"
			End
		Endif
	Next
	
	If Not target Fail "No target specified"
	
	ENV_CONFIG=CASED_CONFIG.ToLower()
	
	If OPT_RUN OPT_BUILD=True
	If OPT_BUILD OPT_UPDATE=True
	
	'Default to OPT_BUILD...
	If Not OPT_UPDATE
		OPT_UPDATE=True
		OPT_BUILD=True
	Endif
	
	target.Make srcpath

End
