
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
	
	game_canvas=document.getElementById( "GameCanvas" );
	
	game_console=document.getElementById( "GameConsole" );

	try{
	
		bbInit();
		bbMain();
		
		if( game_runner!=null ) game_runner();
		
	}catch( err ){
	
		alertError( err );
	}
}

var game_canvas;
var game_console;
var game_runner;

//${CONFIG_BEGIN}
//${CONFIG_END}

//${METADATA_BEGIN}
//${METADATA_END}

function getMetaData( path,key ){

	if( path.toLowerCase().indexOf("monkey://data/")!=0 ) return "";
	path=path.slice(14);

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

function fixDataPath( path ){
	if( path.toLowerCase().indexOf("monkey://data/")==0 ) return "data/"+path.slice(14);
	return path;
}

function openXMLHttpRequest( req,path,async ){

	path=fixDataPath( path );
	
	var xhr=new XMLHttpRequest;
	xhr.open( req,path,async );
	return xhr;
}

function loadArrayBuffer( path ){

	var xhr=openXMLHttpRequest( "GET",path,false );

	if( xhr.overrideMimeType ) xhr.overrideMimeType( "text/plain; charset=x-user-defined" );

	xhr.send( null );
	
	if( xhr.status!=200 && xhr.status!=0 ) return null;

	var r=xhr.responseText;
	var buf=new ArrayBuffer( r.length );

	for( var i=0;i<r.length;++i ){
		this.dataView.setInt8( i,r.charCodeAt(i) );
	}
	return buf;
}

function loadString( path ){
	path=fixDataPath( path );
	var xhr=new XMLHttpRequest();
	xhr.open( "GET",path,false );
	xhr.send( null );
	if( (xhr.status==200) || (xhr.status==0) ) return xhr.responseText;
	return "";
}

function loadImage( path,onloadfun ){

	var ty=getMetaData( path,"type" );
	if( ty.indexOf( "image/" )!=0 ) return null;

	var image=new Image();
	
	image.meta_width=parseInt( getMetaData( path,"width" ) );
	image.meta_height=parseInt( getMetaData( path,"height" ) );
	image.onload=onloadfun;
	image.src="data/"+path.slice(14);
	
	return image;
}

function loadAudio( path ){

	path=fixDataPath( path );
	
	var audio=new Audio( path );
	return audio;
}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
