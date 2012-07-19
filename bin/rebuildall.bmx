
Strict

Global QuickTrans

?Win32

'GUICKTRANS=True

Local trans$="trans_winnt.exe"
Local makemeta$="makemeta_winnt.exe"
Local newtrans$="trans.build/stdcpp/main_winnt.exe"

?MacOS

QUICKTRANS=True

Local trans$="./trans_macos"
Local makemeta$="makemeta_macos"
Local newtrans$="trans.build/stdcpp/main_macos"

?Linux

QUICKTRANS=True

Local trans$="./trans_linux"
Local makemeta$="makemeta_linux"
Local newtrans$="trans.build/stdcpp/main_linux"

?

If 1

'	Local modpath$=RealPath( "../modules" )
	
	If QUICKTRANS
	
		'just recompile existing C++ source
	
?Macos
		If system_( "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -o trans_macos trans.build/stdcpp/main.cpp" )
?Not Macos
		If system_( "g++ -o trans_macos trans.build/stdcpp/main.cpp" )
?
			Print "Failed to compile trans"
			Return
		EndIf
		
	Else
	
		'Full on trans
		
'		If system_( trans+" -clean -target=stdcpp -modpath="+modpath+" trans.monkey" )

		If system_( trans+" -clean -target=stdcpp trans.monkey" )
			Print "Failed to build trans"
			Return
		EndIf
		
		Delay 100	'wassup?!?
		DeleteFile trans
		
		If FileType( trans )<>FILETYPE_NONE
			Print "Failed to delete trans"
			Return
		EndIf
		
		CopyFile newtrans,trans
		If FileType( trans )<>FILETYPE_FILE
			Print "Failed to copy trans"
			Return
		EndIf
		
	EndIf
	
	Print "trans built OK!"
EndIf

If system_( "~q"+BlitzMaxPath()+"/bin/bmk~q makeapp -t console -a -r -o "+makemeta+" makemeta.bmx" )
	Print "Failed to build makemeta"
	Return
EndIf

Print "makemeta built OK!"
