
using System;
using System.IO;
using System.IO.IsolatedStorage;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Globalization;

using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Audio;
using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using Microsoft.Xna.Framework.Media;

public class MonkeyConfig{
#if WINDOWS
	public const int WINDOW_WIDTH=640;
	public const int WINDOW_HEIGHT=480;
	public const bool WINDOW_RESIZABLE=false;
	public const bool WINDOW_FULLSCREEN=false;
#elif XBOX
	public const int WINDOW_WIDTH=640;
	public const int WINDOW_HEIGHT=480;
	public const bool WINDOW_RESIZABLE=false;
	public const bool WINDOW_FULLSCREEN=true;
#elif WINDOWS_PHONE
	public const int WINDOW_WIDTH=640;
	public const int WINDOW_HEIGHT=480;
	public const bool WINDOW_RESIZABLE=false;
	public const bool WINDOW_FULLSCREEN=true;
#endif
}

public class MonkeyData{

	public static String LoadString( String path ){
		if( path=="" ) return "";
//${TEXTFILES_BEGIN}
//${TEXTFILES_END}
	}
	
	public static Texture2D LoadTexture2D( String path,ContentManager content ){
		try{
			return content.Load<Texture2D>( "Content/"+path );
		}catch( Exception ){
		}
		return null;
	}

	public static SoundEffect LoadSoundEffect( String path,ContentManager content ){
		try{
			return content.Load<SoundEffect>( "Content/"+path );
		}catch( Exception ){
		}
		return null;
	}
	
};

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
