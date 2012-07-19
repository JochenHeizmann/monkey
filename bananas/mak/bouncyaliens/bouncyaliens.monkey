
Import mojo

Const WIDTH#=320
Const HEIGHT#=240

Class Sprite

	Field x#,vx#
	Field y#,vy#
	Field f#,vf#
	
	Method New()
		x=Rnd( WIDTH )
		y=Rnd( HEIGHT )
		vx=Rnd(1,2)
		If Rnd(1)>=.5 vx=-vx
		vy=Rnd(1,2)
		If Rnd(1)>=.5 vy=-vy
		vf=Rnd( .5,1.5 )
	End
		
	Method Update()
		x+=vx
		If x<0 Or x>=WIDTH vx=-vx
		y+=vy
		If y<0 Or y>=HEIGHT vy=-vy
		f+=vf
		If f>=8 f-=8
	End
	
End

Class MyApp Extends App

	Field time,frames,fps
	Field image:Image
	Field image_trans:Image
	Field image_solid:Image
	Field sprites:=New Stack<Sprite>

	Method OnCreate()
	
		image_trans=LoadImage( "alien1.png",8,Image.MidHandle )
		image_solid=LoadImage( "alien1_solid.png",8,Image.MidHandle )
		
		image=image_trans
		
		For Local i=0 Until 100
			sprites.Push New Sprite
		Next
		
		time=Millisecs
		
		SetUpdateRate 60
	End
	
	Method OnUpdate()
	
		If TouchHit(0)
			If TouchX(0)<DeviceWidth/3
				For Local i=0 Until 25
					If Not sprites.IsEmpty() sprites.Pop
				Next
			Else If TouchX(0)>DeviceWidth-DeviceWidth/3
				For Local i=0 Until 25
					sprites.Push New Sprite
				Next
			Else
				If image=image_trans
					image=image_solid
				Else
					image=image_trans
				Endif
			Endif
		Endif
	
		For Local sprite:=Eachin sprites
			sprite.Update
		Next

	End
	
	Method OnRender()

		frames+=1
		Local e=Millisecs-time
		If e>=1000
			fps=frames
			frames=0
			time+=e
		Endif
	
		Cls 0,128,255
		
		PushMatrix 
				
		Scale DeviceWidth/WIDTH,DeviceHeight/HEIGHT
		
		For Local sprite:=Eachin sprites
			DrawImage image,sprite.x,sprite.y,0,5,5,sprite.f
		Next
		
		PopMatrix
		
		DrawText "[<<]",0,8,0,.5
		DrawText "sprites="+sprites.Length()+", fps="+fps,DeviceWidth/2,8,.5,.5
		DrawText "[>>]",DeviceWidth,8,1,.5

	End

End

Function Main()

	New MyApp
	
End
