
function BBAsyncDataLoaderThread(){
	BBThread.call(this);
}

BBAsyncDataLoaderThread.prototype=extend_class( BBThread );

BBAsyncDataLoaderThread.prototype.Run__UNSAFE__=function(){

	var thread=this;
	
	var xhr=openXMLHttpRequest( "GET",thread._path,true );

	xhr.responseType='arraybuffer';
	
	xhr.onload=function(e){
		if( this.status==200 || this.status==0 ){
			thread._buf._Init( xhr.response );
			thread._result=true;
		}else{
			thread._result=false;
		}
		thread.Finished();
	}
	
	xhr.onerror=function(e){
		thread._result=false;
		thread.Finished();
	}
	
	xhr.send();
}
