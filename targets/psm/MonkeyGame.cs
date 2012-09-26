
using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using System.Threading;

using Sce.PlayStation.Core;
using Sce.PlayStation.Core.Environment;
using Sce.PlayStation.Core.Graphics;
using Sce.PlayStation.Core.Input;
using Sce.PlayStation.Core.Audio;
using Sce.PlayStation.Core.Imaging;

namespace monkeygame{
	
public class MonkeyConfig{
//${CONFIG_BEGIN}
//${CONFIG_END}
}

public class MonkeyData{

	public static FileStream OpenFile( String path,FileMode mode ){
		if( path.StartsWith( "monkey://internal/" ) ){
			FileStream stream=new FileStream( "/Documents/"+path.Substring(18),mode );
			return stream;
		}
		return null;
	}

	public static String dataPath( String path ){
		if( path.ToLower().StartsWith("monkey://data/") ) return "/Application/data/"+path.Substring(14);
		return "";
	}

	public static byte[] loadBytes( String path ){
		path=dataPath( path );
		return File.ReadAllBytes( path );
	}

	public static String LoadString( String path ){
		path=dataPath( path );
		return File.ReadAllText( path );
	}
	
	public static Texture2D LoadTexture2D( String path ){
		path=dataPath( path );
		return new Texture2D( path,false );
	}

	public static Sound LoadSound( String path ){
		path=dataPath( path );
		return new Sound( path );
	}
	
	public static Bgm LoadBgm( String path ){
		path=dataPath( path );
		return new Bgm( path );
	}
	
};

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}

}
