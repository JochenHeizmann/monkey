
//${PACKAGE_BEGIN}
package com.monkey;
//${PACKAGE_END}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}

class MonkeyData{

	static final boolean HICOLOR_TEXTURES=true;		//true=32 bit textures, false=16 bit

	static AssetManager getAssets(){
		return MonkeyGame.activity.getAssets();
	}
	
	static String loadString( String path ){
		path="monkey/"+path;
		
		try{
			InputStream stream=getAssets().open( path );
			InputStreamReader reader=new InputStreamReader( stream );

			int len;
			char[] chr=new char[4096];
			StringBuffer buffer = new StringBuffer();
			
			while( (len=reader.read(chr))>0 ){
				buffer.append( chr,0,len );
			}
			reader.close();
			
			return buffer.toString();
		
		}catch( IOException e ){
		}
		return "";		
	}

	static Bitmap loadBitmap( String path ){
		path="monkey/"+path;

		try{
			return BitmapFactory.decodeStream( getAssets().open( path ) );
		}catch( IOException e ){
		}
		return null;
	}

	static int loadSound( String path,SoundPool pool ){
		path="monkey/"+path;

		try{
			return pool.load( getAssets().openFd( path ),1 );
		}catch( IOException e ){
		}
		return 0;
	}
	
	static MediaPlayer openMedia( String path ){
		path="monkey/"+path;

		try{
			android.content.res.AssetFileDescriptor afd=getAssets().openFd( path );

			MediaPlayer mp=new MediaPlayer();
			mp.setDataSource( afd.getFileDescriptor(),afd.getStartOffset(),afd.getLength() );
			mp.prepare();
			
			afd.close();
			return mp;
		}catch( IOException e ){
		}
		return null;
	}

};
