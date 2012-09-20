
Import databuffer

Import asyncevent

Private

Import thread

#If LANG="js" Or LANG="as"

Import "native/asyncdataloader.${LANG}"

Extern Private

Class AsyncDataLoaderThread Extends BBThread="BBAsyncDataLoaderThread"

	Field _buf:DataBuffer
	Field _path:String
	Field _result:Bool

	Method Run_UNSAFE__:Void()
End

#Else

Private

Class AsyncDataLoaderThread Extends Thread

	Field _buf:DataBuffer
	Field _path:String
	Field _result:Bool
	
	Method Run__UNSAFE__:Void()
		_result=_buf._Load( _path )
	End
End

#Endif

Public

Interface IOnLoadDataComplete
	Method OnLoadDataComplete:Void( data:DataBuffer,path:String,source:AsyncEventSource )
End

Class AsyncDataLoader Extends AsyncDataLoaderThread Implements IAsyncEventSource

	Method New( path:String,onComplete:IOnLoadDataComplete )
		_path=path
		_onComplete=onComplete
	End
	
	Method Start:Void()
		_buf=New DataBuffer
		AddAsyncEventSource Self
		Start		
	End
	
	Private
	
	Field _onComplete:IOnLoadDataComplete
	
	Method UpdateAsyncEvents:Void()
		If IsRunning() Return
		RemoveAsyncEventSource Self
		If Not _result _buf=Null
		_onComplete.OnLoadDataComplete _buf,_path,Self
	End
	
End
