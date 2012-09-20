
Function IntToString:String( n:Int,base:Int=10 )
	If Not n Return "0"
	If n<0 Return "-"+IntToString( -n,base )
	Local t:String
	While n
		Local c=(n Mod base)+48
		If c>=58 c+=97-58
		t=String.FromChar( c )+t
		n=n/base
	Wend
	Return t
End

Function StringToInt:Int( str:String,base:Int=10 )
	Local i:=0
	Local l:=str.length
	While i<l And str[i]<=32
		i+=1
	Wend
	Local neg:=False
	If i<l And (str[i]=43 Or str[i]=45)
		neg=(str[i]=45)
		i+=1
	Endif
	Local n:=0
	While i<l
		Local c:=str[i],t:Int
		If c>=48 And c<58
			t=c-48
		Else If c>=65 And c<=90
			t=c-55
		Else If c>=97 And c<=122
			t=c-87
		Else
			Exit
		Endif
		If t>=base Exit
		n=n*base+t
		i+=1
	Next
	If neg n=-n
	Return n
End

Function Enquote:String( str:String,lang:String="" )
	Select lang
	Case "monkey"
		str=str.Replace( "~~","~~~~" )
		str=str.Replace( "~0","~~0" )	
		str=str.Replace( "~t","~~t" )
		str=str.Replace( "~n","~~n" )
		str=str.Replace( "~r","~~r" )
		str=str.Replace( "~q","~~q" )
		For Local i:=0 Until str.Length
			If str[i]>=32 And str[i]<128 Continue
			Local t:="~~x"+("0000"+IntToString( str[i],16 ))[-4..]
			str=str[..i]+t+str[i..]
			i+=t.Length-1
		Next
		Return "~q"+str+"~q"
	Case "cpp"
		str=str.Replace( "\","\\" )
		str=str.Replace( "~q","\~q" )
		str=str.Replace( "~n","\n" )
		str=str.Replace( "~r","\r" )
		str=str.Replace( "~t","\t" )
		For Local i:=0 Until str.Length
			If str[i]>=32 And str[i]<128 Continue
			Local t:="~qL~q\x"+IntToString( str[i],16 )+"~qL~q"
			str=str[..i]+t+str[i..]
			i+=t.Length-1
		Next
		Return "L~q"+str+"~q"
	Case "java","js","as","cs"
		str=str.Replace( "\","\\" )
		str=str.Replace( "~q","\~q" )
		str=str.Replace( "~n","\n" )
		str=str.Replace( "~r","\r" )
		str=str.Replace( "~t","\t" )
		For Local i:=0 Until str.Length
			If str[i]>=32 And str[i]<128 Continue
			Local t:="\x"+("0000"+IntToString( str[i],16 ))[-4..]
			str=str[..i]+t+str[i..]
			i+=t.Length-1
		Next
		Return "L~q"+str+"~q"
	End
	Return "~q"+str+"~q"
End

Function CEnquote:String( str:String,lang:String="" )
	If str.Length>=2 And str.StartsWith( "~q" ) And str.EndsWith( "~q" ) Return str
	Return Enquote( str,lang )
End

Function Dequote:String( str:String,lang:String="" )
End
