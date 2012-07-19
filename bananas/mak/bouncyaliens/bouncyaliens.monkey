
Import mojo

Const WIDTH#=320'/4
Const HEIGHT#=240'/4

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
	Field sprites:=New Stack<Sprite>

	Method OnCreate()
	
		image=LoadImage( "alien1.png",8,Image.MidHandle )
		
		For Local i=0 Until 10
			sprites.Push New Sprite
		Next
		
		time=Millisecs
		
		SetUpdateRate 60
	End
	
	Method OnUpdate()
		If TouchDown(0) And TouchY(0)<16
			If TouchX(0)<DeviceWidth/2
				If Not sprites.IsEmpty() sprites.Pop
			Else
				sprites.Push New Sprite
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
	
		Cls
		
		PushMatrix 
				
		Scale DeviceWidth/WIDTH,DeviceHeight/HEIGHT
		
		For Local sprite:=Eachin sprites
			DrawImage image,sprite.x,sprite.y,sprite.f
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
