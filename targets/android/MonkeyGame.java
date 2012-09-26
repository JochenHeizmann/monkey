
//${PACKAGE_BEGIN}
package com.monkey;
//${PACKAGE_END}

import java.net.*;

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
	
	static String filePath( String path ){
		if( !path.startsWith("monkey://") ){
			return path;
		}else if( path.startsWith("monkey://internal/") ){
			File f=MonkeyGame.activity.getFilesDir();
			if( f!=null ) return f+"/"+path.substring(18);
		}else if( path.startsWith("monkey://external/") ){
			File f=MonkeyGame.activity.getExternalFilesDir(null);
			if( f!=null ) return f+"/"+path.substring(18);
		}
		return "";
	}
	
	static String assetPath( String path ){
		if( path.toLowerCase().startsWith("monkey://data/") ) return "monkey/"+path.substring(14);
		return "";
	}
	
	static byte[] loadBytes( String path ){
	
		path=assetPath( path );
		if( path=="" ) return null;
		
		try{
			//Man, they sure don't make this easy for ya do they?!?
			InputStream in=getAssets().open( path );
			
			ByteArrayOutputStream out=new ByteArrayOutputStream(1024);
			byte[] buf=new byte[4096];
			
			for(;;){
				int n=in.read( buf );
				if( n<0 ) break;
				out.write( buf,0,n );
			}
			in.close();
			return out.toByteArray();
			
		}catch( IOException e ){
		}
		return null;
	}

	static String loadString( String path ){
		byte[] bytes=loadBytes( path );
		if( bytes!=null ) return loadString( bytes );
		return "";
	}

	static Bitmap loadBitmap( String path ){

		Bitmap bitmap=null;
		
		BitmapFactory.Options opts=new BitmapFactory.Options();
		opts.inPurgeable=true;
		
		try{
			if( path.toLowerCase().startsWith("monkey://data/") ){
				path=assetPath( path );
				InputStream in=getAssets().open( path );
				bitmap=BitmapFactory.decodeStream( in,null,opts );
				in.close();
			}else{
				URL url=new URL( path );
				URLConnection con=url.openConnection();
				BufferedInputStream in=new BufferedInputStream( con.getInputStream() );
				bitmap=BitmapFactory.decodeStream( in,null,opts );
				in.close();
			}
		}catch( IOException e ){
		}
		return bitmap;
	}

	static int loadSound( String path,SoundPool pool ){
		path=assetPath( path );
		if( path=="" ) return 0;

		try{
			return pool.load( getAssets().openFd( path ),1 );
		}catch( IOException e ){
		}
		return 0;
	}
	
	static MediaPlayer openMedia( String path ){
		path=assetPath( path );
		if( path=="" ) return null;

		try{
			android.content.res.AssetFileDescriptor fd=getAssets().openFd( path );

			MediaPlayer mp=new MediaPlayer();
			mp.setDataSource( fd.getFileDescriptor(),fd.getStartOffset(),fd.getLength() );
			mp.prepare();
			
			fd.close();
			
			return mp;
		}catch( IOException e ){
		}
		return null;
	}
}

//${TRANSCODE_BEGIN}
//${TRANSCODE_END}
