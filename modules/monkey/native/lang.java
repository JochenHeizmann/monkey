
// Java Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

import java.lang.Math;
import java.lang.reflect.Array;
import java.util.Vector;

import java.text.NumberFormat;
import java.text.ParseException;

class bb_std_lang{

	//***** Error handling *****

	static String errInfo="";
	static Vector errStack=new Vector();
	
	static float D2R=0.017453292519943295f;
	static float R2D=57.29577951308232f;
	
	static NumberFormat numberFormat=NumberFormat.getInstance();
	
	static void pushErr(){
		errStack.addElement( errInfo );
	}
	
	static void popErr(){
		if( errStack.size()==0 ) throw new Error( "STACK ERROR!" );
		errInfo=(String)errStack.remove( errStack.size()-1 );
	}
	
	static String stackTrace(){
		String str=errInfo+"\n";
		for( int i=errStack.size()-1;i>=0;--i ){
			str+=(String)errStack.elementAt(i)+"\n";
		}
		return str;
	}
	
	static void print( String str ){
		System.out.println( str );
	}
	
	static void error( String str ){
		throw new Error( str );
	}
	
	//***** String stuff *****

	static public String[] stringArray( int n ){
		String[] t=new String[n];
		for( int i=0;i<n;++i ) t[i]="";
		return t;
	}
	
	static String slice( String str,int from ){
		return slice( str,from,str.length() );
	}
	
	static String slice( String str,int from,int term ){
		int len=str.length();
		if( from<0 ){
			from+=len;
			if( from<0 ) from=0;
		}else if( from>len ){
			from=len;
		}
		if( term<0 ){
			term+=len;
		}else if( term>len ){
			term=len;
		}
		if( term>from ) return str.substring( from,term );
		return "";
	}
	
	static public String[] split( String str,String sep ){
		if( sep.length()==0 ){
			String[] bits=new String[str.length()];
			for( int i=0;i<str.length();++i){
				bits[i]=String.valueOf( str.charAt(i) );
			}
			return bits;
		}else{
			int i=0,i2,n=1;
			while( (i2=str.indexOf( sep,i ))!=-1 ){
				++n;
				i=i2+sep.length();
			}
			String[] bits=new String[n];
			i=0;
			for( int j=0;j<n;++j ){
				i2=str.indexOf( sep,i );
				if( i2==-1 ) i2=str.length();
				bits[j]=slice( str,i,i2 );
				i=i2+sep.length();
			}
			return bits;
		}
	}
	
	static public String join( String sep,String[] bits ){
		StringBuffer buf=new StringBuffer();
		for( int i=0;i<bits.length;++i ){
			if( i>0 ) buf.append( sep );
			buf.append( bits[i] );
		}
		return buf.toString();
	}
	
	static public String replace( String str,String find,String rep ){
		int i=0;
		for(;;){
			i=str.indexOf( find,i );
			if( i==-1 ) return str;
			str=str.substring( 0,i )+rep+str.substring( i+find.length() );
			i+=rep.length();
		}
	}
	
	static public String fromChars( int[] chars ){
		int n=chars.length;
		char[] chrs=new char[n];
		for( int i=0;i<n;++i ){
			chrs[i]=(char)chars[i];
		}
		return new String( chrs,0,n );
	}
	
	//***** Array Stuff *****
	
	static Object sliceArray( Object arr,int from ){
		return sliceArray( arr,from,Array.getLength( arr ) );
	}
	
	static Object sliceArray( Object arr,int from,int term ){
		int len=Array.getLength( arr );
		if( from<0 ){
			from+=len;
			if( from<0 ) from=0;
		}else if( from>len ){
			from=len;
		}
		if( term<0 ){
			term+=len;
		}else if( term>len ){
			term=len;
		}
		if( term<from ) term=from;
		int newlen=term-from;
		Object res=Array.newInstance( arr.getClass().getComponentType(),newlen );
		if( newlen>0 ) System.arraycopy( arr,from,res,0,newlen );
		return res;
	}
	
	static Object resizeArray( Object arr,int newlen ){
		int len=Array.getLength( arr );
		Object res=Array.newInstance( arr.getClass().getComponentType(),newlen );
		int n=Math.min( len,newlen );
		if( n>0 ) System.arraycopy( arr,0,res,0,n );
		return res;
	}
	
	static Object[] resizeArrayArray( Object[] arr,int newlen ){
		int i=arr.length;
		arr=(Object[])resizeArray( arr,newlen );
		if( i<newlen ){
			Object empty=Array.newInstance( arr.getClass().getComponentType().getComponentType(),0 );
			while( i<newlen ) arr[i++]=empty;
		}
		return arr;
	}
	
	static String[] resizeStringArray( String[] arr,int newlen ){
		int i=arr.length;
		arr=(String[])resizeArray( arr,newlen );
		while( i<newlen ) arr[i++]="";
		return arr;
	}
	
	static Object concatArrays( Object lhs,Object rhs ){
		int lhslen=Array.getLength( lhs );
		int rhslen=Array.getLength( rhs );
		int len=lhslen+rhslen;
		Object res=Array.newInstance( lhs.getClass().getComponentType(),len );
		if( lhslen>0 ) System.arraycopy( lhs,0,res,0,lhslen );
		if( rhslen>0 ) System.arraycopy( rhs,0,res,lhslen,rhslen );
		return res;
	}
	
	static int arrayLength( Object arr ){
		return arr!=null ? Array.getLength( arr ) : 0;
	}

}
