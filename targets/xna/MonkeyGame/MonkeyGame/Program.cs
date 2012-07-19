
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
//${CONFIG_BEGIN}
//${CONFIG_END}
}

public class MonkeyData{

	public static String LoadString( String path ){
        try{
			Stream stream=TitleContainer.OpenStream( "Content/monkey/"+path );
			StreamReader reader=new StreamReader( stream );
			String text=reader.ReadToEnd();
			reader.Close();
			return text;
		}catch( Exception ){
		}
		return "";
	}
	
	public static Texture2D LoadTexture2D( String path,ContentManager content ){
		try{
			return content.Load<Texture2D>( "Content/monkey/"+path );
		}catch( Exception ){
		}
		return null;
	}

	public static SoundEffect LoadSoundEffect( String path,ContentManager content ){
		try{
			return content.Load<SoundEffect>( "Content/monkey/"+path );
		}catch( Exception ){
		}
		return null;
	}
	
	public static Song LoadSong( String path,ContentManager content ){
		try{
			return content.Load<Song>( "Content/monkey/"+path );
		}catch( Exception ){
		}
		return null;
	}
	
};

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
