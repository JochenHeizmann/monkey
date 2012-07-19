
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

Global CONFIG_DEBUG?'=(ENV_CONFIG="debug")
Global CONFIG_RELEASE?'=(ENV_CONFIG="release")
Global CONFIG_PROFILE?'=(ENV_CONFIG="profile")

Global _errInfo$
Global _errStack:=New StringList

Function ReplaceEnvTags$( path$ )
	path=path.Replace( "${HOST}",ENV_HOST )
	path=path.Replace( "${LANG}",ENV_LANG )
	path=path.Replace( "${TARGET}",ENV_TARGET )
	path=path.Replace( "${CONFIG}",ENV_CONFIG )
	Return path
End

Function PushErr( errInfo$ )
	_errStack.AddLast _errInfo
	_errInfo=errInfo
End

Function PopErr()
	_errInfo=_errStack.RemoveLast()
End

Function Err( err$ )
	Print _errInfo+" : Error : "+err
	'Print _errInfo+" "+err
	ExitApp -1
End

Function InternalErr()
	Print _errInfo+" : Error : Internal error."
	Error _errInfo+" : Error : Internal error."
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

'enquote depending on ENV_LANG
'
Function LangEnquote$( str$ )
	str=str.Replace( "\","\\" )
	str=str.Replace( "~q","\~q" )
	str=str.Replace( "~n","\n" )
	str=str.Replace( "~r","\r" )
	str=str.Replace( "~t","\t" )
	For Local i=0 Until str.Length
		If str[i]>=32 And str[i]<128 Continue
		Local t$,n=str[i]
		While n
			Local c=(n&15)+48
			If c>=58 c+=97-58
			t=String.FromChar( c )+t
			n=(n Shr 4) & $0fffffff
		Wend
		If Not t t="0"
		Select ENV_LANG
		Case "cpp"
			t="~qL~q\x"+t+"~qL~q"
		Default
			t="\u"+("0000"+t)[-4..]
		End
		str=str[..i]+t+str[i+1..]
		i+=t.Length-1
	Next
	Select ENV_LANG
	Case "cpp"
		str="L~q"+str+"~q"
	Default
		str="~q"+str+"~q"
	End
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

