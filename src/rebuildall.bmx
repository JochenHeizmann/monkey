
Strict

'rebuild options
Const Rebuild_Trans=	True
Const Rebuild_Monk=		False
Const Rebuild_Mserver=	False
Const Rebuild_Makemeta=	False

?Win32

Const QUICKTRANS=False

Const trans$="..\bin\trans_winnt.exe"
Const trans2$=trans
'Const trans2$="..\bin\trans_winnt_v49a.exe"
Const newtrans$="trans\trans.build\stdcpp\main_winnt.exe"
Const makemeta$="..\bin\makemeta_winnt.exe"
Const mserver$="..\bin\mserver_winnt.exe"

?MacOS

Const QUICKTRANS=False

Const trans$="../bin/trans_macos"
Const trans2$=trans
'Const trans2$="../bin/trans_macos_v40"
Const newtrans$="trans/trans.build/stdcpp/main_macos"
Const makemeta$="../bin/makemeta_macos"
Const mserver$="../bin/mserver_macos"

?Linux

Const QUICKTRANS=True

Const trans$="../bni/trans_linux"
'Const trans2$="../bin/trans_linux_v38"
Const newtrans$="trans/trans.build/stdcpp/main_linux"
Const makemeta$="../bin/makemeta_linux"
Const mserver$="../bin/mserver_linux"

?

Function system( cmd$,fail=True )
	If system_( cmd ) 
		If fail
			Print "system failed for: "+cmd
			End
		EndIf
	EndIf
End Function

'Rebuild functions....
If Rebuild_Trans And FileType( "trans/trans.monkey" )=FILETYPE_FILE

	If QUICKTRANS
?Win32
		system "g++ -o "+trans+" trans\trans.build\stdcpp\main.cpp"
?Macos
		system "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -o "+trans+" trans/trans.build/stdcpp/main.cpp"
?Linux
		system "g++ -o "+trans+" trans/trans.build/stdcpp/main.cpp"
?
	Else
	
		system trans2+" -clean -target=stdcpp -config=release trans/trans.monkey"

		Delay 100
		
		DeleteFile trans
		If FileType( trans )
			Print "***** ERROR ***** Failed to delete trans"
			End
		EndIf
		
		CopyFile newtrans,trans
		If FileType( trans )<>FILETYPE_FILE 
			Print "***** ERROR ***** Failed to copy trans"
			End
		EndIf
?Not win32
		system "chmod +x "+trans
?
	EndIf
	Print "trans built OK!"
EndIf

If Rebuild_Monk And FileType( "monk/monk.bmx" )=FILETYPE_FILE
	system "~q"+BlitzMaxPath()+"/bin/bmk~q makeapp -t gui -a -r -o ../monk monk/monk.bmx"
?MacOS
	system "cp monk/info.plist ../monk.app/Contents"
	system "cp monk/monk.icns ../monk.app/Contents/Resources"
?
	Print "monk built OK!"
EndIf

If Rebuild_Makemeta And FileType( "trans/makemeta.bmx" )=FILETYPE_FILE
	system "~q"+BlitzMaxPath()+"/bin/bmk~q makeapp -t console -a -r -o "+makemeta+" trans/makemeta.bmx"
	Print "makemeta built OK!"
EndIf

If Rebuild_MServer And FileType( "mserver/mserver.bmx" )=FILETYPE_FILE
	system "~q"+BlitzMaxPath()+"/bin/bmk~q makeapp -h -t gui -a -r -o "+mserver+" mserver/mserver.bmx"
	Print "mserver built OK!"
EndIf

