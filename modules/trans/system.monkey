
' Module trans.system
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Private

Import "native/system.${LANG}"

Extern Private

Class Device="bbSystem"

	'Misc extra stuff...
	'	
	Method HostOS$()
	Method AppPath$()
	Method AppArgs$[]()
	Method ExitApp( retcode )
	Method Alert( err$,kind )
	Method FileType( path$ )
	Method FileSize( path$ )
	Method FileTime( path$ )
	Method LoadString$( path$ )
	Method SaveString( str$,path$ )
	Method LoadDir$[]( path$ )
	Method CopyFile( src$,dst$ )
	
	'Posix-ish stuff...
	'
	Method Realpath$( path$ )
	Method Chdir( path$ )
	Method Getcwd$()
	Method Mkdir( path$ )
	Method Rmdir( path$ )
	Method Remove( path$ )
	Method Setenv( name$,value$ )
	Method Getenv$( name$ )
	Method System( cmd$ )
End

Private

Class SystemDevice Extends Device
End

Global device:Device=New SystemDevice

Global host_os$

Public

Const ALERT_OK=0

Const FILETYPE_NONE=0
Const FILETYPE_FILE=1
Const FILETYPE_DIR=2

'***** App stuff *****

Function HostOS$()
	Return device.HostOS()
End

Function AppPath$()
	Return device.AppPath()
End

Function AppArgs$[]()
	Return device.AppArgs()
End

Function Alert( msg$,kind=ALERT_OK )
	Return device.Alert( msg,kind )
End

Function ExitApp( retcode=0 )
	device.ExitApp retcode
End

Function GetEnv$( name$ )
	Return device.Getenv( name )
End

Function SetEnv( name$,value$ )
	device.Setenv name,value
End

Function Execute( cmd$ )
	Return device.System( cmd )
End


'***** File *****

Function FileType( path$ )
	Return device.FileType( path )
End

Function FileSize( path$ )
	Return device.FileSize( path )
End

Function FileTime( path$ )
	Return device.FileTime( path )
End

Function LoadString$( path$ )
	Return device.LoadString( path )
End

Function SaveString( str$,path$ )
	device.SaveString str,path
End

Function CreateFile( path$ )
	SaveString "",path
End

Function DeleteFile( path$ )
	device.Remove path
	Return FileType( path )=FILETYPE_NONE
End

Function CopyFile( srcpath$,dstpath$ )
	If FileType( dstpath )=FILETYPE_DIR
		dstpath+="/"+StripDir( srcpath )
	Endif
	If device.CopyFile( srcpath,dstpath )>=0 Return True
	'Print "CopyFile failed, srcpath="+srcpath+", dstpath="+dstpath
End


'***** Directory *****

Function RealPath$( path$ )
	Return FixPath( device.Realpath( path ) )
End

Function CurrentDir$()
	Return FixPath( device.Getcwd() )
End

Function ChangeDir( dir$ )
	device.Chdir dir
End

Function LoadDir$[]( path$,recursive=False )

	If Not recursive Return device.LoadDir( path )
	
	Local dirs:=New StringList,files:=New StringList
	dirs.AddLast ""
	
	While Not dirs.IsEmpty()

		Local dir$=dirs.RemoveFirst()

		For Local file$=Eachin LoadDir( path+"/"+dir )
		
			If file.StartsWith(".") Continue
			If dir file=dir+"/"+file
			
			Select FileType( path+"/"+file )
			Case FILETYPE_FILE
				files.AddLast file
			CASE FILETYPE_DIR
				dirs.AddLast file
			End
		Next
	Wend

	Return files.ToArray()

End

Function CreateDir( path$ )
	device.Mkdir path
	If FileType( path )=FILETYPE_DIR Return True
	'Print "CreateDir failed, path="+path
End

Function DeleteDir( path$,recursive=False )
	Select FileType( path )
	Case FILETYPE_NONE 
		Return True
	Case FILETYPE_FILE
		Return False
	End Select
	
	If recursive	
		For Local f$=Eachin LoadDir( path )
			If f="." Or f=".." Continue

			Local fpath$=path+"/"+f

			If FileType( fpath )=FILETYPE_DIR
				If Not DeleteDir( fpath,True ) Return
			Else
				If Not DeleteFile( fpath ) Return
			Endif
		Next
	Endif

	device.Rmdir path
	Return FileType( path )=FILETYPE_NONE
End

Function CopyDir( srcpath$,dstpath$,hidden=True )
	If FileType( srcpath )<>FILETYPE_DIR
		Return False
	Endif

	'do this before create of destdir to allow a dir to be copy into itself!
	'
	Local files:=LoadDir( srcpath )
	
	Select FileType( dstpath )
	Case FILETYPE_NONE 
		If Not CreateDir( dstpath ) Return False
	Case FILETYPE_FILE 
		Return False
	End
	
	For Local f$=Eachin files
		If hidden
			If f="." Or f=".." Continue
		Else
			If f.StartsWith( "." ) Continue
		Endif
		
		Local srcp$=srcpath+"/"+f
		Local dstp$=dstpath+"/"+f
		
		If FileType( srcp )=FILETYPE_DIR
			If Not CopyDir( srcp,dstp ) Return False
		Else
			If Not CopyFile( srcp,dstp ) Return False
		Endif
	Next
	
	Return True
End

'***** Utility string functions *****

Function FixPath$( path$ )
	path=path.Replace( "\","/" )
	Return path
End

Function StripDir$( path$ )
	path=FixPath( path )
	Local i=path.FindLast( "/" )
	If i<>-1 Return path[i+1..]
	Return path
End

Function ExtractDir$( path$ )
	path=FixPath( path )
	Local i=path.FindLast( "/" )
	If i<>-1 Return path[..i]
End

Function StripExt$( path$ )
	path=FixPath( path )
	Local i=path.FindLast( "." )
	If i<>-1 And path.Find( "/",i+1 )=-1 Return path[..i]
	Return path
End

Function ExtractExt$( path$ )
	path=FixPath( path )
	Local i=path.FindLast( "." )
	If i<>-1 And path.Find( "/",i+1 )=-1 Return path[i+1..]
End

Function StripAll$( path$ )
	path=FixPath( path )
	Return StripDir( StripExt( path ) )
End

