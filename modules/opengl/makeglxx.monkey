
Import os

Function ReplaceBlock$( text$,startTag$,endTag$,repText$ )

	'Find *first* start tag
	Local i=text.Find( startTag )
	If i=-1 Return text
	i+=startTag.Length
	While i<text.Length And text[i-1]<>10
		i+=1
	Wend
	'Find *last* end tag
	Local i2=text.Find( endTag,i )
	If i2=-1 Return text
	While i2>0 And text[i2]<>10
		i2-=1
	Wend
	'replace text!
	Return text[..i]+repText+text[i2..]
End

Function MakeGL11()

	Local kludge_glfw,kludge_android,kludge_ios

	Local const_decls:=New StringStack
	Local glfw_decls:=New StringStack
	Local ios_decls:=New StringStack
	Local android_decls:=New StringStack

	Print "Parsing gles11_src.txt"
		
	Local src:=LoadString( "gles11_src.txt" )
	
	Local lines:=src.Split( "~n" )
	
	For Local line:=Eachin lines
	
		line=line.Trim()
		
		If line.StartsWith( "Const " )
		
			const_decls.Push( line )
			
		Else If line.StartsWith( "Kludge " )
		
			kludge_glfw=False
			kludge_android=False
			kludge_ios=False
		
			If line.Contains( "glfw" ) kludge_glfw=True
			If line.Contains( "android" ) kludge_android=True
			If line.Contains( "ios" ) kludge_ios=True
			
		Else If line.StartsWith( "Function " )
			Local i=line.Find( "(" )
			If i<>-1
				Local id$=line[9..i]
				Local i2=id.Find( ":" )
				If i2<>-1 id=id[..i2]
				
				'GLFW!
				If kludge_glfw
					glfw_decls.Push line+"=~q_"+id+"~q"
				Else
					glfw_decls.Push line
				Endif
				
				'Android!
				If kludge_android
					android_decls.Push line+"=~qbb_opengl_gles11._"+id+"~q"
				Else
					android_decls.Push line+"=~qGLES11."+id+"~q"
				Endif
				
				'ios!
				If kludge_ios
					ios_decls.Push line+"=~q_"+id+"~q"
				Else
					ios_decls.Push line
				Endif
				
			Endif
		Endif
		
	Next
	
	Print "Updating gles11.monkey..."
	
	Local dst:=LoadString( "gles11.monkey" )
	
	dst=ReplaceBlock( dst,"'${CONST_DECLS}","'${END}",const_decls.Join( "~n" ) )
	dst=ReplaceBlock( dst,"'${GLFW_DECLS}","'${END}",glfw_decls.Join( "~n" ) )
	dst=ReplaceBlock( dst,"'${ANDROID_DECLS}","'${END}",android_decls.Join( "~n" ) )
	dst=ReplaceBlock( dst,"'${IOS_DECLS}","'${END}",ios_decls.Join( "~n" ) )

'	Print dst	
	SaveString dst,"gles11.monkey"

	Print "Done!"
End

Function MakeGL20()

	Local const_decls:=New StringStack
	Local html5_decls:=New StringStack
	Local glfw_decls:=New StringStack
	Local ios_decls:=New StringStack
	Local android_decls:=New StringStack

	Print "Parsing gles20_src.txt"
		
	Local src:=LoadString( "gles20_src.txt" )
	
	Local lines:=src.Split( "~n" )
	
	For Local line:=Eachin lines
	
		line=line.Trim()
		
		If line.StartsWith( "Const " )
		
			const_decls.Push( line )
			
		Else If line.StartsWith( "Function " )
			Local i=line.Find( "(" )
			If i<>-1
				Local id$=line[9..i]
				Local i2=id.Find( ":" )
				If i2<>-1 id=id[..i2]
				
				html5_decls.Push line+"=~qgl."+id[2..3].ToLower()+id[3..]+"~q"
				
				glfw_decls.Push line
				
				ios_decls.Push line
				
				android_decls.Push line+"=~qGLES20."+id+"~q"
				
			Endif
		Endif
		
	Next
	
	Print "Updating gles20.monkey..."
	
	Local dst:=LoadString( "gles20.monkey" )
	
	dst=ReplaceBlock( dst,"'${CONST_DECLS}","'${END}",const_decls.Join( "~n" ) )
	dst=ReplaceBlock( dst,"'${HTML5_DECLS}","'${END}",html5_decls.Join( "~n" ) )
	dst=ReplaceBlock( dst,"'${GLFW_DECLS}","'${END}",glfw_decls.Join( "~n" ) )
	dst=ReplaceBlock( dst,"'${IOS_DECLS}","'${END}",ios_decls.Join( "~n" ) )
	dst=ReplaceBlock( dst,"'${ANDROID_DECLS}","'${END}",android_decls.Join( "~n" ) )

'	Print dst	
	SaveString dst,"gles20.monkey"

	Print "Done!"
End

Function Main()

	ChangeDir "../../"

	MakeGL11
	
'	MakeGL20
	
End
