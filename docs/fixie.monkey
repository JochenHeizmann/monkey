
'Absolutely horrendous hack to fix links to ie.css
'
Import os

Function Main()

	ChangeDir "../.."

	CopyFile "ie.css","blitz-wiki.appspot.com/gae-wiki-static/ie.css"
	
	Local dir:="blitz-wiki.appspot.com"
	
	Local files:=LoadDir( dir,True )
	
	For Local f:=Eachin files
	
		If Not f.EndsWith( ".html" ) Continue
		
		Local rel:=""
		If f.Contains( "/" ) rel="../"	'should really count /'s...just used for index.html for now.
		
		Local t:=LoadString( dir+"/"+f )
		t=t.Replace( "href=~q/gae-wiki-static/ie.css~q","href=~q"+rel+"gae-wiki-static/ie.css~q" )
		SaveString t,dir+"/"+f

	Next
	
End