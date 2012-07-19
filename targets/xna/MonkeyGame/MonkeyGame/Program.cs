
using System;
using System.IO;
using System.IO.IsolatedStorage;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Audio;
using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using Microsoft.Xna.Framework.Media;

#if !WINDOWS_PHONE
using Microsoft.Xna.Framework.Net;
using Microsoft.Xna.Framework.Storage;
using Microsoft.Xna.Framework.GamerServices;
#endif

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
		return content.Load<Texture2D>( "Content/"+path );
	}

	public static SoundEffect LoadSoundEffect( String path,ContentManager content ){
		return content.Load<SoundEffect>( "Content/"+path );
	}
	
};

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
