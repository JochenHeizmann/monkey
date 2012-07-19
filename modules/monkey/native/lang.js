
// Javascript Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

//***** JavaScript Runtime *****

var err_info="";
var err_stack=[];

var D2R=0.017453292519943295;
var R2D=57.29577951308232;

function push_err(){
	err_stack.push( err_info );
}

function pop_err(){
	err_info=err_stack.pop();
}

function stackTrace(){
	var str="";
	push_err();
	err_stack.reverse();
	for( var i=0;i<err_stack.length;++i ){
		str+=err_stack[i]+"\n";
	}
	err_stack.reverse();
	pop_err();
	return str;
}

function print( str ){
	if( game_console ){
		game_console.value+=str+"\n";
	}
	if( window.console!=undefined ){
		window.console.log( str );
	}
}

function error( err ){
	throw err;
}

function dbg_object( obj ){
	if( obj ) return obj;
	error( "Null object access" );
}

function dbg_array( arr,index ){
	if( index>=0 && index<arr.length ) return arr;
	error( "Array index out of range" );
}

function new_bool_array( len ){
	var arr=Array( len );
	for( var i=0;i<len;++i ) arr[i]=false;
	return arr;
}

function new_number_array( len ){
	var arr=Array( len );
	for( var i=0;i<len;++i ) arr[i]=0;
	return arr;
}

function new_string_array( len ){
	var arr=Array( len );
	for( var i=0;i<len;++i ) arr[i]='';
	return arr;
}

function new_array_array( len ){
	var arr=Array( len );
	for( var i=0;i<len;++i ) arr[i]=[];
	return arr;
}

function new_object_array( len ){
	var arr=Array( len );
	for( var i=0;i<len;++i ) arr[i]=null;
	return arr;
}

function resize_bool_array( arr,len ){
   var res=Array( len );
   var n=Math.min( arr.length,len );
   for( var i=0;i<n;++i ) res[i]=arr[i];
   for( var j=n;j<len;++j ) res[j]=false;
   return res;
}

function resize_number_array( arr,len ){
   var res=Array( len );
   var n=Math.min( arr.length,len );
   for( var i=0;i<n;++i ) res[i]=arr[i];
   for( var j=n;j<len;++j ) res[j]=0;
   return res;
}

function resize_string_array( arr,len ){
   var res=Array( len );
   var n=Math.min( arr.length,len );
   for( var i=0;i<n;++i ) res[i]=arr[i];
   for( var j=n;j<len;++j ) res[j]='';
   return res;
}

function resize_array_array( arr,len ){
   var res=Array( len );
   var n=Math.min( arr.length,len );
   for( var i=0;i<n;++i ) res[i]=arr[i];
   for( var j=n;j<len;++j ) res[j]=[];
   return res;
}

function resize_object_array( arr,len ){
   var res=Array( len );
   var n=Math.min( arr.length,len );
   for( var i=0;i<n;++i ) res[i]=arr[i];
   for( var j=n;j<len;++j ) res[j]=null;
   return res;
}

function string_replace( str,find,rep ){	//no unregex replace all?!?
	var i=0;
	for(;;){
		i=str.indexOf( find,i );
		if( i==-1 ) return str;
		str=str.substring( 0,i )+rep+str.substring( i+find.length );
		i+=rep.length;
	}
}

function string_trim( str ){
	var i=0,i2=str.length;
	while( i<i2 && str.charCodeAt(i)<=32 ) i+=1;
	while( i2>i && str.charCodeAt(i2-1)<=32 ) i2-=1;
	return str.slice( i,i2 );
}

function string_starts_with( str,substr ){
	return substr.length<=str.length && str.slice(0,substr.length)==substr;
}

function string_ends_with( str,substr ){
	return substr.length<=str.length && str.slice(str.length-substr.length,str.length)==substr;
}

function object_downcast( obj,clas ){
	if( obj instanceof clas ) return obj;
	return null;
}

function extend_class( clas ){
	var tmp=function(){};
	tmp.prototype=clas.prototype;
	return new tmp;
}

