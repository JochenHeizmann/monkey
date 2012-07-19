
//${PACKAGE_BEGIN}
package com.monkey;
//${PACKAGE_END}

//${IMPORTS_BEGIN}
//${IMPORTS_END}

class MonkeyConfig{
//${CONFIG_BEGIN}
//${CONFIG_END}
}

class MonkeyData{

	static AssetManager getAssets(){
		return MonkeyGame.activity.getAssets();
	}

	static String toString( byte[] buf ){
		int n=buf.length;
		char tmp[]=new char[n];
		for( int i=0;i<n;++i ){
			tmp[i]=(char)(buf[i] & 0xff);
		}
		return new String( tmp );
	}
	
	static String loadString( byte[] buf ){
	
		int n=buf.length;
		if( n<3 ) return toString( buf );
		
		StringBuffer out=new StringBuffer();
		
		int i=0;
		int cc=buf[i++] & 0xff;
		int dd=buf[i++] & 0xff;
		
		if( cc==0xfe && dd==0xff ){
			while( i<n-1 ){
				int x=buf[i++] & 0xff;
				int y=buf[i++] & 0xff;
				out.append( (char)((x<<8)|y) ); 
			}
		}else if( cc==0xff && dd==0xfe ){
			while( i<n-1 ){
				int x=buf[i++] & 0xff;
				int y=buf[i++] & 0xff;
				out.append( (char)((y<<8)|x) ); 
			}
		}else{
			int ee=buf[i++] & 0xff;
			if( cc!=0xef || dd!=0xbb || ee!=0xbf ) return toString( buf );
			while( i<n ){
				int c=buf[i++] & 0xff;
				if( c>=128 && i<n ){
					int d=buf[i++] & 0xff;
					if( c>=224 && i<n ){
						int e=buf[i++] & 0xff;
						if( c>=240 ) break;
						c=(c-224)*4096+(d-128)*64+(e-128);
					}else{
						c=(c-192)*64+(d-128);
					}
				}
				out.append( (char)c );
			}
		}
		return out.toString();
	}

	static String loadString( String path ){
		path="monkey/"+path;
		
		try{
			InputStream stream=getAssets().open( path );
			ByteArrayOutputStream buf=new ByteArrayOutputStream();

			int n;
			byte[] tmp=new byte[4096];

			while( (n=stream.read( tmp,0,tmp.length) )!=-1 ){
				buf.write( tmp,0,n );
			}

			buf.flush();
			stream.close();

			return loadString( buf.toByteArray() );
			
//			This doesn't appear to handle BOMs:
//			return new String( buf.toByteArray() );	

		}catch( IOException e ){
		}
		return "";		
	}

	static Bitmap loadBitmap( String path ){
		path="monkey/"+path;

		try{
			BitmapFactory.Options opts = new BitmapFactory.Options(); 
			opts.inPurgeable=true; 
			return BitmapFactory.decodeStream( getAssets().open( path ),null,opts );
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

}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
