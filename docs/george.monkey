
Import trans

'maps module idents to nodes
Global modules:=New StringMap<Node>

Global module_json$
Global global_json$
Global member_json$

Global docs_mods:=New StringList
Global docs_search:=New StringList
Global docs_files:=New StringList

Function Enquote$( str$ )

	str=str.Replace( "~t","\t" )
	str=str.Replace( "~n","\n" )
	str=str.Replace( "~r","\r" )
	str=str.Replace( "~q","\~q" )

	return "~q"+str+"~q"
End 	

Function SaveJSON( str$,path$ )

	'I'm not strictly sure how/why this extra ESCing works...oh well!
	str=str.Replace( "\t","\\t" )
	str=str.Replace( "\n","\\n" )
	str=str.Replace( "\r","\\r" )
	str=str.Replace( "\~q","\\~q" )
	
	docs_files.AddLast "~q"+path+"~q:"+Enquote( str )
End

Function IsIdentChar( ch )
	Return (ch>=48 And ch<48+10) Or (ch>=65 And ch<65+26) Or (ch>=97 And ch<97+26) Or (ch=95)
End

'Convert eg: @blah to <b>blah</b>
'
Function Tagify$( docs$,sym$,tag$ )
	Local i
	While i<docs.Length
		i=docs.Find( sym,i )
		If i=-1 Exit
		If i>0 And docs[i-1]>32
			i+=sym.Length
			Continue
		Endif
		Local i2=i+sym.Length
		If i2=docs.Length Exit
		If docs[i2]="{"[0]
			While i2<docs.Length And docs[i2]<>"}"[0]
				i2+=1
			Wend
			If i2=docs.Length Exit
			Local t$=docs[..i]+"<"+tag+">"+docs[i+2..i2]+"</"+tag+">"
			docs=t+docs[i2+1..]
			i=t.Length
		Else
			If IsIdentChar( docs[i2] )
				While i2<docs.Length And IsIdentChar( docs[i2] )
					i2+=1
				Wend
			Else
				i2+=1
			Endif
			Local t$=docs[..i]+"<"+tag+">"+docs[i+1..i2]+"</"+tag+">"
			docs=t+docs[i2..]
			i=t.Length
		Endif
	Wend
	Return docs
End

Global style$

Class Node

	Field type$				'eg: Function, Module, Class, etc
	Field ident$			'eg: SetColor
	Field shortdesc$		'eg: Sets the current color
	
	Field parent:Node
	Field children:=New StringMap<Node>
	
	Field href$
	Field source$
	
	Method New( type$,ident$,shortdesc$,parent:Node )
		Self.type=type
		Self.ident=ident
		Self.shortdesc=shortdesc
		Self.parent=parent
		If parent parent.children.Insert ident,Self
		If type="Module" modules.Insert ident,Self
	End
	
	Method ClassNode:Node()
		If type="Class" Or type="Type" Return Self
		If parent Return parent.ClassNode()
	End
	
	Method ModuleNode:Node()
		If type="Module" Return Self
		If parent Return parent.ModuleNode()
	End
	
	Function AddHtml$( html$,sect$,content$ )

		If Not html.EndsWith( "</p>" ) html+="</p>"
		
		html=html.Replace( "<p></p>","" )				'delete 'blank' paragraphs
		html=html.Replace( "</p><p>","</p><br><p>" )	'double space!

		'If content content+=","
		
		content+=",~q"+sect+"~q:"+Enquote( html )
		
		Return content
	End
	
	Method ToJSON()
	
		Local sect$="introduction",html$="<p>",content$,pre
		
		For Local line$=Eachin source.Split( "~n" )
		
			line=line.Replace( "~r","" )
			line=line.Replace( "<","&lt;" )
			line=line.Replace( ">","&gt;" )
			
			If pre
				If line="}}"
					pre=False
					html+="</pre>\n"
				Else
					html+=line+"\n"
				Endif
				Continue
			Endif

			line=Tagify( line,"@","b" )
			line=Tagify( line,"%","i" )
			line=Tagify( line,"_","u" )
			
			If line.EndsWith( "::" )
				content=AddHtml( html,sect,content )
				sect=line[..-2].ToLower()
				html="<p>"
			Else If line="{{"
				pre=True
				html+="<pre>\n"
			Else If line
				If html.EndsWith( "</p>" ) html+="<p>"
				If line.EndsWith( "\" ) line=line[-1..]+"<br>"
				html+=line
			Else
				If Not html.EndsWith( "</p>" ) html+="</p>"
			Endif
		Next
		content=AddHtml( html,sect,content )
		
		Local classid$,modpath$
		If ClassNode() classid=ClassNode().ident
		If ModuleNode() modpath=ModuleNode().ident
		
		Local json$,file$
		
		If parent.type="Class" Or parent.type="Type"
			json=member_json
			file=modpath+"."+classid+"."+ident
		Else If parent.type="Module"
			json=global_json
			file=modpath+"."+ident
		Else If type="Module"
			json=module_json
			file=modpath+".main"
		Endif
		
		If json
		
			json=json.Replace( "${TYPE}",type )
			json=json.Replace( "${IDENT}",ident )
			json=json.Replace( "${CLASSID}",classid )
			json=json.Replace( "${MODPATH}",modpath )
			json=json.Replace( "${CONTENT}",content )
			
			Print "Saving "+file
			SaveJSON json,file
			
			If type="Module"
				
				docs_mods.AddLast "{~qname~q:~q"+ident+"~q,~qlink~q:~q"+ident+"~q}"
				docs_search.AddLast "{~qid~q:~q"+ident+"~q,~qtext~q:~q"+ident+"~q}"
	
				Local navMap:=New StringMap<List<Node>>
				
				For Local node:=Eachin children.Values
					Local list:=navMap.Get( node.type )
					If Not list
						list=New List<Node>
						navMap.Insert node.type,list
					Endif
					list.AddLast node
				Next
				
				Local nav$
				
				Local types:=["Const","Global","Function","Type","Class"]
				Local tysects:=["constants","globals","functions","types","classes"]
				
				Local navs:=New StringList
				
				For Local i=0 Until types.Length

					Local type:=types[i]
					Local list:=navMap.Get( type )
					If Not list Continue
					
					Local links:=New StringList

					For Local node:=Eachin list
						Local name$=node.ident
						Local link$=node.ident+".html"
						links.AddLast "{~qname~q:~q"+name+"~q,~qlink~q:~q"+name+"~q}"
						docs_search.AddLast "{~qid~q:~q"+ident+"/"+name+"~q,~qtext~q:~q"+name+"~q}"
						
						If node.type="Class" Or node.type="Type"
							For Local child:=Eachin node.children.Values
								Local name$=node.ident+"."+child.ident
								Local link$=node.ident+"/"+child.ident+".html"
								links.AddLast "{~qname~q:~q"+name+"~q,~qlink~q:~q"+name+"~q}"
								docs_search.AddLast "{~qid~q:~q"+ident+"/"+name+"~q,~qtext~q:~q"+name+"~q}"
							Next
						Endif
					Next
					
					navs.AddLast "~q"+tysects[i]+"~q:["+links.Join(",")+"]"
					
				Next
				
				SaveJSON "{"+navs.Join(",")+"}",ident+".contents"
				
			Endif
		Endif
		
		For Local node:=Eachin children.Values
			node.ToJSON
		Next
		
	End
	
End

Function MakeDocs( path$,parent:Node )

	Local toker:=New Toker( path,LoadString( path ) )

	Local node:Node=New Node	'dummy node

	While toker.NextToke
	
		If toker.Toke<>"#"
			While toker.Toke And toker.Toke<>"~n"
				node.source+=toker.Toke
				toker.NextToke
			Wend
			node.source+="~n"
			Continue
		Endif
		
		Select toker.NextToke
		Case "End"
		
			toker.NextToke
			parent=parent.parent
			If Not parent Return

			While toker.Toke And toker.Toke<>"~n"
				toker.NextToke
			Wend
			
		Case "Const","Global","Class","Type","Method","Function","Module","Field","Chapter"
			Local type$=toker.Toke
			
			toker.NextToke
			Local line$
			While toker.Toke And toker.Toke<>"~n"
				line+=toker.Toke
				toker.NextToke
			Wend
			
			Local ident$,shortdesc$
			
			Local bits$[]=line.Split( " - " )
			
			If bits.Length=2
				ident=bits[0].Trim()
				shortdesc=bits[1].Trim()
			Else
				ident=line.Trim()
			Endif

			node=New Node( type,ident,shortdesc,parent )
			
			Select type
			Case "Class","Type","Module"
				parent=node
			End Select

		Default
			Print "Unrecognized metasymbol: '"+toker.Toke+"'"
			While toker.Toke And toker.Toke<>"~n"
				toker.NextToke
			Wend
		End Select
	Wend

End

Function Main()

	ChangeDir "../.."
	
	module_json=LoadString( "module_json.json" )
	global_json=LoadString( "global_json.json" )
	member_json=LoadString( "member_json.json" )
	
	Local root:=New Node( "",".","module docs",Null )
	
	MakeDocs "monkey_.txt",root
	MakeDocs "monkey_lang.txt",root
	MakeDocs "monkey_list.txt",root
	MakeDocs "monkey_set.txt",root
	MakeDocs "monkey_map.txt",root
	MakeDocs "monkey_math.txt",root
	MakeDocs "monkey_random.txt",root
	MakeDocs "monkey_stack.txt",root
	
	MakeDocs "mojo_.txt",root
	MakeDocs "mojo_app.txt",root
	MakeDocs "mojo_audio.txt",root
	MakeDocs "mojo_graphics.txt",root
	MakeDocs "mojo_input.txt",root
	
	ChangeDir "modules"
	
	For Local node:=Eachin modules.Values
		node.ToJSON
	Next

	SaveJSON "{~qmodules~q:["+docs_mods.Join(",")+"]}","modules.list"
	
	SaveString "var files={"+docs_files.Join(",~n")+"};","json-docs-content.js"
	
	SaveString "var searchData=["+docs_search.Join( ",~n" )+"];","json-docs-search.js"
	
End
