
#rem

Very simple 'does it work' opengl test app.

Note: for glfw and ios, you'll need to modify config.h in .build/target folder and change ENABLE_DEPTH_BUFFER to 1.

#end

Import mojo

Import opengl.gles11

Class GLApp Extends App

	Field vbo,ibo	'vertex buffer,index buffer
	Field rot#

	Method OnCreate()
	
		SetUpdateRate 60

	End
	
	Field _inited
	
	Method Init()

		If _inited Return
		_inited=True

		'create buffer objects
		Local bufs[2]
		glGenBuffers 2,bufs
		vbo=bufs[0]
		ibo=bufs[1]

		'create VBO
		Local vtxs:=[-.5,+.5,+.5,+.5,+.5,-.5,-.5,-.5]
		'
		Local vbuf:=DataBuffer.Create( vtxs.Length*4 )
		For Local i=0 Until vtxs.Length
			vbuf.PokeFloat i*4,vtxs[i]
		Next
		glBindBuffer GL_ARRAY_BUFFER,vbo
		glBufferData GL_ARRAY_BUFFER,vbuf.Size(),vbuf,GL_STATIC_DRAW

		'create IBO		
		Local idxs:=[0,1,2,0,2,3]
		'
		Local ibuf:=DataBuffer.Create( idxs.Length*2 )
		For Local i=0 Until idxs.Length
			ibuf.PokeShort i*2,idxs[i]
		Next
		glBindBuffer GL_ELEMENT_ARRAY_BUFFER,ibo
		glBufferData GL_ELEMENT_ARRAY_BUFFER,ibuf.Size(),ibuf,GL_STATIC_DRAW
		
	End
	
	Method OnSuspend()
		Error ""
	End
	
	Method OnRender()

		Init
		
		glViewport 0,0,DeviceWidth,DeviceHeight
	
		glDisable( GL_BLEND );
		
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity
		glFrustumf -1,1,-1,1,1,100
		
		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity
		
		rot+=1
		glRotatef rot,0,0,1

		glClearColor 0,0,.5,1
		glClear GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT
		
		glEnableClientState GL_VERTEX_ARRAY
		glDisableClientState GL_COLOR_ARRAY
		glDisableClientState GL_TEXTURE_COORD_ARRAY

		glBindBuffer GL_ARRAY_BUFFER,vbo
		glVertexPointer 2,GL_FLOAT,0,0
		glBindBuffer GL_ELEMENT_ARRAY_BUFFER,ibo

		glEnable GL_DEPTH_TEST
		glDepthFunc GL_LESS		
		glDepthMask True
		
		glLoadIdentity
		glTranslatef 0,-1,-2
		glRotatef 90,1,0,0
		glScalef 4,1,1
		glColor4f 0,1,0,1
		glDrawElements GL_TRIANGLES,6,GL_UNSIGNED_SHORT,0
		
		glLoadIdentity
		glTranslatef 0,0,-2
		glRotatef rot,0,0,1
		glScalef 1,4,1
		glColor4f 1,1,0,1
		glDrawElements GL_TRIANGLES,6,GL_UNSIGNED_SHORT,0
		
	End
	
End

Function Main()
	New GLApp
End
