
using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;

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

	public static String LoadString( String path ){
		return File.ReadAllText( "/Application/data/"+path );
	}
	
	public static Texture2D LoadTexture2D( String path ){
		return new Texture2D( "/Application/data/"+path,false );
	}

	public static Sound LoadSound( String path ){
		return new Sound( "/Application/data/"+path );
	}
	
	public static Bgm LoadBgm( String path ){
		return new Bgm( "/Application/data/"+path );
	}
	
};

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}

}
