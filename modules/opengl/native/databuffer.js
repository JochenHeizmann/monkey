
// Note: Firefox doesn't support DataView, so we have to kludge...
//
// This means pokes/peeks must be naturally aligned, but data has to be in WebGL anyway so that's OK for now.
//
function DataBuffer( size ){
	this.arrayBuffer=new ArrayBuffer( (size+3)&~3 );
	this.byteArray=new Int8Array( this.arrayBuffer );
	this.shortArray=new Int16Array( this.arrayBuffer );
	this.intArray=new Int32Array( this.arrayBuffer );
	this.floatArray=new Float32Array( this.arrayBuffer );
	this.size=size;
}

DataBuffer.prototype.Size=function(){
	return this.size;
}

DataBuffer.prototype.PokeByte=function( addr,value ){
	this.byteArray[addr]=value;
}

DataBuffer.prototype.PokeShort=function( addr,value ){
	this.shortArray[addr>>1]=value;
}

DataBuffer.prototype.PokeInt=function( addr,value ){
	this.intArray[addr>>2]=value;
}

DataBuffer.prototype.PokeFloat=function( addr,value ){
	this.floatArray[addr>>2]=value;
}

DataBuffer.prototype.Peekbyte=function( addr ){
	return this.byteArray[addr];
}

DataBuffer.prototype.PeekShort=function( addr ){
	return this.shortArray[addr>>1];
}

DataBuffer.prototype.PeekInt=function( addr ){
	return this.intArray[addr>>2];
}

DataBuffer.prototype.PeekFloat=function( addr ){
	return this.floatArray[addr>>2];
}

function createDataBuffer( size ){
	return new DataBuffer( size );
}
