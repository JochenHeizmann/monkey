
// C++ Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <vector>
#include <typeinfo>

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if _MSC_VER
#define snprintf _snprintf
#endif
#endif

#define DEBUG_GC 0	

// ***** gc_object *****

#if DEBUG_GC
int gc_alloced;
int gc_maxalloced;
#endif

struct gc_object;
gc_object *gc_objs;

int gc_total;		//total objects allocated
int gc_marked;	//number of objects marked
int gc_markbit;	//toggling 'marked' bit - 1/0

//queue some gc_marks to prevent ultra recursive stack thrashing
std::vector<gc_object*> gc_marked_queue;

void *gc_malloc( int size ){
	++gc_total;
#if DEBUG_GC
	int *p=(int*)malloc( sizeof(int)+size );
	if( !p ) return 0;
	gc_alloced+=size;
	if( gc_alloced>gc_maxalloced ) gc_maxalloced=gc_alloced;
	*p++=size;
	return p;
#else
	return malloc( size );
#endif
}

void gc_free( void *q ){
	--gc_total;
#if DEBUG_GC
	int *p=(int*)q-1;
	gc_alloced-=*p;
	free( p );
#else
	free( q );
#endif
}

struct gc_object{

	gc_object *succ;
	int flags;

	gc_object(){
		succ=gc_objs;
		gc_objs=this;
		flags=gc_markbit^1;	//starts unmarked
	}

	virtual ~gc_object(){
	}

	virtual void mark(){
	}

	void *operator new( size_t size ){
		return gc_malloc( size );
	}
	
	void operator delete( void *p ){
		gc_free( p );
	}
};

void gc_mark();

void gc_sweep(){

	gc_object **q=&gc_objs;
	
	while( gc_marked!=gc_total ){

		gc_object *p=*q;
		
		if( !p ) { printf( "GC ERROR\n" );fflush( stdout );break; }

		while( (p->flags & 1)==gc_markbit ){
			q=&p->succ;
			p=*q;
			
			if( !p ) { printf( "GC ERROR\n" );fflush( stdout );break; }
		}

		*q=p->succ;
		
		delete p;
	}

	gc_markbit^=1;
	gc_marked=0;
}

void gc_collect(){

#if DEBUG_GC

	static int gc_max;
	if( gc_maxalloced>gc_max ){
		gc_max=gc_maxalloced;
		printf( "gc_max=%i\n",gc_max );fflush( stdout );
	}

	double time=glfwGetTime();

#endif

	gc_mark();
	
	while( !gc_marked_queue.empty() ){
		gc_object *p=gc_marked_queue.back();
		gc_marked_queue.pop_back();
		p->mark();
	}
	
	gc_sweep();
	
#if DEBUG_GC

	time=glfwGetTime()-time;
	int ms=time*1000000;
	printf( "gc_time=%i, gc_total=%i, gc_alloced=%i, gc_maxalloced=%i\n",ms,gc_total,gc_alloced,gc_maxalloced );fflush( stdout );
	
#endif
}

void gc_mark( char t ){
}

void gc_mark( wchar_t t ){
}

void gc_mark( int t ){
}

void gc_mark( float t ){
}

void gc_mark( gc_object *p ){
	if( !p || (p->flags & 1)==gc_markbit ) return;
	p->flags^=1;
	++gc_marked;
	p->mark();
}

void gc_mark_q( gc_object *p ){
	if( !p || (p->flags & 1)==gc_markbit ) return;
	p->flags^=1;
	++gc_marked;
	gc_marked_queue.push_back( p );
}

// Some ugly hacks for interfaces in an attempt to keep plain objects speedy...
//
struct gc_interface{
	virtual ~gc_interface(){}
};

//for fields, elements and globals of type interface
template<class T> struct gc_iptr{
	T *_p;
};

template<class T> void gc_mark( gc_iptr<T> i ){
	gc_mark( dynamic_cast<gc_object*>(i._p) );
}

template<class T> void gc_mark_q( gc_iptr<T> i ){
	gc_mark_q( dynamic_cast<gc_object*>(i._p) );
}

// ***** Monkey Types *****

typedef wchar_t Char;
template<class T> class Array;
class String;
class Object;

// ***** Array *****

template<class T> T *t_memcpy( T *dst,const T *src,int n ){
	memcpy( dst,src,n*sizeof(T) );
	return dst+n;
}

template<class T> T *t_memset( T *dst,int val,int n ){
	memset( dst,val,n*sizeof(T) );
	return dst+n;
}

template<class T> int t_memcmp( const T *x,const T *y,int n ){
	return memcmp( x,y,n*sizeof(T) );
}

template<class T> int t_strlen( const T *p ){
	const T *q=p++;
	while( *q++ ){}
	return q-p;
}

template<class T> T *t_create( int n,T *p ){
//	for( int i=0;i<n;++i ) new( &p[i] ) T();
	t_memset( p,0,n );
	return p+n;
}

template<class T> T *t_create( int n,T *p,const T *q ){
//	for( int i=0;i<n;++i ) new( &p[i] ) T( q[i] );
	t_memcpy( p,q,n );
	return p+n;
}

template<class T> void t_destroy( int n,T *p ){
//	for( int i=0;i<n;++i ) p[i].~String();
}

template<class T> void t_mark( int n,T *p ){
//	for( int i=0;i<n;++i ) gc_mark( p[i] );
}

template<class T> void t_mark( int n,T **p ){
	for( int i=0;i<n;++i ) gc_mark( p[i] );
}

template<class T> void t_mark( int n,gc_iptr<T> *p ){
	for( int i=0;i<n;++i ) gc_mark( p[i] );
}

// Hacks for strings...
//

template<class T> class Array{
public:
	Array():rep( Rep::alloc(0) ){
	}

	//Note: This is *evil* but allows derived class arrays to be cast to base class arrays for use with generics.
	//
	//Not the best solution - the upcast should be in translated code.
	//
	template<class R> Array( const Array<R> &t ):rep( (Rep*)t.rep ){
	}

	Array( int length ):rep( Rep::alloc(length) ){
		t_create( rep->length,rep->data );
	}
	
	Array( const T *p,int length ):rep( Rep::alloc(length) ){
		t_create( rep->length,rep->data,p );
	}
	
	~Array(){
	}

	Array &operator=( const Array &t ){
		rep=t.rep;
		return *this;
	}
	
	int Length()const{ 
		return rep->length; 
	}
	
	T &At( int index ){
		if( index<0 || index>=rep->length ) throw "Array index out of range";
		return rep->data[index]; 
	}
	
	const T &At( int index )const{
		if( index<0 || index>=rep->length ) throw "Array index out of range";
		return rep->data[index]; 
	}
	
	T &operator[]( int index ){ 
		return rep->data[index]; 
	}

	const T &operator[]( int index )const{
		return rep->data[index]; 
	}
	
	Array Slice( int from,int term )const{
		int len=rep->length;
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
		if( term<=from ) return Array();
		return Array( rep->data+from,term-from );
	}

	Array Slice( int from )const{
		return Slice( from,rep->length );
	}
	
	Array Resize( int newlen )const{
		if( newlen<=0 ) return Array();
		int n=rep->length;
		if( newlen<n ) n=newlen;
		Rep *p=Rep::alloc( newlen );
		T *q=p->data;
		q=t_create( n,q,rep->data );
		q=t_create( (newlen-n),q );
		return Array( p );
	}

//private:
	struct Rep : public gc_object{
		int length;
		T data[0];
		
		Rep( int length ):length(length){
		}
		
		~Rep(){
			t_destroy( length,data );
		}
		
		void mark(){
			t_mark( length,data );
		}

		static Rep *alloc( int length ){
			void *p=gc_malloc( sizeof(Rep)+length*sizeof(T) );
			return ::new(p) Rep( length );
		}
	};
	Rep *rep;

	template<class C> friend void gc_mark( Array<C> &t );
	
	Array( Rep *rep ):rep(rep){
	}
};

template<class T> void gc_mark( Array<T> &t ){
	gc_mark( t.rep );
}

// ***** String *****

class String{
public:
	String():rep( Rep::alloc(0) ){
	}
	
	String( const String &t ):rep( t.rep ){
		rep->retain();
	}

	String( int n ){
		char buf[64];
		snprintf( buf,64,"%i",n );
		rep=Rep::alloc( t_strlen(buf) );
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
	}

	String( float n ){
		char buf[64];
		snprintf( buf,64,"%f",n );
		rep=Rep::alloc( t_strlen(buf) );
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
	}

	String( Char ch,int length ):rep( Rep::alloc(length) ){
		for( int i=0;i<length;++i ) rep->data[i]=ch;
	}

	String( const Char *p ):rep( Rep::alloc(t_strlen(p)) ){
		t_memcpy( rep->data,p,rep->length );
	}

	String( const Char *p,int length ):rep( Rep::alloc(length) ){
		t_memcpy( rep->data,p,rep->length );
	}
	
#if __OBJC__	
	String( NSString *nsstr ):rep( Rep::alloc([nsstr length]) ){
		unichar *buf=(unichar*)malloc( rep->length * sizeof(unichar) );
		[nsstr getCharacters:buf range:NSMakeRange(0,rep->length)];
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
		free( buf );
	}
#endif
	
	~String(){
		rep->release();
	}
	
	template<class C> String( const C *p ):rep( Rep::alloc(t_strlen(p)) ){
		for( int i=0;i<rep->length;++i ) rep->data[i]=p[i];
	}
	
	template<class C> String( const C *p,int length ):rep( Rep::alloc(length) ){
		for( int i=0;i<rep->length;++i ) rep->data[i]=p[i];
	}
	
	int Length()const{
		return rep->length;
	}
	
	const Char *Data()const{
		return rep->data;
	}
	
	Char operator[]( int index )const{
		return rep->data[index];
	}
	
	String &operator=( const String &t ){
		t.rep->retain();
		rep->release();
		rep=t.rep;
		return *this;
	}
	
	String &operator+=( const String &t ){
		return operator=( *this+t );
	}
	
	int Compare( const String &t )const{
		int n=rep->length<t.rep->length ? rep->length : t.rep->length;
		for( int i=0;i<n;++i ){
			if( int q=(int)(rep->data[i])-(int)(t.rep->data[i]) ) return q;
		}
		return rep->length-t.rep->length;
	}
	
	bool operator==( const String &t )const{
		return rep->length==t.rep->length && t_memcmp( rep->data,t.rep->data,rep->length )==0;
	}
	
	bool operator!=( const String &t )const{
		return rep->length!=t.rep->length || t_memcmp( rep->data,t.rep->data,rep->length )!=0;
	}
	
	bool operator<( const String &t )const{
		return Compare( t )<0;
	}
	
	bool operator<=( const String &t )const{
		return Compare( t )<=0;
	}
	
	bool operator>( const String &t )const{
		return Compare( t )>0;
	}
	
	bool operator>=( const String &t )const{
		return Compare( t )>=0;
	}
	
	String operator+( const String &t )const{
		if( !rep->length ) return t;
		if( !t.rep->length ) return *this;
		Rep *p=Rep::alloc( rep->length+t.rep->length );
		Char *q=p->data;
		q=t_memcpy( q,rep->data,rep->length );
		q=t_memcpy( q,t.rep->data,t.rep->length );
		return String( p );
	}
	
	int Find( String find,int start=0 )const{
		if( start<0 ) start=0;
		while( start+find.rep->length<=rep->length ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			++start;
		}
		return -1;
	}
	
	int FindLast( String find )const{
		int start=rep->length-find.rep->length;
		while( start>=0 ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			--start;
		}
		return -1;
	}
	
	int FindLast( String find,int start )const{
		if( start>rep->length-find.rep->length ) start=rep->length-find.rep->length;
		while( start>=0 ){
			if( !t_memcmp( rep->data+start,find.rep->data,find.rep->length ) ) return start;
			--start;
		}
		return -1;
	}
	
	String Trim()const{
		int i=0,i2=rep->length;
		while( i<i2 && rep->data[i]<=32 ) ++i;
		while( i2>i && rep->data[i2-1]<=32 ) --i2;
		if( i==0 && i2==rep->length ) return *this;
		return String( rep->data+i,i2-i );
	}

	Array<String> Split( String sep )const{
		if( !sep.rep->length ){
			return Array<String>();
		}
		int i=0,i2,n=1;
		while( (i2=Find( sep,i ))!=-1 ){
			++n;
			i=i2+sep.rep->length;
		}
		Array<String> bits( n );
		if( n==1 ){
			bits[0]=*this;
			return bits;
		}
		i=0;n=0;
		while( (i2=Find( sep,i ))!=-1 ){
			bits[n++]=Slice( i,i2 );
			i=i2+sep.rep->length;
		}
		bits[n]=Slice( i );
		return bits;
	}

	String Join( Array<String> bits )const{
		if( bits.Length()==0 ) return String();
		if( bits.Length()==1 ) return bits[0];
		int newlen=rep->length * (bits.Length()-1);
		for( int i=0;i<bits.Length();++i ){
			newlen+=bits[i].rep->length;
		}
		Rep *p=Rep::alloc( newlen );
		Char *q=p->data;
		q=t_memcpy( q,bits[0].rep->data,bits[0].rep->length );
		for( int i=1;i<bits.Length();++i ){
			q=t_memcpy( q,rep->data,rep->length );
			q=t_memcpy( q,bits[i].rep->data,bits[i].rep->length );
		}
		return String( p );
	}

	String Replace( String find,String repl )const{
		int i=0,i2,newlen=0;
		while( (i2=Find( find,i ))!=-1 ){
			newlen+=(i2-i)+repl.rep->length;
			i=i2+find.rep->length;
		}
		if( !i ) return *this;
		newlen+=rep->length-i;
		Rep *p=Rep::alloc( newlen );
		Char *q=p->data;
		i=0;
		while( (i2=Find( find,i ))!=-1 ){
			q=t_memcpy( q,rep->data+i,i2-i );
			q=t_memcpy( q,repl.rep->data,repl.rep->length );
			i=i2+find.rep->length;
		}
		q=t_memcpy( q,rep->data+i,rep->length-i );
		return String( p );
	}

	String ToLower()const{
		for( int i=0;i<rep->length;++i ){
			Char t=tolower( rep->data[i] );
			if( t==rep->data[i] ) continue;
			Rep *p=Rep::alloc( rep->length );
			Char *q=p->data;
			t_memcpy( q,rep->data,i );
			for( q[i++]=t;i<rep->length;++i ){
				q[i]=tolower( rep->data[i] );
			}
			return String( p );
		}
		return *this;
	}

	String ToUpper()const{
		for( int i=0;i<rep->length;++i ){
			Char t=toupper( rep->data[i] );
			if( t==rep->data[i] ) continue;
			Rep *p=Rep::alloc( rep->length );
			Char *q=p->data;
			t_memcpy( q,rep->data,i );
			for( q[i++]=t;i<rep->length;++i ){
				q[i]=toupper( rep->data[i] );
			}
			return String( p );
		}
		return *this;
	}
	
	bool Contains( String sub )const{
		return Find( sub )!=-1;
	}

	bool StartsWith( String sub )const{
		return sub.rep->length<=rep->length && !t_memcmp( rep->data,sub.rep->data,sub.rep->length );
	}

	bool EndsWith( String sub )const{
		return sub.rep->length<=rep->length && !t_memcmp( rep->data+rep->length-sub.rep->length,sub.rep->data,sub.rep->length );
	}

	String Slice( int from,int term )const{
		int len=rep->length;
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
		if( term<from ) return String();
		if( from==0 && term==len ) return *this;
		return String( rep->data+from,term-from );
	}

	String Slice( int from )const{
		return Slice( from,rep->length );
	}
	
	int ToInt()const{
		return atoi( ToCString<char>() );
	}
	
	float ToFloat()const{
		return atof( ToCString<char>() );
	}
	
	template<class C> C *ToCString()const{

		C *p=&Array<C>( rep->length+1 )[0];
		
		for( int i=0;i<rep->length;++i ) p[i]=rep->data[i];
		p[rep->length]=0;
		return p;
	}

#if __OBJC__	
	NSString *ToNSString()const{
		return [NSString stringWithCharacters:ToCString<unichar>() length:rep->length];
	}
#endif
	
private:
	struct Rep{
		int refs;
		int length;
		Char data[0];
		
		Rep( int length ):refs(1),length(length){
		}
		
		void retain(){
			++refs;
		}
		
		void release(){
			if( --refs || !length ) return;
			free( this );
		}

		static Rep *alloc( int length ){
			if( !length ){
				static Rep null(0);
				return &null;
			}
			void *p=malloc( sizeof(Rep)+length*sizeof(Char) );
			return new(p) Rep( length );
		}
	};
	Rep *rep;
	
	String( Rep *rep ):rep(rep){
	}
};

void gc_mark( String &t ){
}

String *t_create( int n,String *p ){
	for( int i=0;i<n;++i ) new( &p[i] ) String();
	return p+n;
}

String *t_create( int n,String *p,const String *q ){
	for( int i=0;i<n;++i ) new( &p[i] ) String( q[i] );
	return p+n;
}

void t_destroy( int n,String *p ){
	for( int i=0;i<n;++i ) p[i].~String();
}

void t_mark( int n,String *p ){
}

String T( const char *p ){
	return String( p );
}

String T( const wchar_t *p ){
	return String( p );
}

// ***** Object *****

class Object : public gc_object{
public:
	virtual bool Equals( Object *obj ){
		return this==obj;
	}
	
	virtual int Compare( Object *obj ){
		return (char*)this-(char*)obj;
	}
};

// ***** print *****

//**** main ****

int argc;
const char **argv;
const char *errInfo="";
std::vector<const char*> errStack;

float D2R=0.017453292519943295f;
float R2D=57.29577951308232f;

void pushErr(){
	errStack.push_back( errInfo );
}

void popErr(){
	errInfo=errStack.back();
	errStack.pop_back();
}

String StackTrace(){
	String str;
	pushErr();
	for( int i=errStack.size()-1;i>=0;--i ){
		str+=String( errStack[i] )+"\n";
	}
	popErr();
	return str;
}

#if 0	//def _MSC_VER

void Print( String t ){
	OutputDebugString( (t+"\n").ToCString<TCHAR>() );
}

#else

void Print( String t ){
	puts( t.ToCString<char>() );
	fflush( stdout );
}

#endif

void Error( String t ){
	throw t.ToCString<char>();
}

void Die( const char *p ){
	if( !p || !*p ) exit( 0 );
	Print( String(errInfo)+" : Error : "+p );
	Print( String(p)+":\n"+StackTrace() );
	exit( -1 );
}

int bbInit();
int bbMain();

#if _MSC_VER
int seh_call( int(*f)() ){
	__try{
		return f();
	}__except( 1 ){
		switch( GetExceptionCode() ){
		case STATUS_ACCESS_VIOLATION:
			throw "Access violation";
		case STATUS_ILLEGAL_INSTRUCTION:
			throw "Illegal instruction";
		case STATUS_INTEGER_DIVIDE_BY_ZERO:
			throw "Integer divide by zero";
		}
		throw "Unknown SEH exception";
	}
	return -1;
}
#else
void sighandler( int sig  ){
	switch( sig ){
	case SIGILL:throw "Illegal instruction";
	case SIGFPE:throw "Floating point exception";
#if !_WIN32
	case SIGBUS:throw "Bus error";
#endif
	case SIGSEGV:throw "Segmentation violation";
	}
	throw "Unknown exception";
}
#endif

//entry point call by target main()...
//
int bb_std_main( int argc,const char **argv ){
	
	::argc=argc;
	::argv=argv;

#if !_MSC_VER
	signal( SIGILL,sighandler );
	signal( SIGFPE,sighandler );
#if !_WIN32
	signal( SIGBUS,sighandler );
#endif
	signal( SIGSEGV,sighandler );
#endif

	try{

#if _MSC_VER
		seh_call( bbInit );
		seh_call( bbMain );
#else
		bbInit();
		bbMain();
#endif

	}catch( const char *p ){

		Die( p );
	}
	
//	gc_collect();
	
	return 0;
}
