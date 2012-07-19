
package{

	import flash.display.*;
	import flash.events.*;
	import flash.text.*;
	import flash.media.*;
	
	public class MonkeyGame extends Sprite{
	
		public var bitmap:Bitmap;
		public var runner:Function;
		
		private var _console:TextField;
		
		//just a simple text 'overlay'...
		public function GetConsole():TextField{
			if( _console ) return _console;
			_console=new TextField();
			_console.x=0;
			_console.y=0;
			_console.width=bitmap.width;
			_console.height=bitmap.height;
			_console.background=false;
			_console.backgroundColor=0xff000000;
			_console.textColor=0xffffff00;
			bitmap.stage.addChild( _console );
			return _console;
		}
	
		public function OnResize( e:Event ):void{
			bitmap.bitmapData=new BitmapData( stage.stageWidth,stage.stageHeight,false,0xff0000ff );
			bitmap.width=bitmap.bitmapData.width;
			bitmap.height=bitmap.bitmapData.height;
		}
		
		public function MonkeyGame(){
			addEventListener( Event.ADDED_TO_STAGE,MonkeyGameEx );
		}
		
		public function MonkeyGameEx( event:Event ):void{
			removeEventListener( Event.ADDED_TO_STAGE,MonkeyGameEx );

			stage.align=StageAlign.TOP_LEFT;
			stage.scaleMode=StageScaleMode.NO_SCALE;
			
			bitmap=new Bitmap();
			
			bitmap.bitmapData=new BitmapData( 640,480,false,0xff0000ff );
			bitmap.width=bitmap.bitmapData.width;
			bitmap.height=bitmap.bitmapData.height;
		
			addChild( bitmap );
/*			
			stage.addEventListener( Event.RESIZE,OnResize );
			OnResize( null );
*/
			game=this;
			
			try{
				bb_Init();
				bb_Main();
			}catch( ex:Object ){
				return;
			}

			if( runner!=null ){
				runner();
			}
		}
	}
}

import flash.display.*;
import flash.events.*;
import flash.text.*;
import flash.media.*;

internal var game:MonkeyGame;

final class Assets{
//${ASSETS_BEGIN}
//${ASSETS_END}
}

internal function loadString( path:String ):String{
	if( path=="" ) return "";
//${TEXTFILES_BEGIN}
//${TEXTFILES_END}
}

internal function mungPath( path:String ):String{
	var i:int=path.indexOf( "." )
	if( i!=-1 ) path=path.slice(0,i);
	
	var bits:Array=path.split( "/" );
	var munged:String="_";
	
	for( i=0;i<bits.length;++i ){
		munged+=bits[i].length+bits[i];
	}
	return munged;
}

internal function loadBitmap( path:String ):Bitmap{
	
	var t:Class=Assets[ mungPath( path ) ];
	if( t ) return (new t) as Bitmap;
	
	return null;
}

internal function loadSound( path:String ):Sound{
		
	var t:Class=Assets[ mungPath(path) ];
	if( t ) return (new t ) as Sound;

	return null;
}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}