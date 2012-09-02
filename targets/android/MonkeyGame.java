
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
		StringBuilder out=new StringBuilder();
		
		int t0=n>0 ? buf[0] & 0xff : -1;
		int t1=n>1 ? buf[1] & 0xff : -1;
		
		if( t0==0xfe && t1==0xff ){
			int i=2;
			while( i<n-1 ){
				int x=buf[i++] & 0xff;
				int y=buf[i++] & 0xff;
				out.append( (char)((x<<8)|y) ); 
			}
		}else if( t0==0xff && t1==0xfe ){
			int i=2;
			while( i<n-1 ){
				int x=buf[i++] & 0xff;
				int y=buf[i++] & 0xff;
				out.append( (char)((y<<8)|x) ); 
			}
		}else{
			int t2=n>2 ? buf[2] & 0xff : -1;
			int i=(t0==0xef && t1==0xbb && t2==0xbf) ? 3 : 0;
			boolean fail=false;
			while( i<n ){
				int c=buf[i++] & 0xff;
				if( (c & 0x80)!=0 ){
					if( (c & 0xe0)==0xc0 ){
						if( i>=n || (buf[i] & 0xc0)!=0x80 ){
							fail=true;
							break;
						}
						c=((c & 0x1f)<<6) | (buf[i] & 0x3f);
						i+=1;
					}else if( (c & 0xf0)==0xe0 ){
						if( i+1>=n || (buf[i] & 0xc0)!=0x80 || (buf[i+1] & 0xc0)!=0x80 ){
							fail=true;
							break;
						}
						c=((c & 0x0f)<<12) | ((buf[i] & 0x3f)<<6) | (buf[i+1] & 0x3f);
						i+=2;
					}else{
						fail=true;
						break;
					}
				}
				out.append( (char)c );
			}
			if( fail ){
				return toString( buf );
			}
		}
		return out.toString();
	}
	
	static byte[] loadBytes( String path ){
		path="monkey/"+path;
		
		try{
			AssetFileDescriptor fd=getAssets().openFd( path );
			int size=(int)fd.getLength();
			fd.close();
			
			byte[] bytes=new byte[size];
			InputStream input=getAssets().open( path );
			int n=input.read( bytes,0,size );
			input.close();
			
			if( n==size ) return bytes;
			
		}catch( IOException e ){
		}
		return null;
	}

	static String loadString( String path ){

		byte[] bytes=loadBytes( path );
		if( bytes==null ) return "";
		
		return loadString( bytes );
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
