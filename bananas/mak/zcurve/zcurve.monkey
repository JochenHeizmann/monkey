
Import mojo

Class MyApp Extends App

	Field mw=256,mh=256
	
	Field wx=0,wy=0,ww=8,wh=8
	
	Field min_t1,max_t1
	Field min_t2,max_t2

	Method OnCreate()
	
		SetUpdateRate 15
		
	End
	
	Method OnUpdate()
	
		If KeyDown( KEY_LEFT )
			wx-=1
		Else If KeyDown( KEY_RIGHT )
			wx+=1
		Endif
		
		If KeyDown( KEY_UP )
			wy-=1
		Else If KeyDown( KEY_DOWN )
			wy+=1
		Endif
		
		min_t1=10000000
		max_t1=-10000000
		min_t2=10000000
		max_t2=-10000000
		
		For Local y=wy Until wy+wh
			For Local x=wx Until wx+ww
			
				'linear address for x,y
				Local t1=x Shl 8 | y
				min_t1=Min( t1,min_t1 )
				max_t1=Max( t1,max_t1 )
				
				'interleaved zcurve address for x,y
				Local t2
				For Local i=0 Until 8
					If x Shr i & 1 t2|=1 Shl (i*2+0)
					If y Shr i & 1 t2|=1 Shl (i*2+1)
				Next
				min_t2=Min( t2,min_t2 )
				max_t2=Max( t2,max_t2 )
				
			Next
		Next
		
	End
	
	Method OnRender()
	
		Cls
		
		SetColor 64,64,64
		DrawRect 0,0,mw,mh
		
		SetColor 255,255,0
		DrawRect wx,wy,ww,wh
		
		DrawText "min_t1="+min_t1+", max_t1="+max_t1+", diff="+(max_t1-min_t1),0,mh
		DrawText "min_t2="+min_t2+", max_t2="+max_t2+", diff="+(max_t2-min_t2),0,mh+11
		
	End
	
End

Function Main()

	New MyApp
	
End
