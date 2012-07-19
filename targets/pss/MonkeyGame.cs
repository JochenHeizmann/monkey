
using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;

using Sce.Pss.Core;
using Sce.Pss.Core.Environment;
using Sce.Pss.Core.Graphics;
using Sce.Pss.Core.Input;
using Sce.Pss.Core.Audio;
using Sce.Pss.Core.Imaging;

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
