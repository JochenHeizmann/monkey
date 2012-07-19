
'Simple graphics compatibility test
'
'Should behave the same on all targets
Import mojo

Class Test Extends App

	Field image:Image
	
	Method OnCreate()
		image=LoadImage( "RedbrushAlpha.png",1,Image.MidHandle )
		
		SetUpdateRate 60
	End
	
	Method Update()
		If KeyDown( KEY_LMB ) Print "Yes!"
	End

	Field tx#=0,ty#=0

	Method OnRender()
	
		If KeyDown( KEY_RIGHT )
			tx+=.0125
		Else If KeyDown( KEY_LEFT )
			tx-=.0125
		Endif
		If KeyDown( KEY_UP )
			ty-=.0125
		Else If KeyDown( KEY_DOWN )
			ty+=.0125
		Endif
		Translate tx,ty
	
		Cls 0,0,128
	
		Local sz#=Sin(Millisecs*.1)*32
		Local sx=32+sz,sy=32,sw=DeviceWidth-(64+sz*2),sh=DeviceHeight-(64+sz)
		
		SetScissor sx,sy,sw,sh
		
		Cls 255,128,0

		PushMatrix
		
		Scale DeviceWidth/640.0,DeviceHeight/480.0

		Translate 320,240
		Rotate Millisecs/1000.0*12
		Translate -320,-240
		
		SetColor 255,255,0
		DrawRect 32,32,640-64,480-64

		SetColor 0,255,255
		DrawOval 64,64,640-128,480-128

		SetColor 255,0,0
		DrawLine 32,32,640-32,480-32
		DrawLine 640-32,32,32,480-32
		
		If KeyDown( KEY_LMB )
			'SetBlend 1
			SetAlpha Sin(Millisecs*.3)*.5+.5
			DrawImage image,320,240,0
			SetBlend 0
			SetAlpha 1
		Endif
		
		SetColor 255,255,255
		DrawText "The Quick Brown Fox Jumps Over The Lazy Dog",DeviceWidth/2,DeviceHeight/2,.5,.5
		
		PopMatrix
		
		SetScissor 0,0,DeviceWidth,DeviceHeight
		SetColor 255,0,0
		DrawRect 0,0,DeviceWidth,1
		DrawRect DeviceWidth-1,0,1,DeviceHeight
		DrawRect 0,DeviceHeight-1,DeviceWidth,1
		DrawRect 0,0,1,DeviceHeight-1
	End

End

Function Main()

	New Test

End
