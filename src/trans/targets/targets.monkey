
Import target

Import html5
Import flash
Import android
Import glfw
Import xna
Import ios
Import stdcpp

Function ValidTargets$()
	Local valid:=New StringStack

	If Html5Target.IsValid() valid.Push "html5"
	If FlashTarget.IsValid() valid.Push "flash"
	If AndroidTarget.IsValid() valid.Push "android"
	If GlfwTarget.IsValid() valid.Push "glfw"
	If XnaTarget.IsValid() valid.Push "xna"
	If IosTarget.IsValid() valid.Push "ios"
	If StdcppTarget.IsValid() valid.Push "stdcpp"
	
	Return valid.Join( " " )
End

Function SelectTarget:Target( target$ )

	If Not (" "+ValidTargets()+" ").Contains( " "+target+" " ) Die "'"+target+"' is not a valid target."
	
	Select target
	Case "html5"
		Return New Html5Target
	Case "flash"
		Return New FlashTarget
	Case "xna"
		Return New XnaTarget
	Case "android"
		Return New AndroidTarget
	Case "ios"
		Return New IosTarget
	Case "glfw"
		Return New GlfwTarget
	Case "stdcpp"
		Return New StdcppTarget
	Default
		Die "Unknown target '"+target+"'"
	End

End
