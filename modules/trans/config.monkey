
' Module trans.config
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

Global ENV_HOST$
Global ENV_LANG$
Global ENV_CONFIG$
Global ENV_TARGET$
Global ENV_MODPATH$
Global ENV_SAFEMODE	'True for safe mode!

Global _errInfo$
Global _errStack:=New StringList

Function PushErr( errInfo$ )
	_errStack.AddLast _errInfo
	_errInfo=errInfo
End

Function PopErr()
	_errInfo=_errStack.RemoveLast()
End

Function Err( err$ )
	Print _errInfo+" : Error : "+err
	ExitApp -1
End

Function SyntaxErr()
	Err "Syntax error."
End

Function InternalErr()
	Error "Internal error."
End

Function Asc( str$ )
	If str Return str[0]
End

Function IsSpace( ch )
	Return ch<=Asc(" ")
End

Function IsDigit( ch )
	Return ch>=Asc("0") And ch<=Asc("9")
End

Function IsAlpha( ch )
	Return (ch>=Asc("A") And ch<=Asc("Z")) Or (ch>=Asc("a") And ch<=Asc("z"))
End

Function IsBinDigit( ch )
	Return ch=Asc("0") Or ch=Asc("1")
End

Function IsHexDigit( ch )
	Return IsDigit(ch) Or (ch>=Asc("A") And ch<=Asc("F")) Or (ch>=Asc("a") And ch<=Asc("f"))
End

Function Todo() 
	Err "TODO!"
End

Function CppEnquote$( str$ )
	str=str.Replace( "\","\\" )
	str=str.Replace( "~q","\~q" )
	str=str.Replace( "~n","\n" )
	str=str.Replace( "~r","\r" )
	str=str.Replace( "~t","\t" )
'	str=str.Replace( "~0","\0" )	'Sort me out!
	str="~q"+str+"~q"
	Return str
End

Function BmxEnquote$( str$ )
	str=str.Replace( "~~","~~~~" )
	str=str.Replace( "~q","~~q" )
	str=str.Replace( "~n","~~n" )
	str=str.Replace( "~r","~~r" )
	str=str.Replace( "~t","~~t" )
	str=str.Replace( "~0","~~0" )
	str="~q"+str+"~q"
	Return str
End

Function BmxUnquote$( str$ )
	str=str[1..str.Length-1]
	str=str.Replace( "~~~~","~~z" )	'a bit dodgy - uses bad esc sequence ~z 
	str=str.Replace( "~~q","~q" )
	str=str.Replace( "~~n","~n" )
	str=str.Replace( "~~r","~r" )
	str=str.Replace( "~~t","~t" )
	str=str.Replace( "~~0","~0" )
	str=str.Replace( "~~z","~~" )
	Return str
End

