
// Note: Firefox doesn't support DataView, so we have to kludge...
//
// This means pokes/peeks must be naturally aligned, but data has to be in WebGL anyway so that's OK for now.
//
function BBDataBuffer(){
	this.arrayBuffer=null;
	this.length=0;
}

BBDataBuffer.prototype._Init=function( buffer,length ){
	this.arrayBuffer=buffer;
	this.length=length;
	this.byteArray=new Int8Array( buffer );
	this.shortArray=new Int16Array( buffer );
	this.intArray=new Int32Array( buffer );
	this.floatArray=new Float32Array( buffer );
}

BBDataBuffer.prototype._New=function( length ){
	if( this.arrayBuffer ) return false;
	
	var buf=new ArrayBuffer( (length+3)&~3 );
	if( !buf ) return false;
	
	this._Init( buf,length );
	
	return true;
}

BBDataBuffer.prototype._Load=function( path ){
	if( this.arrayBuffer ) return false;

	var xhr=openXhr( path );
	if( !xhr ) return false;
		
	if( xhr.overrideMimeType ){
		xhr.overrideMimeType( "text/plain; charset=x-user-defined" );
	}
	xhr.send( null );
	if( (xhr.status!=200) && (xhr.status!=0) ) return false;
	
	var r=xhr.responseText;
	
	this._New( r.length );
	
	for( var i=0;i<r.length;++i ){
		this.byteArray[i]=r.charCodeAt(i);
	}
	return true;
}

BBDataBuffer.prototype.Length=function(){
	return this.length;
}

BBDataBuffer.prototype.Discard=function(){
	if( this.arrayBuffer ){
		this.arrayBuffer=null;
		this.length=0;
		this.byteArray=null;
		this.shortArray=null;
		this.intArray=null;
		this.floatArray=null
	}
}

BBDataBuffer.prototype.PokeByte=function( addr,value ){
	this.byteArray[addr]=value;
}

BBDataBuffer.prototype.PokeShort=function( addr,value ){
	this.shortArray[addr>>1]=value;
}

BBDataBuffer.prototype.PokeInt=function( addr,value ){
	this.intArray[addr>>2]=value;
}

BBDataBuffer.prototype.PokeFloat=function( addr,value ){
	this.floatArray[addr>>2]=value;
}

BBDataBuffer.prototype.PeekByte=function( addr ){
	return this.byteArray[addr];
}

BBDataBuffer.prototype.PeekShort=function( addr ){
	return this.shortArray[addr>>1];
}

BBDataBuffer.prototype.PeekInt=function( addr ){
	return this.intArray[addr>>2];
}

BBDataBuffer.prototype.PeekFloat=function( addr ){
	return this.floatArray[addr>>2];
}
