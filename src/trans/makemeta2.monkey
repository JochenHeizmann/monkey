
Import trans.system

Import "makemeta2.cpp"

Extern

Global info_width="makemeta2::info_width"
Global info_height="makemeta2::info_height"
Global info_length="makemeta2::info_length"
Global info_hertz="makemeta2::info_hertz"

Function get_info_png( path$ )="makemeta2::get_info_png"
Function get_info_wav( path$ )="makemeta2::get_info_wav"

Public

Global meta$

Function MakeMeta( dir$ )

	For Local file$=EachIn LoadDir( "./"+dir )
	
		If file.StartsWith( "." ) Continue
		
		Local path$=file
		If dir path=dir+"/"+file
		
		Select FileType( path )
		Case FILETYPE_FILE
			Local ext$=ExtractExt( file ).ToLower()
			
			Select ext
			Case "png"	',"jpg"
				If get_info_png( path )=0
					meta+="["+path+"];type=image/"+ext+";"
					meta+="width="+info_width+";"
					meta+="height="+info_height+";"
					meta+="~n"
				EndIf
			Case "wav"	',"ogg","mp3"
				If get_info_wav( path )=0
					meta+="["+path+"];type=audio/"
					meta+="length="+info_length+";"
					meta+="hertz="+info_hertz+";"
					meta+="~n"
				EndIf
			End 
		Case FILETYPE_DIR
			MakeMeta path
		End
	Next
End

Function Main()

	If AppArgs.Length<3
		Error "Usage: scandir <path> <outfile>~n"
	EndIf

	If FileType( AppArgs[1] )=FILETYPE_DIR
		Local cd$=CurrentDir()
		ChangeDir AppArgs[1]
		MakeMeta ""
		ChangeDir cd
	EndIf
		
	SaveString meta,AppArgs[2]

End
