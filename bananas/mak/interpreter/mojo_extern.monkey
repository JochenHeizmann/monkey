

' Module mojo.graphics
'
' Copyright 2011 Mark Sibly, all rights reserved.
' No warranty implied; use at your own risk.

Extern

Class Image

	Const MidHandle=1
	Const XPadding=2
	Const YPadding=4
	Const XYPadding=XPadding|YPadding
	
	Global DefaultFlags

	Method Width()
	Method Height()
	Method Loaded()
	Method Frames()
	Method Flags()
	Method HandleX#()
	Method HandleY#()
	Method GrabImage:Image( x,y,width,height,frames=1,flags=DefaultFlags )
	Method SetHandle( tx#,ty# )
	Method Discard()

End	

Const AlphaBlend=0
Const AdditiveBlend=1

Function DeviceWidth()
Function DeviceHeight()
Function LoadImage:Image( path$,frameCount=1,flags=Image.DefaultFlags )
Function LoadImage:Image( path$,frameWidth,frameHeight,frameCount,flags=Image.DefaultFlags )
Function SetColor( r#,g#,b# )
Function GetColor#[]()
Function SetAlpha( alpha# )
Function GetAlpha#()
Function SetBlend( blend )
Function GetBlend()
Function SetScissor( x#,y#,width#,height# )
Function GetScissor#[]()
Function SetMatrix( m#[] )
Function SetMatrix( ix#,iy#,jx#,jy#,tx#,ty# )
Function GetMatrix#[]()
Function PushMatrix()
Function PopMatrix()
Function Transform( m#[] )
Function Transform( ix#,iy#,jx#,jy#,tx#,ty# )
Function Translate( x#,y# )
Function Scale( x#,y# )
Function Rotate( angle# )
Function Cls( r#=0,g#=0,b#=0 )
Function DrawPoint( x#,y# )
Function DrawRect( x#,y#,w#,h# )
Function DrawLine( x1#,y1#,x2#,y2# )
Function DrawOval( x#,y#,w#,h# )
Function DrawCircle( x#,y#,r# )
Function DrawEllipse( x#,y#,xr#,yr# )
Function DrawPoly( verts#[] )
Function DrawImage( image:Image,x#,y#,frame=0 )
Function DrawImage( image:Image,x#,y#,rotation#,scaleX#,scaleY#,frame=0 )
Function DrawImageRect( image:Image,x#,y#,srcX,srcY,srcWidth,srcHeight,frame=0 )
Function DrawImageRect( image:Image,x#,y#,srcX,srcY,srcWidth,srcHeight,rotation#,scaleX#,scaleY#,frame=0 )
Function SetFont( font:Image,firstChar=32 )
Function GetFont:Image()
Function TextWidth#( text$ )
Function TextHeight#()
Function DrawText( text$,x#,y#,xalign#=0,yalign#=0 )
Function Transform#[]( coords#[] )
Function InvTransform#[]( coords#[] )
