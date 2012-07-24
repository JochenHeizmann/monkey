
Strict

RebuildTrans
'RebuildMonk
'RebuildMServer

End

?Win32

Const QUICKTRANS=False

Const trans$="..\bin\trans_winnt.exe"
Const trans2$=trans
'Const trans2$="..\bin\trans_winnt_bak.exe"
Const newtrans$="trans\trans.build\stdcpp\main_winnt.exe"
Const mserver$="..\bin\mserver_winnt.exe"

?MacOS

Const QUICKTRANS=False

Const trans$="../bin/trans_macos"
Const trans2$=trans
'Const trans2$="../bin/trans_macos_bak"
Const newtrans$="trans/trans.build/stdcpp/main_macos"
Const mserver$="../bin/mserver_macos"

?Linux

Const QUICKTRANS=True

Const trans$="../bin/trans_linux"
Const trans2$=trans
'Const trans2$="../bin/trans_linux_bak"
Const newtrans$="trans/trans.build/stdcpp/main_linux"
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

Function RebuildTrans()
	If QUICKTRANS
?Win32
		system "g++ -o "+trans+" trans\trans.build\stdcpp\main.cpp"
?Macos
		system "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -o "+trans+" trans/trans.build/stdcpp/main.cpp"
?Linux
		system "g++ -o "+trans+" trans/trans.build/stdcpp/main.cpp"
?
		Return
	Endif
	
	system trans2+" -clean -target=stdcpp -config=release +CPP_INCREMENTAL_GC=0 +CPP_DOUBLE_PRECISION_FLOATS=1 trans/trans.monkey"

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
	Print "trans built OK!"

End Function

Function RebuildMonk()
	system "~q"+BlitzMaxPath()+"/bin/bmk~q makeapp -t gui -a -r -o ../monk monk/monk.bmx"
?MacOS
	system "cp monk/info.plist ../monk.app/Contents"
	system "cp monk/monk.icns ../monk.app/Contents/Resources"
?
	Print "monk built OK!"
End Function

Function RebuildMServer()
	system "~q"+BlitzMaxPath()+"/bin/bmk~q makeapp -h -t gui -a -r -o "+mserver+" mserver/mserver.bmx"
	Print "mserver built OK!"
End Function
