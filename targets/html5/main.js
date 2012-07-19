
//Change this to true for a stretchy canvas!
//
var RESIZEABLE_CANVAS=false;

//Start us up!
//
window.onload=function( e ){

	if( RESIZEABLE_CANVAS ){
		window.onresize=function( e ){
			var canvas=document.getElementById( "GameCanvas" );

			//This vs window.innerWidth, which apparently doesn't account for scrollbar?
			var width=document.body.clientWidth;
			
			//This vs document.body.clientHeight, which does weird things - document seems to 'grow'...perhaps canvas resize pushing page down?
			var height=window.innerHeight;			

			canvas.width=width;
			canvas.height=height;
		}
		window.onresize( null );
	}
	
	var canvas=document.getElementById( "GameCanvas" );

	GameMain( canvas );
}

//${METADATA_BEGIN}
//${METADATA_END}
function getMetaData( path,key ){	
	var i=META_DATA.indexOf( "["+path+"]" );
	if( i==-1 ) return "";
	i+=path.length+2;

	var e=META_DATA.indexOf( "\n",i );
	if( e==-1 ) e=META_DATA.length;

	i=META_DATA.indexOf( ";"+key+"=",i )
	if( i==-1 || i>=e ) return "";
	i+=key.length+2;

	e=META_DATA.indexOf( ";",i );
	if( e==-1 ) return "";

	return META_DATA.slice( i,e );
}

function loadString( path ){
	if( path=="" ) return "";
//${TEXTFILES_BEGIN}
//${TEXTFILES_END}
}

//This is generally redefined by mojo.
//
function GameMain( canvas ){
	bb_Init();
	bb_Main();
}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}

//This overrides print in 'std.lang/lang.js'
//
function print( str ){

	var cons=document.getElementById( "GameConsole" );
	if( cons ){
		cons.value+=str+"\n";
	}
	
	if( window.console!=undefined ){
		window.console.log( str );
	}
}

