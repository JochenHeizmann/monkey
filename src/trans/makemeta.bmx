
'***** IMPORTANT *****
'
' Don't forget to rename output file if using MaxIDE!!!!!
'
' Should be one of:

Strict

Framework brl.blitz

Import brl.pixmap
Import brl.audiosample
Import brl.filesystem

Import brl.pngloader
Import brl.jpgloader
Import brl.bmploader
Import brl.tgaloader

Import brl.wavloader
Import brl.oggloader


Global meta$

Function MakeMeta( dir$ )

	For Local file$=EachIn LoadDir( "./"+dir )
	
		If file.StartsWith( "." ) Continue
		
		Local path$=file
		If dir path=dir+"/"+file
		
		'Print "Processing:"+path+"~n"
		
		Select FileType( path )
		Case FILETYPE_FILE
		
			Local ext$=ExtractExt( file ).ToLower()
			
			Select ext
			Case "png","jpg"
			
				meta:+"["+path+"];type=image/"+ext+";"
				Local pixmap:TPixmap=LoadPixmap( path )
				If pixmap
					meta:+"width="+pixmap.width+";"
					meta:+"height="+pixmap.height+";"
				EndIf
				meta:+"~n"
			Case "wav","ogg","mp3"
			
				meta:+"["+path+"];type=audio/"
				Select ext
				Case "wav" meta:+"x-wav;"
				Case "mp3" meta:+"mpeg;"
				Default meta:+ext+";"
				End Select
				Local sample:TAudioSample=LoadAudioSample( path )
				If sample
					meta:+"length="+sample.length+";"
					meta:+"hertz="+sample.hertz+";"
				EndIf
				meta:+"~n"
			End Select
		
		Case FILETYPE_DIR
		
			MakeMeta path
		
		End Select
	Next
	
End Function

'make 'metadata' strings for gxtk 'data' dirs...
'
'needs to be in bmx for now!

If AppArgs.length<3
	WriteStdout "Usage: scandir <path> <outfile>~n"
	exit_ -1
EndIf

If FileType( AppArgs[1] )=FILETYPE_DIR

	Local dir$=AppArgs[1]
	
	Local cd$=CurrentDir()
	
	ChangeDir dir
	
	MakeMeta ""
	
	ChangeDir cd

EndIf
	
SaveString meta,AppArgs[2]
