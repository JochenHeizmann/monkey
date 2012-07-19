
package{

	import flash.display.*;
	import flash.events.*;
	import flash.media.*;

	[SWF(width="640",height="480")]
	
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

			}catch( err:String ){
			
				showError( err );

			}
		}
		
		private function mungPath( path:String ):String{
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
		
		public function loadString( path:String ):String{
			if( path=="" ) return "";
//${TEXTFILES_BEGIN}
//${TEXTFILES_END}
		}

		public function loadBitmap( path:String ):Bitmap{
			var t:Class=Assets[ mungPath( path ) ];
			if( t ) return (new t) as Bitmap;
			return null;
		}
		
		public function loadSound( path:String ):Sound{
			var t:Class=Assets[ mungPath( path ) ];
			if( t ) return (new t) as Sound;
			return null;
		}
		
	}		
}

var game:MonkeyGame;

final class Assets{
//${ASSETS_BEGIN}
//${ASSETS_END}
}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
