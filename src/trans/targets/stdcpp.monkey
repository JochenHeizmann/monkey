
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

		If CONFIG_PROFILE main="#define PROFILE 1~n"+main

		SaveString main,"main.cpp"

		If OPT_BUILD

			Local out$="main_"+HostOS
			DeleteFile out
			
			Select ENV_HOST
			Case "macos"
				Select ENV_CONFIG
				Case "release"
					Execute "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -O3 -o "+out+" main.cpp"
				Case "debug"
					Execute "g++ -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3 -o "+out+" main.cpp"
				End
			Default
				Select ENV_CONFIG
				Case "release"
					Execute "g++ -O3 -o "+out+" main.cpp"
				Case "profile"
					Execute "g++ -O3 -o "+out+" main.cpp -lwinmm"
				Case "debug"
					Execute "g++ -o "+out+" main.cpp"
				End
			End

			If OPT_RUN
				Execute "~q"+RealPath( out )+"~q"
			Endif
		Endif
	End
	
End

