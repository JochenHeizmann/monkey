
Import target

Class StdcppTarget Extends Target

	Function IsValid()
		Select HostOS
		Case "winnt"
			If MINGW_PATH Return True
		Case "macos"
			Return True
		Case "linux"
			Return True
		End
	End

	Method Begin()
		ENV_TARGET="stdcpp"
		ENV_LANG="cpp"
		_trans=New CppTranslator
	End

	Method MakeTarget()
	
		Local main$=LoadString( "main.cpp" )
		main=ReplaceBlock( main,"${TRANSCODE_BEGIN}","${TRANSCODE_END}",transCode )
		SaveString main,"main.cpp"

		If OPT_BUILD

			Local out$="main_"+HostOS
			DeleteFile out
			
			Select ENV_HOST
			Case "macos"
				Execute "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -o "+out+" main.cpp"
			Default
				Execute "g++ -o "+out+" main.cpp"
			End

			If OPT_RUN
				Execute "~q"+RealPath( out )+"~q"
			Endif
		Endif
	End
	
End

