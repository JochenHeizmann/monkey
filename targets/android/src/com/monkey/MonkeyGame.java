
//${TRANSCODE_BEGIN}
//${TRANSCODE_END}

class MonkeyData{

	static String loadString( String path ){
		if( path.compareTo("")==0 ) return "";
//${TEXTFILES_BEGIN}
//${TEXTFILES_END}
	}

	static Bitmap loadBitmap( String path ){
		path="monkey/"+path;
		try{
			return BitmapFactory.decodeStream( MonkeyGame.activity.getAssets().open( path ) );
		}catch( IOException e ){
		}
		return null;
	}

	static int loadSound( String path,SoundPool pool ){
		path="monkey/"+path;
		try{
			return pool.load( MonkeyGame.activity.getAssets().openFd( path ),1 );
		}catch( IOException e ){
		}
		return 0;
	}

};
