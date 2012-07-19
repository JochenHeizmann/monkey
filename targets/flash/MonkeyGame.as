
package{

	import flash.display.*;
	import flash.text.*;
	import flash.events.*;

	public class MonkeyGame extends Sprite{
	
		internal var bitmap:Bitmap;
		
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
			GameMain( bitmap );
		}
	}
}

//${BEGIN_CODE}
//${END_CODE}
