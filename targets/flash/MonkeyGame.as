
package{

	import flash.display.*;
	import flash.events.*;
	import flash.media.*;
	import flash.net.*;
	import flash.utils.ByteArray;

	[SWF(width="800",height="600")]
	[Frame(factoryClass="Preloader")]
	
	public class MonkeyGame extends Sprite{
	
		public var runner:Function;
		
		public function MonkeyGame(){
		
			game=this;

			addEventListener( Event.ADDED_TO_STAGE,onAddedToStage );
		}
		
		private function onAddedToStage( e:Event ):void{
		
			try{
				bbInit();
				bbMain();

				if( runner!=null ) runner();

			}catch( err:Object ){
			
				printError( err.toString() );
			}
		}
		
		private function mungPath( path:String ):String{
		
			if( path.toLowerCase().indexOf("monkey://data/")!=0 ) return "";
			path=path.slice(14);
			
			var i:int=path.indexOf( "." ),ext:String="";
			if( i!=-1 ){
				ext=path.slice(i+1);
				path=path.slice(0,i);
			}

			var munged:String="_";
			var bits:Array=path.split( "/" );
			
			for( i=0;i<bits.length;++i ){
				munged+=bits[i].length+bits[i];
			}
			munged+=ext.length+ext;
			
			return munged;
		}
		
		public function urlRequest( path:String ):URLRequest{
			if( path.toLowerCase().indexOf("monkey://data/")==0 ) path="data/"+path.slice(14);
			return new URLRequest( path );
		}
		
		public function loadByteArray( path:String ):ByteArray{
			path=mungPath( path );
			var t:Class=Assets[path];
			if( t ) return (new t) as ByteArray;
			return null;
		}
		
		public function loadString( path:String ):String{
			var buf:ByteArray=loadByteArray( path );
			if( buf ) return buf.toString();
			return "";
		}

		public function loadBitmap( path:String ):Bitmap{
			path=mungPath( path );
			var t:Class=Assets[path];
			if( t ) return (new t) as Bitmap;
			return null;
		}
		
		public function loadSound( path:String ):Sound{
			path=mungPath( path );
			var t:Class=Assets[path];
			if( t ) return (new t) as Sound;
			return null;
		}
	}		
}

var game:MonkeyGame;

final class Config{
//${CONFIG_BEGIN}
//${CONFIG_END}
}

final class Assets{
//${ASSETS_BEGIN}
//${ASSETS_END}
}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
