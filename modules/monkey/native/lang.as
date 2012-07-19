
// Actionscript Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

//***** ActionScript Runtime *****

var err_info:String="";
var err_stack:Array=[];

var D2R:Number=0.017453292519943295;
var R2D:Number=57.29577951308232;

function push_err():void{
	err_stack.push( err_info );
}

function pop_err():void{
	err_info=err_stack.pop();
}

function stackTrace():String{
	var str:String="";
	push_err();
	err_stack.reverse();
	for( var i:int=0;i<err_stack.length;++i ){
		str+=err_stack[i]+"\n";
	}
	err_stack.reverse();
	pop_err();
	return str;
}

function print( str:String ):void{
	var c:TextField=game.GetConsole();
	c.appendText( str+"\n" );
//	trace( str );	//this causes a funky VerifyError in flash!
}

function alert( str:String ):void{
	var c:TextField=game.GetConsole();
	c.appendText( str+"\n" );
}

function error( str:String ):void{
	throw str;
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

function resize_number_array( arr:Array,len:int ):Array{
   var res:Array=new Array( len );
   var n:int=Math.min( arr.length,len );
   for( var i:int=0;i<n;++i ) res[i]=arr[i];
   for( var j:int=n;j<len;++j ) res[j]=0;
   return res;
}

function resize_string_array( arr:Array,len:int ):Array{
   var res:Array=new Array( len );
   var n:int=Math.min( arr.length,len );
   for( var i:int=0;i<n;++i ) res[i]=arr[i];
   for( var j:int=n;j<len;++j ) res[j]='';
   return res;
}

function resize_array_array( arr:Array,len:int ):Array{
   var res:Array=new Array( len );
   var n:int=Math.min( arr.length,len );
   for( var i:int=0;i<n;++i ) res[i]=arr[i];
   for( var j:int=n;j<len;++j ) res[j]=[];
   return res;
}

function resize_object_array( arr:Array,len:int ):Array{
   var res:Array=new Array( len );
   var n:int=Math.min( arr.length,len );
   for( var i:int=0;i<n;++i ) res[i]=arr[i];
   for( var j:int=n;j<len;++j ) res[j]=null;
   return res;
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
