
' Module trans.toker
'
' Placed into the public domain 24/02/2011.
' No warranty implied; use at your own risk.

Import trans

'toke_type
Const TOKE_EOF=0
Const TOKE_SPACE=1
Const TOKE_IDENT=2
Const TOKE_KEYWORD=3
Const TOKE_INTLIT=4
Const TOKE_FLOATLIT=5
Const TOKE_STRINGLIT=6
Const TOKE_STRINGLITEX=7
Const TOKE_SYMBOL=8
Const TOKE_LINECOMMENT=9

'***** Tokenizer *****
Class Toker

	Const _keywords$=";"+
	"void;strict;"+
	"public;private;property;"+
	"bool;int;float;string;array;object;mod;continue;exit;"+
	"include;import;module;extern;"+
	"new;self;super;eachin;true;false;null;not;"+
	"extends;abstract;select;case;default;"+
	"const;local;global;field;method;function;class;"+
	"and;or;shl;shr;end;if;then;else;elseif;endif;while;wend;repeat;until;forever;"+
	"for;to;step;next;return;"+
	"interface;implements;inline;"

	Global _symbols$[]=[ "..","[]","()",":=","*=","/=","+=","-=","|=","&=","~=" ]

	Field _path$
	Field _line
	Field _source$
	Field _toke$
	Field _tokeType
	Field _tokePos
	
	Method New( path$,source$ )
		_path=path
		_line=1
		_source=source
		_toke=""
		_tokeType=TOKE_EOF
		_tokePos=0
	End
	
	Method Path$()
		Return _path
	End
	
	Method Line:Int()
		Return _line
	End
	
	Method NextToke$()

		If _tokePos=_source.Length
			_toke=""
			_tokeType=TOKE_EOF
			Return _toke
		Endif
		
		Local ch=_source[_tokePos]
		Local start=_tokePos
		_tokePos+=1
		_toke=""
		
		If ch=Asc( "~n" )
			_line+=1
			_tokeType=TOKE_SYMBOL
		Else If IsSpace( ch )
			While _tokePos<_source.Length And IsSpace( _source[_tokePos] ) And _source[_tokePos]<>Asc( "~n" )
				_tokePos+=1
			Wend
			_tokeType=TOKE_SPACE
		Else If ch=Asc("_") Or IsAlpha( ch )
			_tokeType=TOKE_IDENT
			While _tokePos<_source.Length
				Local ch=_source[_tokePos]
				If ch<>Asc("_") And Not IsAlpha( ch ) And Not IsDigit( ch ) Exit
				_tokePos+=1
			Wend
			_toke=_source[start.._tokePos]
			If _keywords.Contains( ";"+_toke.ToLower()+";" )
				_tokeType=TOKE_KEYWORD
			EndIf
		Else If IsDigit( ch )
			_tokeType=TOKE_INTLIT
			While _tokePos<_source.Length And IsDigit( _source[_tokePos] )
				_tokePos+=1
			Wend
			If _tokePos<_source.Length-1 And _source[_tokePos]=Asc(".") And IsDigit( _source[_tokePos+1] )
				_tokePos+=2
				_tokeType=TOKE_FLOATLIT
				While _tokePos<_source.Length And IsDigit( _source[_tokePos] )
					_tokePos+=1
				Wend
			EndIf
		Else If ch=Asc(".") And _tokePos<_source.Length And IsDigit( _source[_tokePos] )
			_tokeType=TOKE_FLOATLIT
			While _tokePos<_source.Length And IsDigit( _source[_tokePos] )
				_tokePos+=1
			Wend
		Else If ch=Asc("%") And _tokePos<_source.Length And IsBinDigit( _source[_tokePos] )
			_tokeType=TOKE_INTLIT
			_tokePos+=1
			While _tokePos<_source.Length And IsBinDigit( _source[_tokePos] )
				_tokePos+=1
			Wend
		Else If ch=Asc("$") And _tokePos<_source.Length And IsHexDigit( _source[_tokePos] )
			_tokeType=TOKE_INTLIT
			_tokePos+=1
			While _tokePos<_source.Length And IsHexDigit( _source[_tokePos] )
				_tokePos+=1
			Wend
		Else If ch=Asc("~q")
			_tokeType=TOKE_STRINGLIT
			While _tokePos<_source.Length And _source[_tokePos]<>Asc("~q")
				_tokePos+=1
			Wend
			If _tokePos<_source.Length _tokePos+=1 Else _tokeType=TOKE_STRINGLITEX
		Else If ch=Asc("'")
			_tokeType=TOKE_LINECOMMENT
			While _tokePos<_source.Length And _source[_tokePos]<>Asc("~n")
				_tokePos+=1
			Wend
			If _tokePos<_source.Length
				_tokePos+=1
				_line+=1
			Endif
		Else
			_tokeType=TOKE_SYMBOL
			For Local sym$=EachIn _symbols
				If ch<>sym[0] Continue
				If _source[_tokePos-1.._tokePos+sym.Length-1]=sym
					_tokePos+=sym.Length-1
					Exit
				EndIf
			Next
		EndIf
		
		If Not _toke _toke=_source[start.._tokePos]
		
		Return _toke
	End
	
	Method Toke$()
		Return _toke
	End
	
	Method TokeType()
		Return _tokeType
	End
	
	Method SkipSpace()
		While _tokeType=TOKE_SPACE
			NextToke
		Wend
	End
	
End
