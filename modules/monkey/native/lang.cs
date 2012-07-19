
// C# Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

//using System;
//using System.Collections;

public class bb_std_lang{

	public static String errInfo="";
	public static List<String> errStack=new List<String>();
	
	public const float D2R=0.017453292519943295f;
	public const float R2D=57.29577951308232f;
	
	public static void pushErr(){
		errStack.Add( errInfo );
	}
	
	public static void popErr(){
		errInfo=errStack[ errStack.Count-1 ];
		errStack.RemoveAt( errStack.Count-1 );
	}

	public static String StackTrace(){
		String str=errInfo+"\n";
		for( int i=errStack.Count-1;i>=0;--i ){
			str+=errStack[i]+"\n";
		}
		return str;
	}
	
	public static void Print( String str ){
		Console.WriteLine( str );
	}
	
	public static void Error( String str ){
		throw new Exception( str );
	}
	
	//***** String stuff *****
	
	static public String slice( String str,int from ){
		return slice( str,from,str.Length );
	}
	
	static public String slice( String str,int from,int term ){
		int len=str.Length;
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
		if( term>from ) return str.Substring( from,term-from );
		return "";
	}

	static public String[] split( String str,String sep ){
		if( sep.Length!=0 ){
			int i=0,i2,n=1;
			while( (i2=str.IndexOf( sep,i ))!=-1 ){
				++n;
				i=i2+sep.Length;
			}
			String[] bits=new String[n];
			i=0;
			for( int j=0;j<n;++j ){
				i2=str.IndexOf( sep,i );
				if( i2==-1 ) i2=str.Length;
				bits[j]=slice( str,i,i2 );
				i=i2+sep.Length;
			}
			return bits;
		}
		return null;
	}
	
	//***** Array stuff *****
	
	static public Array slice( Array arr,int from ){
		return slice( arr,from,arr.Length );
	}
	
	static public Array slice( Array arr,int from,int term ){
		int len=arr.Length;
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
		Array res=Array.CreateInstance( arr.GetType().GetElementType(),newlen );
		if( newlen>0 ) Array.Copy( arr,from,res,0,newlen );
		return res;
	}

   static public Array resize( Array arr,int len ){
      Type ty=arr.GetType().GetElementType();
      Array res=Array.CreateInstance( ty,len );
      int n=Math.Min( arr.Length,len );
      if( n>0 ) Array.Copy( arr,res,n );
      return res;
   }

	static public Array concat( Array lhs,Array rhs ){
		Array res=Array.CreateInstance( lhs.GetType().GetElementType(),lhs.Length+rhs.Length );
		Array.Copy( lhs,0,res,0,lhs.Length );
		Array.Copy( rhs,0,res,lhs.Length,rhs.Length );
		return res;
	}
	
	static public int length( Array arr ){
		return arr!=null ? arr.Length : 0;
	}
}
