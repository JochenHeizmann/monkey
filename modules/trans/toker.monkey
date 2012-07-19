
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
	"interface;implements;inline;alias;"

	Global _symbols$[]=[ "..","[]",":=","*=","/=","+=","-=","|=","&=","~=" ]

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
	
	Method New( toker:Toker )
		_path=toker._path
		_line=toker._line
		_source=toker._source
		_toke=toker._toke
		_tokeType=toker._tokeType
		_tokePos=toker._tokePos
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
				
		Local chr=_source[_tokePos]
		Local str$=String.FromChar( chr )
		
		Local start=_tokePos
		_tokePos+=1
		_toke=""
		
		If str="~n"
			_line+=1
			_tokeType=TOKE_SYMBOL
		Else If IsSpace( chr )
			While _tokePos<_source.Length And IsSpace( TCHR() ) And TSTR()<>"~n"
				_tokePos+=1
			Wend
			_tokeType=TOKE_SPACE
		Else If str="_" Or IsAlpha( chr )
			_tokeType=TOKE_IDENT
			While _tokePos<_source.Length
				Local chr=_source[_tokePos]
				If chr<>Asc("_") And Not IsAlpha( chr ) And Not IsDigit( chr ) Exit
				_tokePos+=1
			Wend
			_toke=_source[start.._tokePos]
			If _keywords.Contains( ";"+_toke.ToLower()+";" )
				_tokeType=TOKE_KEYWORD
			Endif
		Else If IsDigit( chr ) Or str="." And IsDigit( TCHR() )
			_tokeType=TOKE_INTLIT
			If str="." _tokeType=TOKE_FLOATLIT
			While IsDigit( TCHR() )
				_tokePos+=1
			Wend
			If _tokeType=TOKE_INTLIT And TSTR()="." And IsDigit( TCHR(1) )
				_tokeType=TOKE_FLOATLIT
				_tokePos+=2
				While IsDigit( TCHR() )
					_tokePos+=1
				Wend
			Endif
			If TSTR().ToLower()="e"
				_tokeType=TOKE_FLOATLIT
				_tokePos+=1
				If TSTR()="+" Or TSTR()="-" _tokePos+=1
				While IsDigit( TCHR() )
					_tokePos+=1
				Wend
			Endif
		Else If str="%" And IsBinDigit( TCHR() )
			_tokeType=TOKE_INTLIT
			_tokePos+=1
			While IsBinDigit( TCHR() )
				_tokePos+=1
			Wend
		Else If str="$" And IsHexDigit( TCHR() )
			_tokeType=TOKE_INTLIT
			_tokePos+=1
			While IsHexDigit( TCHR() )
				_tokePos+=1
			Wend
		Else If str="~q"
			_tokeType=TOKE_STRINGLIT
			While TSTR() And TSTR()<>"~q"
				_tokePos+=1
			Wend
			If _tokePos<_source.Length _tokePos+=1 Else _tokeType=TOKE_STRINGLITEX
		Else If str="'"
			_tokeType=TOKE_LINECOMMENT
			While TSTR() And TSTR()<>"~n"
				_tokePos+=1
			Wend
			If _tokePos<_source.Length
				_tokePos+=1
				_line+=1
			Endif
		Else
			_tokeType=TOKE_SYMBOL
			For Local sym$=Eachin _symbols
				If chr<>sym[0] Continue
				If _source[_tokePos-1.._tokePos+sym.Length-1]=sym
					_tokePos+=sym.Length-1
					Exit
				Endif
			Next
		Endif
		
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
	
Private

	Method TCHR( i=0 )
		If _tokePos+i<_source.Length Return _source[_tokePos+i]
	End
	
	Method TSTR$( i=0 )
		If _tokePos+i<_source.Length Return String.FromChar( _source[_tokePos+i] )
	End
	
	
End
