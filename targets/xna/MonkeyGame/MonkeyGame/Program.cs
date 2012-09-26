
//Enable this ONLY if you upgrade the Windows Phone project to 7.1!
//#define MANGO

using System;
using System.IO;
using System.IO.IsolatedStorage;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Globalization;
using System.Threading;
using System.Diagnostics;

using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Audio;
using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using Microsoft.Xna.Framework.Media;

#if WINDOWS_PHONE
using Microsoft.Devices.Sensors;
using Microsoft.Xna.Framework.Input.Touch;
#endif

public class MonkeyConfig{
//${CONFIG_BEGIN}
//${CONFIG_END}
}

public class MonkeyData{

	public static FileStream OpenFile( String path,FileMode mode ){
		if( path.StartsWith("monkey://internal/") ){
#if WINDOWS
			IsolatedStorageFile file=IsolatedStorageFile.GetUserStoreForAssembly();
#else
			IsolatedStorageFile file=IsolatedStorageFile.GetUserStoreForApplication();
#endif
			if( file==null ) return null;
			
			IsolatedStorageFileStream stream=file.OpenFile( path.Substring(18),mode );
			
			return stream;
		}
		return null;
	}

	public static String ContentPath( String path ){
		if( path.StartsWith("monkey://data/") ) return "Content/monkey/"+path.Substring(14);
		return "";
	}

	public static byte[] loadBytes( String path ){
		path=ContentPath( path );
		if( path=="" ) return null;
        try{
			Stream stream=TitleContainer.OpenStream( path );
			int len=(int)stream.Length;
			byte[] buf=new byte[len];
			int n=stream.Read( buf,0,len );
			stream.Close();
			if( n==len ) return buf;
		}catch( Exception ){
		}
		return null;
	}
	
	public static String LoadString( String path ){
		path=ContentPath( path );
		if( path=="" ) return "";
        try{
			Stream stream=TitleContainer.OpenStream( path );
			StreamReader reader=new StreamReader( stream );
			String text=reader.ReadToEnd();
			reader.Close();
			return text;
		}catch( Exception ){
		}
		return "";
	}
	
	public static Texture2D LoadTexture2D( String path,ContentManager content ){
		path=ContentPath( path );
		if( path=="" ) return null;
		try{
			return content.Load<Texture2D>( path );
		}catch( Exception ){
		}
		return null;
	}

	public static SoundEffect LoadSoundEffect( String path,ContentManager content ){
		path=ContentPath( path );
		if( path=="" ) return null;
		try{
			return content.Load<SoundEffect>( path );
		}catch( Exception ){
		}
		return null;
	}
	
	public static Song LoadSong( String path,ContentManager content ){
		path=ContentPath( path );
		if( path=="" ) return null;
		try{
			return content.Load<Song>( path );
		}catch( Exception ){
		}
		return null;
	}
	
};

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
