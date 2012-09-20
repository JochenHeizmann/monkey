
class BBThread{

	internal var running:Boolean=false;
	
	public function Start():void{
		Run__UNSAFE__();
	}
	
	public function IsRunning():Boolean{
		return running;
	}
	
	public function Run__UNSAFE__():void{
	}
}
