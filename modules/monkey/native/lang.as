
// Actionscript Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

//***** ActionScript Runtime *****

import flash.display.*;
import flash.text.*;

//Consts for radians<->degrees conversions
var D2R:Number=0.017453292519943295;
var R2D:Number=57.29577951308232;

//private
var _console:TextField;
var _errInfo:String="";
var _errStack:Array=[];

function _getConsole():TextField{
	if( _console ) return _console;
	_console=new TextField();
	_console.x=0;
	_console.y=0;
	_console.width=game.stage.stageWidth;
	_console.height=game.stage.stageHeight;
	_console.background=false;
	_console.backgroundColor=0xff000000;
	_console.textColor=0xffffff00;
	game.stage.addChild( _console );
	return _console;
}

function pushErr():void{
	_errStack.push( _errInfo );
}

function popErr():void{
	_errInfo=_errStack.pop();
}

function stackTrace():String{
	var str:String="";
	pushErr();
	_errStack.reverse();
	for( var i:int=0;i<_errStack.length;++i ){
		str+=_errStack[i]+"\n";
	}
	_errStack.reverse();
	popErr();
	return str;
}

function print( str:String ):int{
	var console:TextField=_getConsole();
	if( !console ) return 0;
	console.appendText( str+"\n" );
	return 0;
}

function showError( err:String ):void{
	if( err.length ) print( "Monkey runtime error: "+err+"\n"+stackTrace() );
}

function error( err:String ):int{
	throw err;
	return 0;
}

function dbg_object( obj:Object ):Object{
	if( obj ) return obj;
	error( "Null object access" );
	return obj;
}

function dbg_array( arr:Array,index:int ):Array{
	if( index>=0 && index<arr.length ) return arr;
	error( "Array index out of range" );
	return arr;
}

function new_bool_array( len:int ):Array{
	var arr:Array=new Array( len )
	for( var i:int=0;i<len;++i ) arr[i]=false;
	return arr;
}

function new_number_array( len:int ):Array{
	var arr:Array=new Array( len )
	for( var i:int=0;i<len;++i ) arr[i]=0;
	return arr;
}

function new_string_array( len:int ):Array{
	var arr:Array=new Array( len );
	for( var i:int=0;i<len;++i ) arr[i]='';
	return arr;
}

function new_array_array( len:int ):Array{
	var arr:Array=new Array( len );
	for( var i:int=0;i<len;++i ) arr[i]=[];
	return arr;
}

function new_object_array( len:int ):Array{
	var arr:Array=new Array( len );
	for( var i:int=0;i<len;++i ) arr[i]=null;
	return arr;
}

function resize_bool_array( arr:Array,len:int ):Array{
	var i:int=arr.length;
	arr=arr.slice(0,len);
	if( len<=i ) return arr;
	arr.length=len;
	while( i<len ) arr[i++]=false;
	return arr;
}

function resize_number_array( arr:Array,len:int ):Array{
	var i:int=arr.length;
	arr=arr.slice(0,len);
	if( len<=i ) return arr;
	arr.length=len;
	while( i<len ) arr[i++]=0;
	return arr;
}

function resize_string_array( arr:Array,len:int ):Array{
	var i:int=arr.length;
	arr=arr.slice(0,len);
	if( len<=i ) return arr;
	arr.length=len;
	while( i<len ) arr[i++]="";
	return arr;
}

function resize_array_array( arr:Array,len:int ):Array{
	var i:int=arr.length;
	arr=arr.slice(0,len);
	if( len<=i ) return arr;
	arr.length=len;
	while( i<len ) arr[i++]=[];
	return arr;
}

function resize_object_array( arr:Array,len:int ):Array{
	var i:int=arr.length;
	arr=arr.slice(0,len);
	if( len<=i ) return arr;
	arr.length=len;
	while( i<len ) arr[i++]=null;
	return arr;
}

function string_compare( lhs:String,rhs:String ):int{
	var n:int=Math.min( lhs.length,rhs.length ),i:int,t:int;
	for( i=0;i<n;++i ){
		t=lhs.charCodeAt(i)-rhs.charCodeAt(i);
		if( t ) return t;
	}
	return lhs.length-rhs.length;
}

function string_replace( str:String,find:String,rep:String ):String{	//no unregex replace all?!?
	var i:int=0;
	for(;;){
		i=str.indexOf( find,i );
		if( i==-1 ) return str;
		str=str.substring( 0,i )+rep+str.substring( i+find.length );
		i+=rep.length;
	}
	return str;
}

function string_trim( str:String ):String{
	var i:int=0,i2:int=str.length;
	while( i<i2 && str.charCodeAt(i)<=32 ) i+=1;
	while( i2>i && str.charCodeAt(i2-1)<=32 ) i2-=1;
	return str.slice( i,i2 );
}

function string_starts_with( str:String,sub:String ):Boolean{
	return sub.length<=str.length && str.slice(0,sub.length)==sub;
}

function string_ends_with( str:String,sub:String ):Boolean{
	return sub.length<=str.length && str.slice(str.length-sub.length,str.length)==sub;
}

function string_from_chars( chars:Array ):String{
	var str:String="",i:int;
	for( i=0;i<chars.length;++i ){
		str+=String.fromCharCode( chars[i] );
	}
	return str;
}

