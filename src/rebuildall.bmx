
Strict

Global QuickTrans

?Win32

'GUICKTRANS=True

Local cp$="copy /Y"
Local rm$="del"
Local trans$="..\bin\trans_winnt.exe"
Local makemeta$="..\bin\makemeta_winnt.exe"
Local newtrans$="trans\trans.build\stdcpp\main_winnt.exe"

?MacOS

QUICKTRANS=True

Local cp$="cp"
Local rm$="rm"
Local trans$="../bin/trans_macos"
Local makemeta$="../bin/makemeta_macos"
Local newtrans$="trans/trans.build/stdcpp/main_macos"

?Linux

QUICKTRANS=True

Local cp$="cp"
Local rm$="rm"
Local trans$="../bni/trans_linux"
Local makemeta$="../bin/makemeta_linux"
Local newtrans$="trans/trans.build/stdcpp/main_linux"

?

Function system( cmd$,fail=True )
	If system_( cmd ) 
		If fail
			Print "system failed for: "+cmd
			End
		EndIf
	EndIf
End Function

'trans available?
If FileType( "trans/trans.monkey" )=FILETYPE_FILE

	If QUICKTRANS
?Win32
		system "g++ -o "+trans+" trans\trans.build\stdcpp\main.cpp"
?Macos
		system "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -o "+trans+" trans/trans.build/stdcpp/main.cpp"
?Linux
		system "g++ -o "+trans+" trans/trans.build/stdcpp/main.cpp"
?
	Else
	
		system trans+" -clean -target=stdcpp trans/trans.monkey"
		

		CopyFile newtrans,trans
		If FileType( trans )<>FILETYPE_FILE 
			Print "cp failed"
			End
		EndIf
		
	EndIf
	Print "trans built OK!"
EndIf

'trans available?
Rem
If FileType( "trans/makemeta.bmx" )=FILETYPFILE
	system "~q"+BlitzMaxPath()+"/bin/bmk~q makeapp -t console -a -r -o "+makemeta+" trans/makemeta.bmx"
	Print "makemeta built OK!"
EndIf
End Rem

'monk available?
Rem
If FileType( "monk/monk.bmx" )=FILETYPE_FILE
	system "~q"+BlitzMaxPath()+"/bin/bmk~q makeapp -t gui -a -r -o ../monk monk/monk.bmx"
	Print "monk built OK!"
EndIf
End Rem
