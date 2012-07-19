
Import rockout

Function CenterStringX:Float (string:String)
		 Return (VDeviceWidth () * 0.5) - (string.Length () * FONT_WIDTH * 0.5)
End

Function CenterStringY:Float (string:String)
		 Return (VDeviceHeight () * 0.5) - (FONT_HEIGHT * 0.5)
End

Function CenterText (string:String)
	DrawText string, CenterStringX (string), CenterStringY (string)
End

Function ShowState ()

	'Return
	
		 Select Game.GameState

				   Case STATE_MENU
				   		DrawText "In menu", 0, DeviceHeight () - FONT_HEIGHT

				   Case STATE_LOADLEVEL
				   		DrawText "Loading level...", 0, DeviceHeight () - FONT_HEIGHT

				   Case STATE_PLAYING
				   		DrawText "Playing", 0, DeviceHeight () - FONT_HEIGHT

				   Case STATE_PAUSED
				   		DrawText "Paused", 0, DeviceHeight () - FONT_HEIGHT

				   Default
				   		DrawText "Unknown game state!", 0, DeviceHeight () - FONT_HEIGHT

		 End

End

Function MidHandle (image:Image)
		 image.SetHandle image.Width () * 0.5, image.Height () * 0.5
End

Function LoadCenteredImage:Image (image:String)
		 Local img:Image = LoadImage (image)
		 MidHandle img
		 Return img
End
