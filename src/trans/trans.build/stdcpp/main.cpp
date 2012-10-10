
#include "main.h"

//${CONFIG_BEGIN}
#define CFG_CD 
#define CFG_CONFIG release
#define CFG_CPP_DOUBLE_PRECISION_FLOATS 1
#define CFG_HOST macos
#define CFG_LANG cpp
#define CFG_MODPATH .;/Applications/Monkey/src/trans;/Applications/Monkey/modules
#define CFG_RELEASE 1
#define CFG_SAFEMODE 0
#define CFG_TARGET stdcpp
#define CFG_TRANSDIR 
//${CONFIG_END}

//${TRANSCODE_BEGIN}

// C++ Monkey runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use at your own risk.

//***** Monkey Types *****

typedef wchar_t Char;
template<class T> class Array;
class String;
class Object;

#if CFG_CPP_DOUBLE_PRECISION_FLOATS
typedef double Float;
#define FLOAT(X) X
#else
typedef float Float;
#define FLOAT(X) X##f
#endif

void dbg_error( const char *p );

#if !_MSC_VER
#define sprintf_s sprintf
#define sscanf_s sscanf
#endif

//***** GC Config *****

//How many objects to mark per update/render
//
#ifndef CFG_CPP_GC_MARK_RATE
#define CFG_CPP_GC_MARK_RATE 2500
#endif

//How much to alloc before GC - set to 0 for continuous GC
//
#ifndef CFG_CPP_GC_TRIGGER
#define CFG_CPP_GC_TRIGGER 4*1024*1024
#endif

//#define DEBUG_GC 1

// ***** GC *****

#if _WIN32

int gc_micros(){
	static int f;
	static LARGE_INTEGER pcf;
	if( !f ){
		if( QueryPerformanceFrequency( &pcf ) && pcf.QuadPart>=1000000L ){
			pcf.QuadPart/=1000000L;
			f=1;
		}else{
			f=-1;
		}
	}
	if( f>0 ){
		LARGE_INTEGER pc;
		if( QueryPerformanceCounter( &pc ) ) return pc.QuadPart/pcf.QuadPart;
		f=-1;
	}
	return 0;// timeGetTime()*1000;
}

#elif __APPLE__

#include <mach/mach_time.h>

int gc_micros(){
	static int f;
	static mach_timebase_info_data_t timeInfo;
	if( !f ){
		mach_timebase_info( &timeInfo );
		timeInfo.denom*=1000L;
		f=1;
	}
	return mach_absolute_time()*timeInfo.numer/timeInfo.denom;
}

#else

int gc_micros(){
	return 0;
}

#endif

//***** New GC *****

#define gc_mark_roots gc_mark

void gc_mark_roots();

struct gc_object;

gc_object *gc_malloc( int size );
void gc_free( gc_object *p );

struct gc_object{
	gc_object *succ;
	gc_object *pred;
	int flags;
	
	virtual ~gc_object(){
	}
	
	virtual void mark(){
	}
	
	void *operator new( size_t size ){
		return gc_malloc( size );
	}
	
	void operator delete( void *p ){
		gc_free( (gc_object*)p );
	}
};

gc_object gc_free_list;
gc_object gc_marked_list;
gc_object gc_unmarked_list;
gc_object gc_queued_list;

int gc_free_bytes;
int gc_marked_objs;
int gc_marked_bytes;
int gc_alloced_bytes;
int gc_max_alloced_bytes;
int gc_markbit=1;

gc_object *gc_cache[8];

#define GC_LIST_EMPTY( LIST ) ((LIST).succ==&(LIST))

#define GC_REMOVE_NODE( NODE ){\
(NODE)->pred->succ=(NODE)->succ;\
(NODE)->succ->pred=(NODE)->pred;}

#define GC_INSERT_NODE( NODE,SUCC ){\
(NODE)->pred=(SUCC)->pred;\
(NODE)->succ=(SUCC);\
(SUCC)->pred->succ=(NODE);\
(SUCC)->pred=(NODE);}

void gc_init1(){
	gc_free_list.succ=gc_free_list.pred=&gc_free_list;
	gc_marked_list.succ=gc_marked_list.pred=&gc_marked_list;
	gc_unmarked_list.succ=gc_unmarked_list.pred=&gc_unmarked_list;
	gc_queued_list.succ=gc_queued_list.pred=&gc_queued_list;
}

void gc_init2(){
	gc_mark_roots();
}

gc_object *gc_malloc( int size ){

	size=(size+7)&~7;
	
	int t=gc_free_bytes-size;
	while( gc_free_bytes && gc_free_bytes>t ){
		gc_object *p=gc_free_list.succ;
		if( !p || p==&gc_free_list ){
			printf("ERROR:p=%p gc_free_bytes=%i\n",p,gc_free_bytes);
			fflush(stdout);
			gc_free_bytes=0;
			break;
		}
		GC_REMOVE_NODE(p);
		delete p;	//...to gc_free
	}
	
	gc_object *p;
	if( size<64 ){
		if( (p=gc_cache[size>>3]) ){
			gc_cache[size>>3]=p->succ;
		}else{
			p=(gc_object*)malloc( size );
		}
	}else{
		p=(gc_object*)malloc( size );
	}
	
	p->flags=size|gc_markbit;
	
	GC_INSERT_NODE( p,&gc_unmarked_list );
	
	gc_alloced_bytes+=size;
	
	if( gc_alloced_bytes>gc_max_alloced_bytes ) gc_max_alloced_bytes=gc_alloced_bytes;
	
	return p;
}

void gc_free( gc_object *p ){

	int size=p->flags & ~7;
	gc_free_bytes-=size;
	
	if( size<64 ){
		p->succ=gc_cache[size>>3];
		gc_cache[size>>3]=p;
	}else{
		free( p );
	}
}

template<class T> void gc_mark( T *t ){

	gc_object *p=dynamic_cast<gc_object*>(t);
	
	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_marked_list );
		gc_marked_bytes+=(p->flags & ~7);
		gc_marked_objs+=1;
		p->mark();
	}
}

template<class T> void gc_mark_q( T *t ){

	gc_object *p=dynamic_cast<gc_object*>(t);
	
	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_queued_list );
	}
}

template<class T,class V> void gc_assign( T *&lhs,V *rhs ){

	gc_object *p=dynamic_cast<gc_object*>(rhs);

	if( p && (p->flags & 3)==gc_markbit ){
		p->flags^=1;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_queued_list );
	}

	lhs=rhs;
}

void gc_collect(){

#if DEBUG_GC
	int us=gc_micros();
#endif

	static int last_alloced;
	
	int sweep=0;

#if CFG_CPP_GC_TRIGGER!=0	
	if( gc_alloced_bytes>last_alloced+CFG_CPP_GC_TRIGGER ){
		sweep=1;
	}
#endif	
	
	int tomark=sweep ? 0x7fffffff : gc_marked_objs+CFG_CPP_GC_MARK_RATE;

	while( !GC_LIST_EMPTY( gc_queued_list ) && gc_marked_objs<tomark ){
		gc_object *p=gc_queued_list.succ;
		GC_REMOVE_NODE( p );
		GC_INSERT_NODE( p,&gc_marked_list );
		gc_marked_bytes+=(p->flags & ~7);
		gc_marked_objs+=1;
		p->mark();
	}

#if CFG_CPP_GC_TRIGGER==0
	if( GC_LIST_EMPTY( gc_queued_list ) ){
		sweep=1;
	}
#endif	
	
	int reclaimed_bytes=-1;
	
	if( sweep && !GC_LIST_EMPTY( gc_unmarked_list ) ){
	
		reclaimed_bytes=gc_alloced_bytes-gc_marked_bytes;
		
		//append unmarked list to end of free list
		gc_object *head=gc_unmarked_list.succ;
		gc_object *tail=gc_unmarked_list.pred;
		gc_object *succ=&gc_free_list;
		gc_object *pred=succ->pred;
		head->pred=pred;
		tail->succ=succ;
		pred->succ=head;
		succ->pred=tail;
		
		//move marked to unmarked.
		gc_unmarked_list=gc_marked_list;
		gc_unmarked_list.succ->pred=gc_unmarked_list.pred->succ=&gc_unmarked_list;
		
		//clear marked.
		gc_marked_list.succ=gc_marked_list.pred=&gc_marked_list;
		
		//adjust sizes
		gc_alloced_bytes=gc_marked_bytes;
		gc_free_bytes+=reclaimed_bytes;
		gc_marked_bytes=0;
		gc_marked_objs=0;
		gc_markbit^=1;
		
		gc_mark_roots();
		
		last_alloced=gc_alloced_bytes;
	}

#if DEBUG_GC
	int us2=gc_micros(),us3=us2-us;
	if( reclaimed_bytes>=0 || us3>=1000 ){
		printf("gc_collect :: us:%i reclaimed:%i alloced_bytes:%i max_alloced_bytes:%i free_bytes:%i\n",us2-us,reclaimed_bytes,gc_alloced_bytes,gc_max_alloced_bytes,gc_free_bytes );
	}		
	fflush(stdout);
#endif
}

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
	t_memset( p,0,n );
	return p+n;
}

template<class T> T *t_create( int n,T *p,const T *q ){
	t_memcpy( p,q,n );
	return p+n;
}

template<class T> void t_destroy( int n,T *p ){
}

//for int, float etc arrays...needs to go before Array<> decl to shut xcode 4.0.2 up.
template<class T> void gc_mark_array( int n,T *p ){
}

template<class T> class Array{
public:
	Array(){
		static Rep null;
		rep=&null;
	}

	//Uses default...
//	Array( const Array<T> &t )...
	
	Array( int length ):rep( Rep::alloc( length ) ){
		t_create( rep->length,rep->data );
	}
	
	Array( const T *p,int length ):rep( Rep::alloc(length) ){
		t_create( rep->length,rep->data,p );
	}
	
	~Array(){
	}

	//Uses default...
//	Array &operator=( const Array &t )...
	
	int Length()const{ 
		return rep->length; 
	}
	
	T &At( int index ){
		if( index<0 || index>=rep->length ) dbg_error( "Array index out of range" );
		return rep->data[index]; 
	}
	
	const T &At( int index )const{
		if( index<0 || index>=rep->length ) dbg_error( "Array index out of range" );
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
	
private:
	struct Rep : public gc_object{
		int length;
		T data[0];
		
		Rep():length(0){
			flags=3;
		}
		
		Rep( int length ):length(length){
		}
		
		~Rep(){
			t_destroy( length,data );
		}
		
		void mark(){
			gc_mark_array( length,data );
		}
		
		static Rep *alloc( int length ){
			static Rep null;
			if( !length ) return &null;
			void *p=gc_malloc( sizeof(Rep)+length*sizeof(T) );
			return ::new(p) Rep( length );
		}
	};
	Rep *rep;
	
	template<class C> friend void gc_mark( Array<C> &t );
	template<class C> friend void gc_mark_q( Array<C> &t );
	template<class C> friend void gc_assign( Array<C> &lhs,Array<C> rhs );
	
	Array( Rep *rep ):rep(rep){
	}
};

template<class T> Array<T> *t_create( int n,Array<T> *p ){
	for( int i=0;i<n;++i ) *p++=Array<T>();
	return p;
}

template<class T> Array<T> *t_create( int n,Array<T> *p,const Array<T> *q ){
	for( int i=0;i<n;++i ) *p++=*q++;
	return p;
}

template<class T> void gc_mark( Array<T> &t ){
	gc_mark( t.rep );
}

template<class T> void gc_mark_q( Array<T> &t ){
	gc_mark_q( t.rep );
}

//for object arrays....
template<class T> void gc_mark_array( int n,T **p ){
	for( int i=0;i<n;++i ) gc_mark( p[i] );
}

//for array arrays...
template<class T> void gc_mark_array( int n,Array<T> *p ){
	for( int i=0;i<n;++i ) gc_mark( p[i] );
}

template<class T> void gc_assign( Array<T> &lhs,Array<T> rhs ){
	gc_mark( rhs.rep );
	lhs=rhs;
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
		char buf[256];
		sprintf_s( buf,"%i",n );
		rep=Rep::alloc( t_strlen(buf) );
		for( int i=0;i<rep->length;++i ) rep->data[i]=buf[i];
	}
	
	String( Float n ){
		char buf[256];
		
		//would rather use snprintf, but it's doing weird things in MingW.
		//
		sprintf_s( buf,"%.17lg",n );
		//
		char *p;
		for( p=buf;*p;++p ){
			if( *p=='.' || *p=='e' ) break;
		}
		if( !*p ){
			*p++='.';
			*p++='0';
			*p=0;
		}

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
			Array<String> bits( rep->length );
			for( int i=0;i<rep->length;++i ){
				bits[i]=String( (Char)(*this)[i],1 );
			}
			return bits;
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
	
	Array<int> ToChars()const{
		Array<int> chars( rep->length );
		for( int i=0;i<rep->length;++i ) chars[i]=rep->data[i];
		return chars;
	}
	
	int ToInt()const{
		return atoi( ToCString<char>() );
	}
	
	Float ToFloat()const{
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

	bool Save( FILE *fp ){
		std::vector<unsigned char> buf;
		Save( buf );
		return buf.size() ? fwrite( &buf[0],1,buf.size(),fp )==buf.size() : true;
	}
	
	void Save( std::vector<unsigned char> &buf ){
	
		Char *p=rep->data;
		Char *e=p+rep->length;
		
		while( p<e ){
			Char c=*p++;
			if( c<0x80 ){
				buf.push_back( c );
			}else if( c<0x800 ){
				buf.push_back( 0xc0 | (c>>6) );
				buf.push_back( 0x80 | (c & 0x3f) );
			}else{
				buf.push_back( 0xe0 | (c>>12) );
				buf.push_back( 0x80 | ((c>>6) & 0x3f) );
				buf.push_back( 0x80 | (c & 0x3f) );
			}
		}
	}
	
	static String FromChars( Array<int> chars ){
		int n=chars.Length();
		Rep *p=Rep::alloc( n );
		for( int i=0;i<n;++i ){
			p->data[i]=chars[i];
		}
		return String( p );
	}

	static String Load( FILE *fp ){
		unsigned char tmp[4096];
		std::vector<unsigned char> buf;
		for(;;){
			int n=fread( tmp,1,4096,fp );
			if( n>0 ) buf.insert( buf.end(),tmp,tmp+n );
			if( n!=4096 ) break;
		}
		return buf.size() ? String::Load( &buf[0],buf.size() ) : String();
	}
	
	static String Load( unsigned char *p,int n ){
	
		unsigned char *e=p+n;
		std::vector<Char> chars;
		
		int t0=n>0 ? p[0] : -1;
		int t1=n>1 ? p[1] : -1;

		if( t0==0xfe && t1==0xff ){
			p+=2;
			while( p<e-1 ){
				int c=*p++;
				chars.push_back( (c<<8)|*p++ );
			}
		}else if( t0==0xff && t1==0xfe ){
			p+=2;
			while( p<e-1 ){
				int c=*p++;
				chars.push_back( (*p++<<8)|c );
			}
		}else{
			int t2=n>2 ? p[2] : -1;
			if( t0==0xef && t1==0xbb && t2==0xbf ) p+=3;
			unsigned char *q=p;
			bool fail=false;
			while( p<e ){
				unsigned int c=*p++;
				if( c & 0x80 ){
					if( (c & 0xe0)==0xc0 ){
						if( p>=e || (p[0] & 0xc0)!=0x80 ){
							fail=true;
							break;
						}
						c=((c & 0x1f)<<6) | (p[0] & 0x3f);
						p+=1;
					}else if( (c & 0xf0)==0xe0 ){
						if( p+1>=e || (p[0] & 0xc0)!=0x80 || (p[1] & 0xc0)!=0x80 ){
							fail=true;
							break;
						}
						c=((c & 0x0f)<<12) | ((p[0] & 0x3f)<<6) | (p[1] & 0x3f);
						p+=2;
					}else{
						fail=true;
						break;
					}
				}
				chars.push_back( c );
			}
			if( fail ){
				puts( "-- Invalid UTF-8!" );fflush( stdout );
				return String( q,n );
			}
		}
		return chars.size() ? String( &chars[0],chars.size() ) : String();
	}
	
private:
	struct Rep{
		int refs;
		int length;
		Char data[0];
		
		Rep():refs(1),length(0){
		}
		
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
				static Rep null;
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
	
	virtual String debug(){
		return "+Object\n";
	}
};

class ThrowableObject : public Object{
};

struct gc_interface{
	virtual ~gc_interface(){}
};


//***** Debugger *****

int Print( String t );

#define dbg_stream stderr

#if _MSC_VER
#define dbg_typeof decltype
#else
#define dbg_typeof __typeof__
#endif 

struct dbg_func;
struct dbg_var_type;

static int dbg_suspend;
static int dbg_stepmode;

const char *dbg_info;
String dbg_exstack;

static void *dbg_var_buf[65536*3];
static void **dbg_var_ptr=dbg_var_buf;

static dbg_func *dbg_func_buf[1024];
static dbg_func **dbg_func_ptr=dbg_func_buf;

String dbg_type( bool *p ){
	return "Bool";
}

String dbg_type( int *p ){
	return "Int";
}

String dbg_type( Float *p ){
	return "Float";
}

String dbg_type( String *p ){
	return "String";
}

template<class T> String dbg_type( T *p ){
	return "Object";
}

template<class T> String dbg_type( Array<T> *p ){
	return dbg_type( &(*p)[0] )+"[]";
}

String dbg_value( bool *p ){
	return *p ? "True" : "False";
}

String dbg_value( int *p ){
	return String( *p );
}

String dbg_value( Float *p ){
	return String( *p );
}

String dbg_value( String *p ){
	String t=*p;
	if( t.Length()>100 ) t=t.Slice( 0,100 )+"...";
	t=t.Replace( "\"","~q" );
	t=t.Replace( "\t","~t" );
	t=t.Replace( "\n","~n" );
	t=t.Replace( "\r","~r" );
	return String("\"")+t+"\"";
}

template<class T> String dbg_value( T *t ){
	Object *p=dynamic_cast<Object*>( *t );
	char buf[64];
	sprintf_s( buf,"%p",p );
	return String("@") + (buf[0]=='0' && buf[1]=='x' ? buf+2 : buf );
}

template<class T> String dbg_value( Array<T> *p ){
	String t="[";
	int n=(*p).Length();
	for( int i=0;i<n;++i ){
		if( i ) t+=",";
		t+=dbg_value( &(*p)[i] );
	}
	return t+"]";
}

template<class T> String dbg_decl( const char *id,T *ptr ){
	return String( id )+":"+dbg_type(ptr)+"="+dbg_value(ptr)+"\n";
}

struct dbg_var_type{
	virtual String type( void *p )=0;
	virtual String value( void *p )=0;
};

template<class T> struct dbg_var_type_t : public dbg_var_type{

	String type( void *p ){
		return dbg_type( (T*)p );
	}
	
	String value( void *p ){
		return dbg_value( (T*)p );
	}
	
	static dbg_var_type_t<T> info;
};
template<class T> dbg_var_type_t<T> dbg_var_type_t<T>::info;

struct dbg_blk{
	void **var_ptr;
	
	dbg_blk():var_ptr(dbg_var_ptr){
		if( dbg_stepmode=='l' ) --dbg_suspend;
	}
	
	~dbg_blk(){
		if( dbg_stepmode=='l' ) ++dbg_suspend;
		dbg_var_ptr=var_ptr;
	}
};

struct dbg_func : public dbg_blk{
	const char *id;
	const char *info;

	dbg_func( const char *p ):id(p),info(dbg_info){
		*dbg_func_ptr++=this;
		if( dbg_stepmode=='s' ) --dbg_suspend;
	}
	
	~dbg_func(){
		if( dbg_stepmode=='s' ) ++dbg_suspend;
		--dbg_func_ptr;
		dbg_info=info;
	}
};

int dbg_print( String t ){
	static char *buf;
	static int len;
	int n=t.Length();
	if( n+100>len ){
		len=n+100;
		free( buf );
		buf=(char*)malloc( len );
	}
	buf[n]='\n';
	for( int i=0;i<n;++i ) buf[i]=t[i];
	fwrite( buf,n+1,1,dbg_stream );
	fflush( dbg_stream );
	return 0;
}

void dbg_callstack(){

	void **var_ptr=dbg_var_buf;
	dbg_func **func_ptr=dbg_func_buf;
	
	while( var_ptr!=dbg_var_ptr ){
		while( func_ptr!=dbg_func_ptr && var_ptr==(*func_ptr)->var_ptr ){
			const char *id=(*func_ptr++)->id;
			const char *info=func_ptr!=dbg_func_ptr ? (*func_ptr)->info : dbg_info;
			fprintf( dbg_stream,"+%s;%s\n",id,info );
		}
		void *vp=*var_ptr++;
		const char *nm=(const char*)*var_ptr++;
		dbg_var_type *ty=(dbg_var_type*)*var_ptr++;
		dbg_print( String(nm)+":"+ty->type(vp)+"="+ty->value(vp) );
	}
	while( func_ptr!=dbg_func_ptr ){
		const char *id=(*func_ptr++)->id;
		const char *info=func_ptr!=dbg_func_ptr ? (*func_ptr)->info : dbg_info;
		fprintf( dbg_stream,"+%s;%s\n",id,info );
	}
}

String dbg_stacktrace(){
	if( !dbg_info || !dbg_info[0] ) return "";
	String str=String( dbg_info )+"\n";
	dbg_func **func_ptr=dbg_func_ptr;
	if( func_ptr==dbg_func_buf ) return str;
	while( --func_ptr!=dbg_func_buf ){
		str+=String( (*func_ptr)->info )+"\n";
	}
	return str;
}

void dbg_throw( const char *err ){
	dbg_exstack=dbg_stacktrace();
	throw err;
}

void dbg_stop(){

#ifdef TARGET_OS_IPHONE
	dbg_throw( "STOP" );
#endif

	fprintf( dbg_stream,"{{~~%s~~}}\n",dbg_info );
	dbg_callstack();
	dbg_print( "" );
	
	for(;;){

		char buf[256];
		char *e=fgets( buf,256,stdin );
		if( !e ) exit( -1 );
		
		e=strchr( buf,'\n' );
		if( !e ) exit( -1 );
		
		*e=0;
		
		Object *p;
		
		switch( buf[0] ){
		case '?':
			break;
		case 'r':	//run
			dbg_suspend=0;		
			dbg_stepmode=0;
			return;
		case 's':	//step
			dbg_suspend=1;
			dbg_stepmode='s';
			return;
		case 'e':	//enter func
			dbg_suspend=1;
			dbg_stepmode='e';
			return;
		case 'l':	//leave block
			dbg_suspend=0;
			dbg_stepmode='l';
			return;
		case '@':	//dump object
			p=0;
			sscanf_s( buf+1,"%p",&p );
			if( p ){
				dbg_print( p->debug() );
			}else{
				dbg_print( "" );
			}
			break;
		case 'q':	//quit!
			exit( 0 );
			break;			
		default:
			printf( "????? %s ?????",buf );fflush( stdout );
			exit( -1 );
		}
	}
}

void dbg_error( const char *err ){

#ifdef TARGET_OS_IPHONE
	dbg_throw( err );
#endif

	for(;;){
		Print( String("Monkey Runtime Error : ")+err );
		Print( dbg_stacktrace() );
		dbg_stop();
	}
}

#define DBG_INFO(X) dbg_info=(X);if( dbg_suspend>0 ) dbg_stop();

#define DBG_ENTER(P) dbg_func _dbg_func(P);

#define DBG_BLOCK(T) dbg_blk _dbg_blk;

#define DBG_GLOBAL( ID,NAME )	//TODO!

#define DBG_LOCAL( ID,NAME )\
*dbg_var_ptr++=&ID;\
*dbg_var_ptr++=(void*)NAME;\
*dbg_var_ptr++=&dbg_var_type_t<dbg_typeof(ID)>::info;

//**** main ****

int argc;
const char **argv;

Float D2R=0.017453292519943295f;
Float R2D=57.29577951308232f;

int Print( String t ){
	static char *buf;
	static int len;
	int n=t.Length();
	if( n+100>len ){
		len=n+100;
		free( buf );
		buf=(char*)malloc( len );
	}
	for( int i=0;i<n;++i ) buf[i]=t[i];
	buf[n]=0;
	puts( buf );
	fflush( stdout );
	return 0;
}

int Error( String err ){
	if( !err.Length() ) exit( 0 );
	dbg_error( err.ToCString<char>() );
	return 0;
}

int DebugLog( String t ){
	Print( t );
	return 0;
}

int DebugStop(){
	dbg_stop();
	return 0;
}

int bbInit();
int bbMain();

#if _MSC_VER

static void _cdecl seTranslator( unsigned int ex,EXCEPTION_POINTERS *p ){

	switch( ex ){
	case EXCEPTION_ACCESS_VIOLATION:dbg_error( "Memory access violation" );
	case EXCEPTION_ILLEGAL_INSTRUCTION:dbg_error( "Illegal instruction" );
	case EXCEPTION_INT_DIVIDE_BY_ZERO:dbg_error( "Integer divide by zero" );
	case EXCEPTION_STACK_OVERFLOW:dbg_error( "Stack overflow" );
	}
	dbg_error( "Unknown exception" );
}

#else

void sighandler( int sig  ){
	switch( sig ){
	case SIGSEGV:dbg_error( "Memory access violation" );
	case SIGILL:dbg_error( "Illegal instruction" );
	case SIGFPE:dbg_error( "Floating point exception" );
#if !_WIN32
	case SIGBUS:dbg_error( "Bus error" );
#endif	
	}
	dbg_error( "Unknown signal" );
}

#endif

//entry point call by target main()...
//
int bb_std_main( int argc,const char **argv ){

	::argc=argc;
	::argv=argv;
	
#if _MSC_VER

	_set_se_translator( seTranslator );

#else
	
	signal( SIGSEGV,sighandler );
	signal( SIGILL,sighandler );
	signal( SIGFPE,sighandler );
#if !_WIN32
	signal( SIGBUS,sighandler );
#endif

#endif

	gc_init1();

	bbInit();
	
	gc_init2();

	bbMain();

	return 0;
}

// Stdcpp trans.system runtime.
//
// Placed into the public domain 24/02/2011.
// No warranty implied; use as your own risk.

#if _WIN32

/*
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
*/

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

typedef wchar_t OS_CHAR;
typedef struct _stat stat_t;

#define mkdir( X,Y ) _wmkdir( X )
#define rmdir _wrmdir
#define remove _wremove
#define rename _wrename
#define stat _wstat
#define _fopen _wfopen
#define putenv _wputenv
#define getenv _wgetenv
#define system _wsystem
#define chdir _wchdir
#define getcwd _wgetcwd
#define realpath(X,Y) _wfullpath( Y,X,PATH_MAX )	//Note: first args SWAPPED to be posix-like!
#define opendir _wopendir
#define readdir _wreaddir
#define closedir _wclosedir
#define DIR _WDIR
#define dirent _wdirent

#elif __APPLE__

/*
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <dirent.h>
#include <copyfile.h>
*/

typedef char OS_CHAR;
typedef struct stat stat_t;

#define _fopen fopen

#elif __linux

/*
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
*/

typedef char OS_CHAR;
typedef struct stat stat_t;

#define _fopen fopen

#endif

static String _appPath;
static Array<String> _appArgs;

static char *C_STR( const String &t ){
	return t.ToCString<char>();
}

static OS_CHAR *OS_STR( const String &t ){
	return t.ToCString<OS_CHAR>();
}

String HostOS(){
#if _WIN32
	return "winnt";
#elif __APPLE__
	return "macos";
#elif __linux
	return "linux";
#else
	return "";
#endif
}

String RealPath( String path ){
	std::vector<OS_CHAR> buf( PATH_MAX+1 );
	realpath( OS_STR( path ),&buf[0] );
	buf[buf.size()-1]=0;
	for( int i=0;i<PATH_MAX && buf[i];++i ){
		if( buf[i]=='\\' ) buf[i]='/';
		
	}
	return String( &buf[0] );
}

String AppPath(){

	if( _appPath.Length() ) return _appPath;
	
#if _WIN32

	OS_CHAR buf[PATH_MAX+1];
	GetModuleFileNameW( GetModuleHandleW(0),buf,PATH_MAX );
	buf[PATH_MAX]=0;
	_appPath=String( buf );
	
#elif __APPLE__

	char buf[PATH_MAX];
	uint32_t size=sizeof( buf );
	_NSGetExecutablePath( buf,&size );
	buf[PATH_MAX-1]=0;
	_appPath=String( buf );
	
#elif __linux

	char lnk[PATH_MAX],buf[PATH_MAX];
	pid_t pid=getpid();
	sprintf( lnk,"/proc/%i/exe",pid );
	int i=readlink( lnk,buf,PATH_MAX );
	if( i>0 && i<PATH_MAX ){
		buf[i]=0;
		_appPath=String( buf );
	}

#endif

	_appPath=RealPath( _appPath );
	return _appPath;
}

Array<String> AppArgs(){
	if( _appArgs.Length() ) return _appArgs;
	_appArgs=Array<String>( argc );
	for( int i=0;i<argc;++i ){
		_appArgs[i]=String( argv[i] );
	}
	return _appArgs;
}
	
int FileType( String path ){
	stat_t st;
	if( stat( OS_STR(path),&st ) ) return 0;
	switch( st.st_mode & S_IFMT ){
	case S_IFREG : return 1;
	case S_IFDIR : return 2;
	}
	return 0;
}

int FileSize( String path ){
	stat_t st;
	if( stat( OS_STR(path),&st ) ) return -1;
	return st.st_size;
}

int FileTime( String path ){
	stat_t st;
	if( stat( OS_STR(path),&st ) ) return -1;
	return st.st_mtime;
}

String LoadString( String path ){
	if( FILE *fp=_fopen( OS_STR(path),OS_STR("rb") ) ){
//		Print( String( "LoadString:" )+path );
		String str=String::Load( fp );
		fclose( fp );
		return str;
	}
	printf( "FOPEN 'rb' for LoadString '%s' failed\n",C_STR( path ) );fflush( stdout );
	return "";
}
	
int SaveString( String str,String path ){
	if( FILE *fp=_fopen( OS_STR(path),OS_STR("wb") ) ){
		bool ok=str.Save( fp );
		fclose( fp );
		return ok ? 0 : -2;
	}else{
		printf( "FOPEN 'wb' for SaveString '%s' failed\n",C_STR( path ) );
		fflush( stdout );
	}
	return -1;
}

Array<String> LoadDir( String path ){
	std::vector<String> files;
	
#if _WIN32

	WIN32_FIND_DATAW filedata;
	HANDLE handle=FindFirstFileW( OS_STR(path+"/*"),&filedata );
	if( handle!=INVALID_HANDLE_VALUE ){
		do{
			String f=filedata.cFileName;
			if( f=="." || f==".." ) continue;
			files.push_back( f );
		}while( FindNextFileW( handle,&filedata ) );
		FindClose( handle );
	}else{
		printf( "FindFirstFileW for LoadDir(%s) failed\n",C_STR(path) );
		fflush( stdout );
	}
	
#else

	if( DIR *dir=opendir( OS_STR(path) ) ){
		while( dirent *ent=readdir( dir ) ){
			String f=ent->d_name;
			if( f=="." || f==".." ) continue;
			files.push_back( f );
		}
		closedir( dir );
	}else{
		printf( "opendir for LoadDir(%s) failed\n",C_STR(path) );
		fflush( stdout );
	}

#endif

	return files.size() ? Array<String>( &files[0],files.size() ) : Array<String>();
}
	
int CopyFile( String srcpath,String dstpath ){

#if _WIN32

	if( CopyFileW( OS_STR(srcpath),OS_STR(dstpath),FALSE ) ) return 1;
	return 0;
	
#elif __APPLE__

	// Would like to use COPY_ALL here, but it breaks trans on MacOS - produces weird 'pch out of date' error with copied projects.
	//
	// Ranlib strikes back!
	//
	if( copyfile( OS_STR(srcpath),OS_STR(dstpath),0,COPYFILE_DATA )>=0 ) return 1;
	return 0;
	
#else

	int err=-1;
	if( FILE *srcp=_fopen( OS_STR( srcpath ),OS_STR( T("rb") ) ) ){
		err=-2;
		if( FILE *dstp=_fopen( OS_STR( dstpath ),OS_STR( T("wb") ) ) ){
			err=0;
			char buf[1024];
			while( int n=fread( buf,1,1024,srcp ) ){
				if( fwrite( buf,1,n,dstp )!=n ){
					err=-3;
					break;
				}
			}
			fclose( dstp );
		}else{
			printf( "FOPEN 'wb' for CopyFile(%s,%s) failed\n",C_STR(srcpath),C_STR(dstpath) );
			fflush( stdout );
		}
		fclose( srcp );
	}else{
		printf( "FOPEN 'rb' for CopyFile(%s,%s) failed\n",C_STR(srcpath),C_STR(dstpath) );
		fflush( stdout );
	}
	return err==0;
	
#endif
}

int ChangeDir( String path ){
	return chdir( OS_STR(path) );
}

String CurrentDir(){
	std::vector<OS_CHAR> buf( PATH_MAX+1 );
	getcwd( &buf[0],buf.size() );
	buf[buf.size()-1]=0;
	return String( &buf[0] );
}

int CreateDir( String path ){
	mkdir( OS_STR( path ),0777 );
	return FileType(path)==2;
}

int DeleteDir( String path ){
	rmdir( OS_STR(path) );
	return FileType(path)==0;
}

int DeleteFile( String path ){
	remove( OS_STR(path) );
	return FileType(path)==0;
}

int SetEnv( String name,String value ){
	return putenv( OS_STR( String(name)+T("=")+String(value) ) );
}

String GetEnv( String name ){
	if( OS_CHAR *p=getenv( OS_STR(name) ) ) return String( p );
	return T("");
}

int Execute( String cmd ){

#if _WIN32

	cmd=T("cmd /S /C \"")+cmd+T("\"");

	PROCESS_INFORMATION pi={0};
	STARTUPINFOW si={sizeof(si)};

	if( !CreateProcessW( 0,(WCHAR*)OS_STR(cmd),0,0,1,CREATE_DEFAULT_ERROR_MODE,0,0,&si,&pi ) ) return -1;

	WaitForSingleObject( pi.hProcess,INFINITE );

	int res=GetExitCodeProcess( pi.hProcess,(DWORD*)&res ) ? res : -1;

	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	return res;

#else

	return system( OS_STR(cmd) );

#endif
}

int ExitApp( int retcode ){
	exit( retcode );
	return 0;
}

//Note: OS_CHAR and OS_STR are declared in os file os.cpp

int info_width;
int info_height;

int get_info_png( String path ){

	if( FILE *f=_fopen( OS_STR(path),OS_STR("rb") ) ){
		unsigned char data[32];
		int n=fread( data,1,24,f );
		fclose( f );
		if( n==24 && data[1]=='P' && data[2]=='N' && data[3]=='G' ){
			info_width=(data[16]<<24)|(data[17]<<16)|(data[18]<<8)|(data[19]);
			info_height=(data[20]<<24)|(data[21]<<16)|(data[22]<<8)|(data[23]);
			return 0;
		}
	}
	return -1;
}

int get_info_gif( String path ){

	if( FILE *f=_fopen( OS_STR(path),OS_STR("rb") ) ){
		unsigned char data[32];
		int n=fread( data,1,10,f );
		fclose( f );
		if( n==10 && data[0]=='G' && data[1]=='I' && data[2]=='F' ){
			info_width=(data[7]<<8)|data[6];
			info_height=(data[9]<<8)|data[8];
			return 0;
		}
	}
	return -1;
}

int get_info_jpg( String path ){

	if( FILE *f=_fopen( OS_STR(path),OS_STR("rb") ) ){
	
		unsigned char buf[32];
		
		if( fread( buf,1,2,f )==2 && buf[0]==0xff && buf[1]==0xd8 ){
		
			for(;;){
		
				while( fread( buf,1,1,f )==1 && buf[0]!=0xff ){}
				if( feof( f ) ) break;
				
				while( fread( buf,1,1,f )==1 && buf[0]==0xff ){}
				if( feof( f ) ) break;
				
				int marker=buf[0];
				
				switch( marker ){
				case 0xD0:case 0xD1:case 0xD2:case 0xD3:case 0xD4:case 0xD5:
				case 0xD6:case 0xD7:case 0xD8:case 0xD9:case 0x00:case 0xFF:

					break;
					
				default:
					if( fread( buf,1,2,f )==2 ){
					
						int datalen=((buf[0]<<8)|buf[1])-2;
						
						switch( marker ){
						case 0xC0:case 0xC1:case 0xC2:case 0xC3:
							if( datalen && fread( buf,1,5,f )==5 ){
								int bpp=buf[0];
								info_height=(buf[1]<<8)|buf[2];
								info_width=(buf[3]<<8)|buf[4];
								fclose( f );
								return 0;
							}
						}
						
						if( fseek( f,datalen,SEEK_CUR )<0 ){
							fclose( f );
							return -1;
						}
						
					}else{
						fclose( f );
						return -1;
					}
				}
			}
		}
		fclose( f );
	}
	return -1;
}
class bb_map_Map;
class bb_map_StringMap;
class bb_map_Node;
class bb_stack_Stack;
class bb_stack_StringStack;
class bb_target_Target;
class bb_html5_Html5Target;
class bb_flash_FlashTarget;
class bb_android_AndroidTarget;
class bb_glfw_GlfwTarget;
class bb_xna_XnaTarget;
class bb_ios_IosTarget;
class bb_stdcpp_StdcppTarget;
class bb_psm_PsmTarget;
class bb_win8_Win8Target;
class bb_decl_Decl;
class bb_decl_ScopeDecl;
class bb_decl_AppDecl;
class bb_toker_Toker;
class bb_set_Set;
class bb_set_StringSet;
class bb_map_Map2;
class bb_map_StringMap2;
class bb_map_Node2;
class bb_type_Type;
class bb_type_BoolType;
class bb_map_NodeEnumerator;
class bb_decl_ValDecl;
class bb_decl_ConstDecl;
class bb_type_StringType;
class bb_expr_Expr;
class bb_expr_ConstExpr;
class bb_type_NumericType;
class bb_type_IntType;
class bb_type_FloatType;
class bb_list_List;
class bb_list_Node;
class bb_list_HeadNode;
class bb_decl_BlockDecl;
class bb_decl_FuncDecl;
class bb_list_List2;
class bb_decl_FuncDeclList;
class bb_list_Node2;
class bb_list_HeadNode2;
class bb_list_List3;
class bb_list_Node3;
class bb_list_HeadNode3;
class bb_parser_Parser;
class bb_decl_ModuleDecl;
class bb_expr_UnaryExpr;
class bb_expr_ArrayExpr;
class bb_stack_Stack2;
class bb_type_ArrayType;
class bb_type_VoidType;
class bb_parser_ScopeExpr;
class bb_type_IdentType;
class bb_stack_Stack3;
class bb_expr_NewArrayExpr;
class bb_expr_NewObjectExpr;
class bb_expr_CastExpr;
class bb_parser_IdentExpr;
class bb_expr_SelfExpr;
class bb_stmt_Stmt;
class bb_list_List4;
class bb_list_Node4;
class bb_list_HeadNode4;
class bb_expr_InvokeSuperExpr;
class bb_parser_IdentTypeExpr;
class bb_parser_FuncCallExpr;
class bb_expr_SliceExpr;
class bb_expr_IndexExpr;
class bb_expr_BinaryExpr;
class bb_expr_BinaryMathExpr;
class bb_expr_BinaryCompareExpr;
class bb_expr_BinaryLogicExpr;
class bb_type_ObjectType;
class bb_decl_ClassDecl;
class bb_decl_AliasDecl;
class bb_list_Enumerator;
class bb_list_List5;
class bb_list_StringList;
class bb_list_Node5;
class bb_list_HeadNode5;
class bb_decl_VarDecl;
class bb_decl_GlobalDecl;
class bb_list_List6;
class bb_list_Node6;
class bb_list_HeadNode6;
class bb_decl_LocalDecl;
class bb_decl_ArgDecl;
class bb_expr_InvokeMemberExpr;
class bb_map_Map3;
class bb_map_StringMap3;
class bb_map_Node3;
class bb_decl_FieldDecl;
class bb_list_Enumerator2;
class bb_stack_Stack4;
class bb_list_List7;
class bb_list_Node7;
class bb_list_HeadNode7;
class bb_stack_Stack5;
class bb_list_List8;
class bb_list_Node8;
class bb_list_HeadNode8;
class bb_stmt_DeclStmt;
class bb_stmt_ReturnStmt;
class bb_stmt_BreakStmt;
class bb_stmt_ContinueStmt;
class bb_stmt_IfStmt;
class bb_stmt_WhileStmt;
class bb_stmt_RepeatStmt;
class bb_parser_ForEachinStmt;
class bb_stmt_AssignStmt;
class bb_stmt_ForStmt;
class bb_expr_VarExpr;
class bb_stmt_CatchStmt;
class bb_stack_Stack6;
class bb_stmt_TryStmt;
class bb_stmt_ThrowStmt;
class bb_stmt_ExprStmt;
class bb_reflector_Reflector;
class bb_map_MapValues;
class bb_map_ValueEnumerator;
class bb_map_Map4;
class bb_map_StringMap4;
class bb_map_Node4;
class bb_list_Enumerator3;
class bb_stack_Stack7;
class bb_list_Enumerator4;
class bb_translator_Translator;
class bb_translator_CTranslator;
class bb_jstranslator_JsTranslator;
class bb_astranslator_AsTranslator;
class bb_javatranslator_JavaTranslator;
class bb_cpptranslator_CppTranslator;
class bb_cstranslator_CsTranslator;
class bb_list_Enumerator5;
class bb_list_List9;
class bb_list_Node9;
class bb_list_HeadNode9;
class bb_expr_MemberVarExpr;
class bb_expr_InvokeExpr;
class bb_expr_StmtExpr;
class bb_map_Map5;
class bb_map_StringMap5;
class bb_map_Node5;
class bb_map_Map6;
class bb_map_StringMap6;
class bb_map_Node6;
class bb_list_Enumerator6;
class bb_stack_Stack8;
class bb_stack_Enumerator;
class bb_stack_Stack9;
class bb_stack_Enumerator2;
String bb_os_ExtractDir(String);
int bb_target_Die(String);
class bb_map_Map : public Object{
	public:
	bb_map_Node* f_root;
	bb_map_Map();
	bb_map_Map* g_new();
	virtual int m_Compare(String,String)=0;
	virtual int m_RotateLeft(bb_map_Node*);
	virtual int m_RotateRight(bb_map_Node*);
	virtual int m_InsertFixup(bb_map_Node*);
	virtual bool m_Set(String,String);
	virtual bb_map_Node* m_FindNode(String);
	virtual String m_Get(String);
	virtual bb_map_Node* m_FirstNode();
	virtual bb_map_NodeEnumerator* m_ObjectEnumerator();
	virtual bool m_Contains(String);
	void mark();
};
class bb_map_StringMap : public bb_map_Map{
	public:
	bb_map_StringMap();
	bb_map_StringMap* g_new();
	virtual int m_Compare(String,String);
	void mark();
};
extern bb_map_StringMap* bb_config__cfgVars;
class bb_map_Node : public Object{
	public:
	String f_key;
	bb_map_Node* f_right;
	bb_map_Node* f_left;
	String f_value;
	int f_color;
	bb_map_Node* f_parent;
	bb_map_Node();
	bb_map_Node* g_new(String,String,int,bb_map_Node*);
	bb_map_Node* g_new2();
	virtual bb_map_Node* m_NextNode();
	virtual String m_Key();
	virtual String m_Value();
	void mark();
};
int bb_config_SetCfgVar(String,String);
class bb_stack_Stack : public Object{
	public:
	Array<String > f_data;
	int f_length;
	bb_stack_Stack();
	bb_stack_Stack* g_new();
	bb_stack_Stack* g_new2(Array<String >);
	virtual int m_Push(String);
	virtual int m_Push2(Array<String >,int,int);
	virtual int m_Push3(Array<String >,int);
	virtual bool m_IsEmpty();
	virtual Array<String > m_ToArray();
	virtual int m_Length();
	virtual String m_Get2(int);
	virtual String m_Pop();
	void mark();
};
class bb_stack_StringStack : public bb_stack_Stack{
	public:
	bb_stack_StringStack();
	bb_stack_StringStack* g_new();
	virtual String m_Join(String);
	void mark();
};
String bb_config_GetCfgVar(String);
String bb_target_ReplaceEnv(String);
String bb_trans2_StripQuotes(String);
extern String bb_target_ANDROID_PATH;
extern String bb_target_JDK_PATH;
extern String bb_target_ANT_PATH;
extern String bb_target_FLEX_PATH;
extern String bb_target_MINGW_PATH;
extern String bb_target_PSM_PATH;
extern String bb_target_MSBUILD_PATH;
extern String bb_target_HTML_PLAYER;
extern String bb_target_FLASH_PLAYER;
int bb_trans2_LoadConfig();
class bb_target_Target : public Object{
	public:
	String f_srcPath;
	bb_decl_AppDecl* f_app;
	String f_transCode;
	String f_TEXT_FILES;
	String f_IMAGE_FILES;
	String f_SOUND_FILES;
	String f_MUSIC_FILES;
	String f_BINARY_FILES;
	String f_DATA_FILES;
	bb_map_StringMap* f_dataFiles;
	bb_target_Target();
	bb_target_Target* g_new();
	virtual int m_Begin()=0;
	virtual int m_AddTransCode(String);
	virtual int m_Translate();
	virtual int m_MakeTarget()=0;
	virtual int m_Make(String);
	virtual int m_CreateDataDir(String);
	virtual int m_Execute(String,int);
	void mark();
};
class bb_html5_Html5Target : public bb_target_Target{
	public:
	bb_html5_Html5Target();
	static int g_IsValid();
	bb_html5_Html5Target* g_new();
	virtual int m_Begin();
	virtual String m_MetaData();
	virtual String m_Config();
	virtual int m_MakeTarget();
	void mark();
};
class bb_flash_FlashTarget : public bb_target_Target{
	public:
	bb_flash_FlashTarget();
	static int g_IsValid();
	bb_flash_FlashTarget* g_new();
	virtual int m_Begin();
	virtual String m_Assets();
	virtual String m_Config();
	virtual int m_MakeTarget();
	void mark();
};
class bb_android_AndroidTarget : public bb_target_Target{
	public:
	bb_android_AndroidTarget();
	static int g_IsValid();
	bb_android_AndroidTarget* g_new();
	virtual int m_Begin();
	virtual String m_Config();
	virtual int m_MakeTarget();
	void mark();
};
class bb_glfw_GlfwTarget : public bb_target_Target{
	public:
	bb_glfw_GlfwTarget();
	static int g_IsValid();
	bb_glfw_GlfwTarget* g_new();
	virtual int m_Begin();
	virtual String m_Config();
	virtual int m_MakeMingw();
	virtual int m_MakeVc2010();
	virtual int m_MakeXcode();
	virtual int m_MakeTarget();
	void mark();
};
class bb_xna_XnaTarget : public bb_target_Target{
	public:
	bb_xna_XnaTarget();
	static int g_IsValid();
	bb_xna_XnaTarget* g_new();
	virtual int m_Begin();
	virtual String m_Content();
	virtual String m_Config();
	virtual int m_MakeTarget();
	void mark();
};
class bb_ios_IosTarget : public bb_target_Target{
	public:
	bb_ios_IosTarget();
	static int g_IsValid();
	bb_ios_IosTarget* g_new();
	virtual int m_Begin();
	virtual String m_Config();
	virtual int m_MakeTarget();
	void mark();
};
class bb_stdcpp_StdcppTarget : public bb_target_Target{
	public:
	bb_stdcpp_StdcppTarget();
	static int g_IsValid();
	bb_stdcpp_StdcppTarget* g_new();
	virtual int m_Begin();
	virtual String m_Config();
	virtual int m_MakeTarget();
	void mark();
};
class bb_psm_PsmTarget : public bb_target_Target{
	public:
	bb_psm_PsmTarget();
	static int g_IsValid();
	bb_psm_PsmTarget* g_new();
	virtual int m_Begin();
	virtual String m_Content();
	virtual String m_Config();
	virtual int m_MakeTarget();
	void mark();
};
class bb_win8_Win8Target : public bb_target_Target{
	public:
	bb_win8_Win8Target();
	static int g_IsValid();
	bb_win8_Win8Target* g_new();
	virtual int m_Begin();
	virtual String m_Config();
	virtual int m_MakeTarget();
	void mark();
};
String bb_targets_ValidTargets();
extern String bb_config_ENV_HOST;
extern String bb_target_OPT_MODPATH;
extern int bb_config_ENV_SAFEMODE;
extern int bb_target_OPT_CLEAN;
extern int bb_target_OPT_ACTION;
extern String bb_target_OPT_OUTPUT;
extern String bb_target_CASED_CONFIG;
bb_target_Target* bb_targets_SelectTarget(String);
extern String bb_config_ENV_CONFIG;
extern String bb_config_ENV_LANG;
extern String bb_config_ENV_TARGET;
class bb_decl_Decl : public Object{
	public:
	String f_errInfo;
	String f_ident;
	String f_munged;
	int f_attrs;
	bb_decl_ScopeDecl* f_scope;
	bb_decl_Decl();
	bb_decl_Decl* g_new();
	virtual int m_IsSemanted();
	virtual int m_IsAbstract();
	virtual int m_IsExtern();
	virtual String m_ToString();
	virtual int m_IsPrivate();
	virtual bb_decl_ModuleDecl* m_ModuleScope();
	virtual bb_decl_FuncDecl* m_FuncScope();
	virtual int m_CheckAccess();
	virtual int m_IsSemanting();
	virtual int m_OnSemant()=0;
	virtual bb_decl_AppDecl* m_AppScope();
	virtual int m_Semant();
	virtual int m_AssertAccess();
	virtual bb_decl_ClassDecl* m_ClassScope();
	virtual bb_decl_Decl* m_OnCopy()=0;
	virtual bb_decl_Decl* m_Copy();
	virtual int m_IsFinal();
	void mark();
};
class bb_decl_ScopeDecl : public bb_decl_Decl{
	public:
	bb_list_List* f_decls;
	bb_map_StringMap2* f_declsMap;
	bb_list_List* f_semanted;
	bb_decl_ScopeDecl();
	bb_decl_ScopeDecl* g_new();
	virtual int m_InsertDecl(bb_decl_Decl*);
	virtual Object* m_GetDecl(String);
	virtual Object* m_FindDecl(String);
	virtual bb_decl_FuncDecl* m_FindFuncDecl(String,Array<bb_expr_Expr* >,int);
	virtual int m_InsertDecls(bb_list_List*);
	virtual bb_list_List* m_Decls();
	virtual bb_type_Type* m_FindType(String,Array<bb_type_Type* >);
	virtual bb_list_List2* m_MethodDecls(String);
	virtual bb_list_List* m_Semanted();
	virtual bb_list_List2* m_SemantedMethods(String);
	virtual bb_decl_ValDecl* m_FindValDecl(String);
	virtual bb_decl_Decl* m_OnCopy();
	virtual int m_OnSemant();
	virtual bb_list_List2* m_SemantedFuncs(String);
	virtual bb_decl_ModuleDecl* m_FindModuleDecl(String);
	virtual bb_decl_ScopeDecl* m_FindScopeDecl(String);
	virtual bb_list_List2* m_FuncDecls(String);
	void mark();
};
class bb_decl_AppDecl : public bb_decl_ScopeDecl{
	public:
	bb_list_List* f_allSemantedDecls;
	bb_list_List6* f_semantedGlobals;
	bb_map_StringMap3* f_imported;
	bb_decl_ModuleDecl* f_mainModule;
	bb_list_StringList* f_fileImports;
	bb_list_List7* f_semantedClasses;
	bb_decl_FuncDecl* f_mainFunc;
	bb_decl_AppDecl();
	bb_decl_AppDecl* g_new();
	virtual int m_InsertModule(bb_decl_ModuleDecl*);
	virtual int m_FinalizeClasses();
	virtual int m_OnSemant();
	void mark();
};
extern String bb_config__errInfo;
class bb_toker_Toker : public Object{
	public:
	String f__path;
	int f__line;
	String f__source;
	int f__length;
	String f__toke;
	int f__tokeType;
	int f__tokePos;
	bb_toker_Toker();
	static bb_set_StringSet* g__keywords;
	static bb_set_StringSet* g__symbols;
	virtual int m__init();
	bb_toker_Toker* g_new(String,String);
	bb_toker_Toker* g_new2(bb_toker_Toker*);
	bb_toker_Toker* g_new3();
	virtual int m_TCHR(int);
	virtual String m_TSTR(int);
	virtual String m_NextToke();
	virtual String m_Toke();
	virtual int m_TokeType();
	virtual String m_Path();
	virtual int m_Line();
	virtual int m_SkipSpace();
	void mark();
};
class bb_set_Set : public Object{
	public:
	bb_map_Map2* f_map;
	bb_set_Set();
	bb_set_Set* g_new(bb_map_Map2*);
	bb_set_Set* g_new2();
	virtual int m_Insert(String);
	virtual bool m_Contains(String);
	void mark();
};
class bb_set_StringSet : public bb_set_Set{
	public:
	bb_set_StringSet();
	bb_set_StringSet* g_new();
	void mark();
};
class bb_map_Map2 : public Object{
	public:
	bb_map_Node2* f_root;
	bb_map_Map2();
	bb_map_Map2* g_new();
	virtual int m_Compare(String,String)=0;
	virtual int m_RotateLeft2(bb_map_Node2*);
	virtual int m_RotateRight2(bb_map_Node2*);
	virtual int m_InsertFixup2(bb_map_Node2*);
	virtual bool m_Set2(String,Object*);
	virtual bool m_Insert2(String,Object*);
	virtual bb_map_Node2* m_FindNode(String);
	virtual bool m_Contains(String);
	virtual Object* m_Get(String);
	void mark();
};
class bb_map_StringMap2 : public bb_map_Map2{
	public:
	bb_map_StringMap2();
	bb_map_StringMap2* g_new();
	virtual int m_Compare(String,String);
	void mark();
};
class bb_map_Node2 : public Object{
	public:
	String f_key;
	bb_map_Node2* f_right;
	bb_map_Node2* f_left;
	Object* f_value;
	int f_color;
	bb_map_Node2* f_parent;
	bb_map_Node2();
	bb_map_Node2* g_new(String,Object*,int,bb_map_Node2*);
	bb_map_Node2* g_new2();
	void mark();
};
int bb_config_IsSpace(int);
int bb_config_IsAlpha(int);
int bb_config_IsDigit(int);
int bb_config_IsBinDigit(int);
int bb_config_IsHexDigit(int);
class bb_type_Type : public Object{
	public:
	bb_type_ArrayType* f_arrayOf;
	bb_type_Type();
	bb_type_Type* g_new();
	static bb_type_BoolType* g_boolType;
	static bb_type_StringType* g_stringType;
	static bb_type_VoidType* g_voidType;
	static bb_type_ArrayType* g_emptyArrayType;
	static bb_type_IntType* g_intType;
	static bb_type_FloatType* g_floatType;
	static bb_type_IdentType* g_objectType;
	static bb_type_IdentType* g_throwableType;
	virtual bb_type_ArrayType* m_ArrayOf();
	static bb_type_IdentType* g_nullObjectType;
	virtual int m_EqualsType(bb_type_Type*);
	virtual bb_type_Type* m_Semant();
	virtual int m_ExtendsType(bb_type_Type*);
	virtual String m_ToString();
	virtual bb_decl_ClassDecl* m_GetClass();
	void mark();
};
class bb_type_BoolType : public bb_type_Type{
	public:
	bb_type_BoolType();
	bb_type_BoolType* g_new();
	virtual int m_EqualsType(bb_type_Type*);
	virtual int m_ExtendsType(bb_type_Type*);
	virtual bb_decl_ClassDecl* m_GetClass();
	virtual String m_ToString();
	void mark();
};
class bb_map_NodeEnumerator : public Object{
	public:
	bb_map_Node* f_node;
	bb_map_NodeEnumerator();
	bb_map_NodeEnumerator* g_new(bb_map_Node*);
	bb_map_NodeEnumerator* g_new2();
	virtual bool m_HasNext();
	virtual bb_map_Node* m_NextObject();
	void mark();
};
class bb_decl_ValDecl : public bb_decl_Decl{
	public:
	bb_type_Type* f_type;
	bb_expr_Expr* f_init;
	bb_decl_ValDecl();
	bb_decl_ValDecl* g_new();
	virtual String m_ToString();
	virtual int m_OnSemant();
	virtual bb_expr_Expr* m_CopyInit();
	void mark();
};
class bb_decl_ConstDecl : public bb_decl_ValDecl{
	public:
	String f_value;
	bb_decl_ConstDecl();
	bb_decl_ConstDecl* g_new(String,int,bb_type_Type*,bb_expr_Expr*);
	bb_decl_ConstDecl* g_new2();
	virtual bb_decl_Decl* m_OnCopy();
	virtual int m_OnSemant();
	void mark();
};
class bb_type_StringType : public bb_type_Type{
	public:
	bb_type_StringType();
	bb_type_StringType* g_new();
	virtual int m_EqualsType(bb_type_Type*);
	virtual int m_ExtendsType(bb_type_Type*);
	virtual bb_decl_ClassDecl* m_GetClass();
	virtual String m_ToString();
	void mark();
};
class bb_expr_Expr : public Object{
	public:
	bb_type_Type* f_exprType;
	bb_expr_Expr();
	bb_expr_Expr* g_new();
	virtual bb_expr_Expr* m_Semant();
	virtual Array<bb_expr_Expr* > m_SemantArgs(Array<bb_expr_Expr* >);
	virtual bb_expr_Expr* m_Cast(bb_type_Type*,int);
	virtual Array<bb_expr_Expr* > m_CastArgs(Array<bb_expr_Expr* >,bb_decl_FuncDecl*);
	virtual String m_ToString();
	virtual String m_Eval();
	virtual bb_expr_Expr* m_EvalConst();
	virtual bb_expr_Expr* m_Semant2(bb_type_Type*,int);
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_CopyExpr(bb_expr_Expr*);
	virtual Array<bb_expr_Expr* > m_CopyArgs(Array<bb_expr_Expr* >);
	virtual bb_type_Type* m_BalanceTypes(bb_type_Type*,bb_type_Type*);
	virtual bb_expr_Expr* m_SemantSet(String,bb_expr_Expr*);
	virtual bb_decl_ScopeDecl* m_SemantScope();
	virtual bb_expr_Expr* m_SemantFunc(Array<bb_expr_Expr* >);
	virtual bool m_SideEffects();
	virtual String m_Trans();
	virtual String m_TransStmt();
	virtual String m_TransVar();
	void mark();
};
class bb_expr_ConstExpr : public bb_expr_Expr{
	public:
	bb_type_Type* f_ty;
	String f_value;
	bb_expr_ConstExpr();
	bb_expr_ConstExpr* g_new(bb_type_Type*,String);
	bb_expr_ConstExpr* g_new2();
	virtual bb_expr_Expr* m_Semant();
	virtual bb_expr_Expr* m_Copy();
	virtual String m_ToString();
	virtual String m_Eval();
	virtual bb_expr_Expr* m_EvalConst();
	virtual bool m_SideEffects();
	virtual String m_Trans();
	void mark();
};
class bb_type_NumericType : public bb_type_Type{
	public:
	bb_type_NumericType();
	bb_type_NumericType* g_new();
	void mark();
};
class bb_type_IntType : public bb_type_NumericType{
	public:
	bb_type_IntType();
	bb_type_IntType* g_new();
	virtual int m_EqualsType(bb_type_Type*);
	virtual int m_ExtendsType(bb_type_Type*);
	virtual bb_decl_ClassDecl* m_GetClass();
	virtual String m_ToString();
	void mark();
};
int bb_config_StringToInt(String,int);
class bb_type_FloatType : public bb_type_NumericType{
	public:
	bb_type_FloatType();
	bb_type_FloatType* g_new();
	virtual int m_EqualsType(bb_type_Type*);
	virtual int m_ExtendsType(bb_type_Type*);
	virtual bb_decl_ClassDecl* m_GetClass();
	virtual String m_ToString();
	void mark();
};
int bb_config_InternalErr(String);
class bb_list_List : public Object{
	public:
	bb_list_Node* f__head;
	bb_list_List();
	bb_list_List* g_new();
	virtual bb_list_Node* m_AddLast(bb_decl_Decl*);
	bb_list_List* g_new2(Array<bb_decl_Decl* >);
	virtual bb_list_Enumerator2* m_ObjectEnumerator();
	virtual int m_Count();
	void mark();
};
class bb_list_Node : public Object{
	public:
	bb_list_Node* f__succ;
	bb_list_Node* f__pred;
	bb_decl_Decl* f__data;
	bb_list_Node();
	bb_list_Node* g_new(bb_list_Node*,bb_list_Node*,bb_decl_Decl*);
	bb_list_Node* g_new2();
	void mark();
};
class bb_list_HeadNode : public bb_list_Node{
	public:
	bb_list_HeadNode();
	bb_list_HeadNode* g_new();
	void mark();
};
class bb_decl_BlockDecl : public bb_decl_ScopeDecl{
	public:
	bb_list_List4* f_stmts;
	bb_decl_BlockDecl();
	bb_decl_BlockDecl* g_new(bb_decl_ScopeDecl*);
	bb_decl_BlockDecl* g_new2();
	virtual int m_AddStmt(bb_stmt_Stmt*);
	virtual bb_decl_Decl* m_OnCopy();
	virtual int m_OnSemant();
	virtual bb_decl_BlockDecl* m_CopyBlock(bb_decl_ScopeDecl*);
	void mark();
};
class bb_decl_FuncDecl : public bb_decl_BlockDecl{
	public:
	Array<bb_decl_ArgDecl* > f_argDecls;
	bb_type_Type* f_retType;
	bb_decl_FuncDecl* f_overrides;
	bb_decl_FuncDecl();
	virtual bool m_IsCtor();
	virtual bool m_IsMethod();
	virtual String m_ToString();
	virtual bool m_EqualsArgs(bb_decl_FuncDecl*);
	virtual bool m_EqualsFunc(bb_decl_FuncDecl*);
	bb_decl_FuncDecl* g_new(String,int,bb_type_Type*,Array<bb_decl_ArgDecl* >);
	bb_decl_FuncDecl* g_new2();
	virtual bb_decl_Decl* m_OnCopy();
	virtual int m_OnSemant();
	virtual bool m_IsStatic();
	virtual bool m_IsProperty();
	void mark();
};
class bb_list_List2 : public Object{
	public:
	bb_list_Node2* f__head;
	bb_list_List2();
	bb_list_List2* g_new();
	virtual bb_list_Node2* m_AddLast2(bb_decl_FuncDecl*);
	bb_list_List2* g_new2(Array<bb_decl_FuncDecl* >);
	virtual bb_list_Enumerator* m_ObjectEnumerator();
	virtual bool m_IsEmpty();
	virtual int m_Count();
	void mark();
};
class bb_decl_FuncDeclList : public bb_list_List2{
	public:
	bb_decl_FuncDeclList();
	bb_decl_FuncDeclList* g_new();
	void mark();
};
class bb_list_Node2 : public Object{
	public:
	bb_list_Node2* f__succ;
	bb_list_Node2* f__pred;
	bb_decl_FuncDecl* f__data;
	bb_list_Node2();
	bb_list_Node2* g_new(bb_list_Node2*,bb_list_Node2*,bb_decl_FuncDecl*);
	bb_list_Node2* g_new2();
	void mark();
};
class bb_list_HeadNode2 : public bb_list_Node2{
	public:
	bb_list_HeadNode2();
	bb_list_HeadNode2* g_new();
	void mark();
};
int bb_config_Err(String);
extern bb_decl_ScopeDecl* bb_decl__env;
class bb_list_List3 : public Object{
	public:
	bb_list_Node3* f__head;
	bb_list_List3();
	bb_list_List3* g_new();
	virtual bb_list_Node3* m_AddLast3(bb_decl_ScopeDecl*);
	bb_list_List3* g_new2(Array<bb_decl_ScopeDecl* >);
	virtual bool m_IsEmpty();
	virtual bb_decl_ScopeDecl* m_RemoveLast();
	void mark();
};
class bb_list_Node3 : public Object{
	public:
	bb_list_Node3* f__succ;
	bb_list_Node3* f__pred;
	bb_decl_ScopeDecl* f__data;
	bb_list_Node3();
	bb_list_Node3* g_new(bb_list_Node3*,bb_list_Node3*,bb_decl_ScopeDecl*);
	bb_list_Node3* g_new2();
	virtual bb_list_Node3* m_GetNode();
	virtual bb_list_Node3* m_PrevNode();
	virtual int m_Remove();
	void mark();
};
class bb_list_HeadNode3 : public bb_list_Node3{
	public:
	bb_list_HeadNode3();
	bb_list_HeadNode3* g_new();
	virtual bb_list_Node3* m_GetNode();
	void mark();
};
extern bb_list_List3* bb_decl__envStack;
int bb_decl_PushEnv(bb_decl_ScopeDecl*);
class bb_parser_Parser : public Object{
	public:
	String f__toke;
	bb_toker_Toker* f__toker;
	bb_decl_AppDecl* f__app;
	bb_decl_ModuleDecl* f__module;
	int f__defattrs;
	int f__tokeType;
	bb_decl_BlockDecl* f__block;
	bb_list_List8* f__blockStack;
	bb_list_StringList* f__errStack;
	bb_parser_Parser();
	virtual int m_SetErr();
	virtual int m_CParse(String);
	virtual int m_SkipEols();
	virtual String m_NextToke();
	bb_parser_Parser* g_new(bb_toker_Toker*,bb_decl_AppDecl*,bb_decl_ModuleDecl*,int);
	bb_parser_Parser* g_new2();
	virtual int m_Parse(String);
	virtual bb_expr_ArrayExpr* m_ParseArrayExpr();
	virtual bb_type_Type* m_CParsePrimitiveType();
	virtual String m_ParseIdent();
	virtual bb_type_IdentType* m_ParseIdentType();
	virtual bb_type_Type* m_ParseType();
	virtual int m_AtEos();
	virtual Array<bb_expr_Expr* > m_ParseArgs(int);
	virtual bb_type_IdentType* m_CParseIdentType(bool);
	virtual bb_expr_Expr* m_ParsePrimaryExpr(int);
	virtual bb_expr_Expr* m_ParseUnaryExpr();
	virtual bb_expr_Expr* m_ParseMulDivExpr();
	virtual bb_expr_Expr* m_ParseAddSubExpr();
	virtual bb_expr_Expr* m_ParseBitandExpr();
	virtual bb_expr_Expr* m_ParseBitorExpr();
	virtual bb_expr_Expr* m_ParseCompareExpr();
	virtual bb_expr_Expr* m_ParseAndExpr();
	virtual bb_expr_Expr* m_ParseOrExpr();
	virtual bb_expr_Expr* m_ParseExpr();
	virtual int m_ValidateModIdent(String);
	virtual String m_RealPath(String);
	virtual int m_ImportModule(String,int);
	virtual String m_ParseStringLit();
	virtual int m_ImportFile(String);
	virtual String m_ParseModPath();
	virtual bb_type_Type* m_ParseDeclType();
	virtual bb_decl_Decl* m_ParseDecl(String,int);
	virtual bb_list_List* m_ParseDecls(String,int);
	virtual int m_PushBlock(bb_decl_BlockDecl*);
	virtual int m_ParseDeclStmts();
	virtual int m_ParseReturnStmt();
	virtual int m_ParseExitStmt();
	virtual int m_ParseContinueStmt();
	virtual int m_PopBlock();
	virtual int m_ParseIfStmt(String);
	virtual int m_ParseWhileStmt();
	virtual int m_PushErr();
	virtual int m_PopErr();
	virtual int m_ParseRepeatStmt();
	virtual int m_ParseForStmt();
	virtual int m_ParseSelectStmt();
	virtual int m_ParseTryStmt();
	virtual int m_ParseThrowStmt();
	virtual int m_ParseStmt();
	virtual bb_decl_FuncDecl* m_ParseFuncDecl(int);
	virtual bb_decl_ClassDecl* m_ParseClassDecl(int);
	virtual int m_ParseMain();
	void mark();
};
class bb_decl_ModuleDecl : public bb_decl_ScopeDecl{
	public:
	String f_filepath;
	bb_map_StringMap3* f_imported;
	bb_map_StringMap3* f_pubImported;
	bb_decl_ModuleDecl();
	bb_decl_ModuleDecl* g_new(String,int,String,String);
	bb_decl_ModuleDecl* g_new2();
	virtual int m_IsStrict();
	virtual int m_SemantAll();
	virtual String m_ToString();
	virtual Object* m_GetDecl2(String);
	virtual Object* m_GetDecl(String);
	virtual int m_OnSemant();
	void mark();
};
class bb_expr_UnaryExpr : public bb_expr_Expr{
	public:
	String f_op;
	bb_expr_Expr* f_expr;
	bb_expr_UnaryExpr();
	bb_expr_UnaryExpr* g_new(String,bb_expr_Expr*);
	bb_expr_UnaryExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Eval();
	virtual String m_Trans();
	void mark();
};
class bb_expr_ArrayExpr : public bb_expr_Expr{
	public:
	Array<bb_expr_Expr* > f_exprs;
	bb_expr_ArrayExpr();
	bb_expr_ArrayExpr* g_new(Array<bb_expr_Expr* >);
	bb_expr_ArrayExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Trans();
	void mark();
};
class bb_stack_Stack2 : public Object{
	public:
	Array<bb_expr_Expr* > f_data;
	int f_length;
	bb_stack_Stack2();
	bb_stack_Stack2* g_new();
	bb_stack_Stack2* g_new2(Array<bb_expr_Expr* >);
	virtual int m_Push4(bb_expr_Expr*);
	virtual int m_Push5(Array<bb_expr_Expr* >,int,int);
	virtual int m_Push6(Array<bb_expr_Expr* >,int);
	virtual Array<bb_expr_Expr* > m_ToArray();
	void mark();
};
class bb_type_ArrayType : public bb_type_Type{
	public:
	bb_type_Type* f_elemType;
	bb_type_ArrayType();
	bb_type_ArrayType* g_new(bb_type_Type*);
	bb_type_ArrayType* g_new2();
	virtual int m_EqualsType(bb_type_Type*);
	virtual int m_ExtendsType(bb_type_Type*);
	virtual bb_type_Type* m_Semant();
	virtual bb_decl_ClassDecl* m_GetClass();
	virtual String m_ToString();
	void mark();
};
class bb_type_VoidType : public bb_type_Type{
	public:
	bb_type_VoidType();
	bb_type_VoidType* g_new();
	virtual int m_EqualsType(bb_type_Type*);
	virtual String m_ToString();
	void mark();
};
class bb_parser_ScopeExpr : public bb_expr_Expr{
	public:
	bb_decl_ScopeDecl* f_scope;
	bb_parser_ScopeExpr();
	bb_parser_ScopeExpr* g_new(bb_decl_ScopeDecl*);
	bb_parser_ScopeExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual String m_ToString();
	virtual bb_expr_Expr* m_Semant();
	virtual bb_decl_ScopeDecl* m_SemantScope();
	void mark();
};
class bb_type_IdentType : public bb_type_Type{
	public:
	String f_ident;
	Array<bb_type_Type* > f_args;
	bb_type_IdentType();
	bb_type_IdentType* g_new(String,Array<bb_type_Type* >);
	bb_type_IdentType* g_new2();
	virtual int m_EqualsType(bb_type_Type*);
	virtual int m_ExtendsType(bb_type_Type*);
	virtual bb_type_Type* m_Semant();
	virtual String m_ToString();
	virtual bb_decl_ClassDecl* m_SemantClass();
	void mark();
};
class bb_stack_Stack3 : public Object{
	public:
	Array<bb_type_Type* > f_data;
	int f_length;
	bb_stack_Stack3();
	bb_stack_Stack3* g_new();
	bb_stack_Stack3* g_new2(Array<bb_type_Type* >);
	virtual int m_Push7(bb_type_Type*);
	virtual int m_Push8(Array<bb_type_Type* >,int,int);
	virtual int m_Push9(Array<bb_type_Type* >,int);
	virtual Array<bb_type_Type* > m_ToArray();
	void mark();
};
class bb_expr_NewArrayExpr : public bb_expr_Expr{
	public:
	bb_type_Type* f_ty;
	bb_expr_Expr* f_expr;
	bb_expr_NewArrayExpr();
	bb_expr_NewArrayExpr* g_new(bb_type_Type*,bb_expr_Expr*);
	bb_expr_NewArrayExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Trans();
	void mark();
};
class bb_expr_NewObjectExpr : public bb_expr_Expr{
	public:
	bb_type_Type* f_ty;
	Array<bb_expr_Expr* > f_args;
	bb_decl_ClassDecl* f_classDecl;
	bb_decl_FuncDecl* f_ctor;
	bb_expr_NewObjectExpr();
	bb_expr_NewObjectExpr* g_new(bb_type_Type*,Array<bb_expr_Expr* >);
	bb_expr_NewObjectExpr* g_new2();
	virtual bb_expr_Expr* m_Semant();
	virtual bb_expr_Expr* m_Copy();
	virtual String m_Trans();
	void mark();
};
class bb_expr_CastExpr : public bb_expr_Expr{
	public:
	bb_type_Type* f_ty;
	bb_expr_Expr* f_expr;
	int f_flags;
	bb_expr_CastExpr();
	bb_expr_CastExpr* g_new(bb_type_Type*,bb_expr_Expr*,int);
	bb_expr_CastExpr* g_new2();
	virtual bb_expr_Expr* m_Semant();
	virtual bb_expr_Expr* m_Copy();
	virtual String m_Eval();
	virtual String m_Trans();
	void mark();
};
class bb_parser_IdentExpr : public bb_expr_Expr{
	public:
	String f_ident;
	bb_expr_Expr* f_expr;
	bb_decl_ScopeDecl* f_scope;
	bool f_static;
	bb_parser_IdentExpr();
	bb_parser_IdentExpr* g_new(String,bb_expr_Expr*);
	bb_parser_IdentExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual String m_ToString();
	virtual int m__Semant();
	virtual int m_IdentErr();
	virtual bb_expr_Expr* m_SemantSet(String,bb_expr_Expr*);
	virtual bb_expr_Expr* m_Semant();
	virtual bb_decl_ScopeDecl* m_SemantScope();
	virtual bb_expr_Expr* m_SemantFunc(Array<bb_expr_Expr* >);
	void mark();
};
class bb_expr_SelfExpr : public bb_expr_Expr{
	public:
	bb_expr_SelfExpr();
	bb_expr_SelfExpr* g_new();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual bool m_SideEffects();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_Stmt : public Object{
	public:
	String f_errInfo;
	bb_stmt_Stmt();
	bb_stmt_Stmt* g_new();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*)=0;
	virtual bb_stmt_Stmt* m_Copy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant()=0;
	virtual int m_Semant();
	virtual String m_Trans()=0;
	void mark();
};
class bb_list_List4 : public Object{
	public:
	bb_list_Node4* f__head;
	bb_list_List4();
	bb_list_List4* g_new();
	virtual bb_list_Node4* m_AddLast4(bb_stmt_Stmt*);
	bb_list_List4* g_new2(Array<bb_stmt_Stmt* >);
	virtual bool m_IsEmpty();
	virtual bb_list_Enumerator5* m_ObjectEnumerator();
	virtual bb_list_Node4* m_AddFirst(bb_stmt_Stmt*);
	virtual bb_stmt_Stmt* m_Last();
	void mark();
};
class bb_list_Node4 : public Object{
	public:
	bb_list_Node4* f__succ;
	bb_list_Node4* f__pred;
	bb_stmt_Stmt* f__data;
	bb_list_Node4();
	bb_list_Node4* g_new(bb_list_Node4*,bb_list_Node4*,bb_stmt_Stmt*);
	bb_list_Node4* g_new2();
	virtual bb_list_Node4* m_GetNode();
	virtual bb_list_Node4* m_PrevNode();
	void mark();
};
class bb_list_HeadNode4 : public bb_list_Node4{
	public:
	bb_list_HeadNode4();
	bb_list_HeadNode4* g_new();
	virtual bb_list_Node4* m_GetNode();
	void mark();
};
class bb_expr_InvokeSuperExpr : public bb_expr_Expr{
	public:
	String f_ident;
	Array<bb_expr_Expr* > f_args;
	bb_decl_FuncDecl* f_funcDecl;
	bb_expr_InvokeSuperExpr();
	bb_expr_InvokeSuperExpr* g_new(String,Array<bb_expr_Expr* >);
	bb_expr_InvokeSuperExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Trans();
	void mark();
};
class bb_parser_IdentTypeExpr : public bb_expr_Expr{
	public:
	bb_decl_ClassDecl* f_cdecl;
	bb_parser_IdentTypeExpr();
	bb_parser_IdentTypeExpr* g_new(bb_type_Type*);
	bb_parser_IdentTypeExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual int m__Semant();
	virtual bb_expr_Expr* m_Semant();
	virtual bb_decl_ScopeDecl* m_SemantScope();
	virtual bb_expr_Expr* m_SemantFunc(Array<bb_expr_Expr* >);
	void mark();
};
String bb_config_Dequote(String,String);
class bb_parser_FuncCallExpr : public bb_expr_Expr{
	public:
	bb_expr_Expr* f_expr;
	Array<bb_expr_Expr* > f_args;
	bb_parser_FuncCallExpr();
	bb_parser_FuncCallExpr* g_new(bb_expr_Expr*,Array<bb_expr_Expr* >);
	bb_parser_FuncCallExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual String m_ToString();
	virtual bb_expr_Expr* m_Semant();
	void mark();
};
class bb_expr_SliceExpr : public bb_expr_Expr{
	public:
	bb_expr_Expr* f_expr;
	bb_expr_Expr* f_from;
	bb_expr_Expr* f_term;
	bb_expr_SliceExpr();
	bb_expr_SliceExpr* g_new(bb_expr_Expr*,bb_expr_Expr*,bb_expr_Expr*);
	bb_expr_SliceExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Eval();
	virtual String m_Trans();
	void mark();
};
class bb_expr_IndexExpr : public bb_expr_Expr{
	public:
	bb_expr_Expr* f_expr;
	bb_expr_Expr* f_index;
	bb_expr_IndexExpr();
	bb_expr_IndexExpr* g_new(bb_expr_Expr*,bb_expr_Expr*);
	bb_expr_IndexExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Eval();
	virtual bb_expr_Expr* m_SemantSet(String,bb_expr_Expr*);
	virtual bool m_SideEffects();
	virtual String m_Trans();
	virtual String m_TransVar();
	void mark();
};
class bb_expr_BinaryExpr : public bb_expr_Expr{
	public:
	String f_op;
	bb_expr_Expr* f_lhs;
	bb_expr_Expr* f_rhs;
	bb_expr_BinaryExpr();
	bb_expr_BinaryExpr* g_new(String,bb_expr_Expr*,bb_expr_Expr*);
	bb_expr_BinaryExpr* g_new2();
	virtual String m_Trans();
	void mark();
};
class bb_expr_BinaryMathExpr : public bb_expr_BinaryExpr{
	public:
	bb_expr_BinaryMathExpr();
	bb_expr_BinaryMathExpr* g_new(String,bb_expr_Expr*,bb_expr_Expr*);
	bb_expr_BinaryMathExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Eval();
	void mark();
};
class bb_expr_BinaryCompareExpr : public bb_expr_BinaryExpr{
	public:
	bb_type_Type* f_ty;
	bb_expr_BinaryCompareExpr();
	bb_expr_BinaryCompareExpr* g_new(String,bb_expr_Expr*,bb_expr_Expr*);
	bb_expr_BinaryCompareExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Eval();
	void mark();
};
class bb_expr_BinaryLogicExpr : public bb_expr_BinaryExpr{
	public:
	bb_expr_BinaryLogicExpr();
	bb_expr_BinaryLogicExpr* g_new(String,bb_expr_Expr*,bb_expr_Expr*);
	bb_expr_BinaryLogicExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Eval();
	void mark();
};
class bb_type_ObjectType : public bb_type_Type{
	public:
	bb_decl_ClassDecl* f_classDecl;
	bb_type_ObjectType();
	bb_type_ObjectType* g_new(bb_decl_ClassDecl*);
	bb_type_ObjectType* g_new2();
	virtual int m_EqualsType(bb_type_Type*);
	virtual bb_decl_ClassDecl* m_GetClass();
	virtual int m_ExtendsType(bb_type_Type*);
	virtual String m_ToString();
	void mark();
};
class bb_decl_ClassDecl : public bb_decl_ScopeDecl{
	public:
	Array<String > f_args;
	bb_decl_ClassDecl* f_instanceof;
	Array<bb_type_Type* > f_instArgs;
	Array<bb_decl_ClassDecl* > f_implmentsAll;
	bb_type_IdentType* f_superTy;
	Array<bb_type_IdentType* > f_impltys;
	bb_type_ObjectType* f_objectType;
	bb_list_List7* f_instances;
	bb_decl_ClassDecl* f_superClass;
	Array<bb_decl_ClassDecl* > f_implments;
	bb_decl_ClassDecl();
	virtual int m_IsInterface();
	virtual String m_ToString();
	virtual bb_decl_FuncDecl* m_FindFuncDecl2(String,Array<bb_expr_Expr* >,int);
	virtual bb_decl_FuncDecl* m_FindFuncDecl(String,Array<bb_expr_Expr* >,int);
	bb_decl_ClassDecl* g_new(String,int,Array<String >,bb_type_IdentType*,Array<bb_type_IdentType* >);
	bb_decl_ClassDecl* g_new2();
	virtual int m_ExtendsObject();
	virtual bb_decl_ClassDecl* m_GenClassInstance(Array<bb_type_Type* >);
	virtual int m_IsFinalized();
	virtual int m_UpdateLiveMethods();
	virtual int m_IsInstanced();
	virtual int m_FinalizeClass();
	static bb_decl_ClassDecl* g_nullObjectClass;
	virtual int m_ExtendsClass(bb_decl_ClassDecl*);
	virtual bb_decl_Decl* m_OnCopy();
	virtual Object* m_GetDecl2(String);
	virtual Object* m_GetDecl(String);
	virtual int m_IsThrowable();
	virtual int m_OnSemant();
	void mark();
};
class bb_decl_AliasDecl : public bb_decl_Decl{
	public:
	Object* f_decl;
	bb_decl_AliasDecl();
	bb_decl_AliasDecl* g_new(String,int,Object*);
	bb_decl_AliasDecl* g_new2();
	virtual bb_decl_Decl* m_OnCopy();
	virtual int m_OnSemant();
	void mark();
};
class bb_list_Enumerator : public Object{
	public:
	bb_list_List2* f__list;
	bb_list_Node2* f__curr;
	bb_list_Enumerator();
	bb_list_Enumerator* g_new(bb_list_List2*);
	bb_list_Enumerator* g_new2();
	virtual bool m_HasNext();
	virtual bb_decl_FuncDecl* m_NextObject();
	void mark();
};
class bb_list_List5 : public Object{
	public:
	bb_list_Node5* f__head;
	bb_list_List5();
	bb_list_List5* g_new();
	virtual bb_list_Node5* m_AddLast5(String);
	bb_list_List5* g_new2(Array<String >);
	virtual String m_RemoveLast();
	virtual bb_list_Enumerator4* m_ObjectEnumerator();
	virtual bool m_IsEmpty();
	virtual String m_RemoveFirst();
	virtual int m_Count();
	virtual Array<String > m_ToArray();
	virtual int m_Clear();
	void mark();
};
class bb_list_StringList : public bb_list_List5{
	public:
	bb_list_StringList();
	bb_list_StringList* g_new();
	virtual String m_Join(String);
	void mark();
};
class bb_list_Node5 : public Object{
	public:
	bb_list_Node5* f__succ;
	bb_list_Node5* f__pred;
	String f__data;
	bb_list_Node5();
	bb_list_Node5* g_new(bb_list_Node5*,bb_list_Node5*,String);
	bb_list_Node5* g_new2();
	virtual bb_list_Node5* m_GetNode();
	virtual bb_list_Node5* m_PrevNode();
	virtual int m_Remove();
	virtual bb_list_Node5* m_NextNode();
	void mark();
};
class bb_list_HeadNode5 : public bb_list_Node5{
	public:
	bb_list_HeadNode5();
	bb_list_HeadNode5* g_new();
	virtual bb_list_Node5* m_GetNode();
	void mark();
};
extern bb_list_StringList* bb_config__errStack;
int bb_config_PushErr(String);
class bb_decl_VarDecl : public bb_decl_ValDecl{
	public:
	bb_decl_VarDecl();
	bb_decl_VarDecl* g_new();
	void mark();
};
class bb_decl_GlobalDecl : public bb_decl_VarDecl{
	public:
	bb_decl_GlobalDecl();
	bb_decl_GlobalDecl* g_new(String,int,bb_type_Type*,bb_expr_Expr*);
	bb_decl_GlobalDecl* g_new2();
	virtual String m_ToString();
	virtual bb_decl_Decl* m_OnCopy();
	void mark();
};
class bb_list_List6 : public Object{
	public:
	bb_list_Node6* f__head;
	bb_list_List6();
	bb_list_List6* g_new();
	virtual bb_list_Node6* m_AddLast6(bb_decl_GlobalDecl*);
	bb_list_List6* g_new2(Array<bb_decl_GlobalDecl* >);
	virtual bb_list_Enumerator6* m_ObjectEnumerator();
	void mark();
};
class bb_list_Node6 : public Object{
	public:
	bb_list_Node6* f__succ;
	bb_list_Node6* f__pred;
	bb_decl_GlobalDecl* f__data;
	bb_list_Node6();
	bb_list_Node6* g_new(bb_list_Node6*,bb_list_Node6*,bb_decl_GlobalDecl*);
	bb_list_Node6* g_new2();
	void mark();
};
class bb_list_HeadNode6 : public bb_list_Node6{
	public:
	bb_list_HeadNode6();
	bb_list_HeadNode6* g_new();
	void mark();
};
int bb_decl_PopEnv();
int bb_config_PopErr();
class bb_decl_LocalDecl : public bb_decl_VarDecl{
	public:
	bb_decl_LocalDecl();
	virtual String m_ToString();
	bb_decl_LocalDecl* g_new(String,int,bb_type_Type*,bb_expr_Expr*);
	bb_decl_LocalDecl* g_new2();
	virtual bb_decl_Decl* m_OnCopy();
	void mark();
};
class bb_decl_ArgDecl : public bb_decl_LocalDecl{
	public:
	bb_decl_ArgDecl();
	virtual String m_ToString();
	bb_decl_ArgDecl* g_new(String,int,bb_type_Type*,bb_expr_Expr*);
	bb_decl_ArgDecl* g_new2();
	virtual bb_decl_Decl* m_OnCopy();
	void mark();
};
class bb_expr_InvokeMemberExpr : public bb_expr_Expr{
	public:
	bb_expr_Expr* f_expr;
	bb_decl_FuncDecl* f_decl;
	Array<bb_expr_Expr* > f_args;
	int f_isResize;
	bb_expr_InvokeMemberExpr();
	bb_expr_InvokeMemberExpr* g_new(bb_expr_Expr*,bb_decl_FuncDecl*,Array<bb_expr_Expr* >);
	bb_expr_InvokeMemberExpr* g_new2();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_ToString();
	virtual String m_Trans();
	virtual String m_TransStmt();
	void mark();
};
String bb_preprocessor_Eval(String,bb_type_Type*);
String bb_preprocessor_Eval2(bb_toker_Toker*,bb_type_Type*);
int bb_math_Min(int,int);
Float bb_math_Min2(Float,Float);
String bb_config_EvalCfgTags(String);
String bb_preprocessor_PreProcess(String);
String bb_os_StripExt(String);
String bb_os_StripDir(String);
String bb_os_StripAll(String);
class bb_map_Map3 : public Object{
	public:
	bb_map_Node3* f_root;
	bb_map_Map3();
	bb_map_Map3* g_new();
	virtual int m_Compare(String,String)=0;
	virtual int m_RotateLeft3(bb_map_Node3*);
	virtual int m_RotateRight3(bb_map_Node3*);
	virtual int m_InsertFixup3(bb_map_Node3*);
	virtual bool m_Set3(String,bb_decl_ModuleDecl*);
	virtual bool m_Insert3(String,bb_decl_ModuleDecl*);
	virtual bb_map_Node3* m_FindNode(String);
	virtual bool m_Contains(String);
	virtual bb_decl_ModuleDecl* m_Get(String);
	virtual bb_decl_ModuleDecl* m_ValueForKey(String);
	virtual bb_map_MapValues* m_Values();
	virtual bb_map_Node3* m_FirstNode();
	void mark();
};
class bb_map_StringMap3 : public bb_map_Map3{
	public:
	bb_map_StringMap3();
	bb_map_StringMap3* g_new();
	virtual int m_Compare(String,String);
	void mark();
};
class bb_map_Node3 : public Object{
	public:
	String f_key;
	bb_map_Node3* f_right;
	bb_map_Node3* f_left;
	bb_decl_ModuleDecl* f_value;
	int f_color;
	bb_map_Node3* f_parent;
	bb_map_Node3();
	bb_map_Node3* g_new(String,bb_decl_ModuleDecl*,int,bb_map_Node3*);
	bb_map_Node3* g_new2();
	virtual bb_map_Node3* m_NextNode();
	void mark();
};
extern String bb_parser_FILE_EXT;
bb_decl_ModuleDecl* bb_parser_ParseModule(String,bb_decl_AppDecl*);
class bb_decl_FieldDecl : public bb_decl_VarDecl{
	public:
	bb_decl_FieldDecl();
	bb_decl_FieldDecl* g_new(String,int,bb_type_Type*,bb_expr_Expr*);
	bb_decl_FieldDecl* g_new2();
	virtual String m_ToString();
	virtual bb_decl_Decl* m_OnCopy();
	void mark();
};
class bb_list_Enumerator2 : public Object{
	public:
	bb_list_List* f__list;
	bb_list_Node* f__curr;
	bb_list_Enumerator2();
	bb_list_Enumerator2* g_new(bb_list_List*);
	bb_list_Enumerator2* g_new2();
	virtual bool m_HasNext();
	virtual bb_decl_Decl* m_NextObject();
	void mark();
};
class bb_stack_Stack4 : public Object{
	public:
	Array<bb_type_IdentType* > f_data;
	int f_length;
	bb_stack_Stack4();
	bb_stack_Stack4* g_new();
	bb_stack_Stack4* g_new2(Array<bb_type_IdentType* >);
	virtual int m_Push10(bb_type_IdentType*);
	virtual int m_Push11(Array<bb_type_IdentType* >,int,int);
	virtual int m_Push12(Array<bb_type_IdentType* >,int);
	virtual Array<bb_type_IdentType* > m_ToArray();
	void mark();
};
class bb_list_List7 : public Object{
	public:
	bb_list_Node7* f__head;
	bb_list_List7();
	bb_list_List7* g_new();
	virtual bb_list_Node7* m_AddLast7(bb_decl_ClassDecl*);
	bb_list_List7* g_new2(Array<bb_decl_ClassDecl* >);
	virtual bb_list_Enumerator3* m_ObjectEnumerator();
	void mark();
};
class bb_list_Node7 : public Object{
	public:
	bb_list_Node7* f__succ;
	bb_list_Node7* f__pred;
	bb_decl_ClassDecl* f__data;
	bb_list_Node7();
	bb_list_Node7* g_new(bb_list_Node7*,bb_list_Node7*,bb_decl_ClassDecl*);
	bb_list_Node7* g_new2();
	void mark();
};
class bb_list_HeadNode7 : public bb_list_Node7{
	public:
	bb_list_HeadNode7();
	bb_list_HeadNode7* g_new();
	void mark();
};
class bb_stack_Stack5 : public Object{
	public:
	Array<bb_decl_ArgDecl* > f_data;
	int f_length;
	bb_stack_Stack5();
	bb_stack_Stack5* g_new();
	bb_stack_Stack5* g_new2(Array<bb_decl_ArgDecl* >);
	virtual int m_Push13(bb_decl_ArgDecl*);
	virtual int m_Push14(Array<bb_decl_ArgDecl* >,int,int);
	virtual int m_Push15(Array<bb_decl_ArgDecl* >,int);
	virtual Array<bb_decl_ArgDecl* > m_ToArray();
	void mark();
};
class bb_list_List8 : public Object{
	public:
	bb_list_Node8* f__head;
	bb_list_List8();
	bb_list_List8* g_new();
	virtual bb_list_Node8* m_AddLast8(bb_decl_BlockDecl*);
	bb_list_List8* g_new2(Array<bb_decl_BlockDecl* >);
	virtual bb_decl_BlockDecl* m_RemoveLast();
	void mark();
};
class bb_list_Node8 : public Object{
	public:
	bb_list_Node8* f__succ;
	bb_list_Node8* f__pred;
	bb_decl_BlockDecl* f__data;
	bb_list_Node8();
	bb_list_Node8* g_new(bb_list_Node8*,bb_list_Node8*,bb_decl_BlockDecl*);
	bb_list_Node8* g_new2();
	virtual bb_list_Node8* m_GetNode();
	virtual bb_list_Node8* m_PrevNode();
	virtual int m_Remove();
	void mark();
};
class bb_list_HeadNode8 : public bb_list_Node8{
	public:
	bb_list_HeadNode8();
	bb_list_HeadNode8* g_new();
	virtual bb_list_Node8* m_GetNode();
	void mark();
};
class bb_stmt_DeclStmt : public bb_stmt_Stmt{
	public:
	bb_decl_Decl* f_decl;
	bb_stmt_DeclStmt();
	bb_stmt_DeclStmt* g_new(bb_decl_Decl*);
	bb_stmt_DeclStmt* g_new2(String,bb_type_Type*,bb_expr_Expr*);
	bb_stmt_DeclStmt* g_new3();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_ReturnStmt : public bb_stmt_Stmt{
	public:
	bb_expr_Expr* f_expr;
	bb_stmt_ReturnStmt();
	bb_stmt_ReturnStmt* g_new(bb_expr_Expr*);
	bb_stmt_ReturnStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_BreakStmt : public bb_stmt_Stmt{
	public:
	bb_stmt_BreakStmt();
	bb_stmt_BreakStmt* g_new();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_ContinueStmt : public bb_stmt_Stmt{
	public:
	bb_stmt_ContinueStmt();
	bb_stmt_ContinueStmt* g_new();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_IfStmt : public bb_stmt_Stmt{
	public:
	bb_expr_Expr* f_expr;
	bb_decl_BlockDecl* f_thenBlock;
	bb_decl_BlockDecl* f_elseBlock;
	bb_stmt_IfStmt();
	bb_stmt_IfStmt* g_new(bb_expr_Expr*,bb_decl_BlockDecl*,bb_decl_BlockDecl*);
	bb_stmt_IfStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_WhileStmt : public bb_stmt_Stmt{
	public:
	bb_expr_Expr* f_expr;
	bb_decl_BlockDecl* f_block;
	bb_stmt_WhileStmt();
	bb_stmt_WhileStmt* g_new(bb_expr_Expr*,bb_decl_BlockDecl*);
	bb_stmt_WhileStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_RepeatStmt : public bb_stmt_Stmt{
	public:
	bb_decl_BlockDecl* f_block;
	bb_expr_Expr* f_expr;
	bb_stmt_RepeatStmt();
	bb_stmt_RepeatStmt* g_new(bb_decl_BlockDecl*,bb_expr_Expr*);
	bb_stmt_RepeatStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_parser_ForEachinStmt : public bb_stmt_Stmt{
	public:
	String f_varid;
	bb_type_Type* f_varty;
	int f_varlocal;
	bb_expr_Expr* f_expr;
	bb_decl_BlockDecl* f_block;
	bb_parser_ForEachinStmt();
	bb_parser_ForEachinStmt* g_new(String,bb_type_Type*,int,bb_expr_Expr*,bb_decl_BlockDecl*);
	bb_parser_ForEachinStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_AssignStmt : public bb_stmt_Stmt{
	public:
	String f_op;
	bb_expr_Expr* f_lhs;
	bb_expr_Expr* f_rhs;
	bb_decl_LocalDecl* f_tmp1;
	bb_decl_LocalDecl* f_tmp2;
	bb_stmt_AssignStmt();
	bb_stmt_AssignStmt* g_new(String,bb_expr_Expr*,bb_expr_Expr*);
	bb_stmt_AssignStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_FixSideEffects();
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_ForStmt : public bb_stmt_Stmt{
	public:
	bb_stmt_Stmt* f_init;
	bb_expr_Expr* f_expr;
	bb_stmt_Stmt* f_incr;
	bb_decl_BlockDecl* f_block;
	bb_stmt_ForStmt();
	bb_stmt_ForStmt* g_new(bb_stmt_Stmt*,bb_expr_Expr*,bb_stmt_Stmt*,bb_decl_BlockDecl*);
	bb_stmt_ForStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_expr_VarExpr : public bb_expr_Expr{
	public:
	bb_decl_VarDecl* f_decl;
	bb_expr_VarExpr();
	bb_expr_VarExpr* g_new(bb_decl_VarDecl*);
	bb_expr_VarExpr* g_new2();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_ToString();
	virtual bool m_SideEffects();
	virtual bb_expr_Expr* m_SemantSet(String,bb_expr_Expr*);
	virtual String m_Trans();
	virtual String m_TransVar();
	void mark();
};
class bb_stmt_CatchStmt : public bb_stmt_Stmt{
	public:
	bb_decl_LocalDecl* f_init;
	bb_decl_BlockDecl* f_block;
	bb_stmt_CatchStmt();
	bb_stmt_CatchStmt* g_new(bb_decl_LocalDecl*,bb_decl_BlockDecl*);
	bb_stmt_CatchStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stack_Stack6 : public Object{
	public:
	Array<bb_stmt_CatchStmt* > f_data;
	int f_length;
	bb_stack_Stack6();
	bb_stack_Stack6* g_new();
	bb_stack_Stack6* g_new2(Array<bb_stmt_CatchStmt* >);
	virtual int m_Push16(bb_stmt_CatchStmt*);
	virtual int m_Push17(Array<bb_stmt_CatchStmt* >,int,int);
	virtual int m_Push18(Array<bb_stmt_CatchStmt* >,int);
	virtual int m_Length();
	virtual Array<bb_stmt_CatchStmt* > m_ToArray();
	void mark();
};
class bb_stmt_TryStmt : public bb_stmt_Stmt{
	public:
	bb_decl_BlockDecl* f_block;
	Array<bb_stmt_CatchStmt* > f_catches;
	bb_stmt_TryStmt();
	bb_stmt_TryStmt* g_new(bb_decl_BlockDecl*,Array<bb_stmt_CatchStmt* >);
	bb_stmt_TryStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_ThrowStmt : public bb_stmt_Stmt{
	public:
	bb_expr_Expr* f_expr;
	bb_stmt_ThrowStmt();
	bb_stmt_ThrowStmt* g_new(bb_expr_Expr*);
	bb_stmt_ThrowStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
class bb_stmt_ExprStmt : public bb_stmt_Stmt{
	public:
	bb_expr_Expr* f_expr;
	bb_stmt_ExprStmt();
	bb_stmt_ExprStmt* g_new(bb_expr_Expr*);
	bb_stmt_ExprStmt* g_new2();
	virtual bb_stmt_Stmt* m_OnCopy2(bb_decl_ScopeDecl*);
	virtual int m_OnSemant();
	virtual String m_Trans();
	void mark();
};
bb_decl_AppDecl* bb_parser_ParseApp(String);
class bb_reflector_Reflector : public Object{
	public:
	bool f_debug;
	bb_decl_ModuleDecl* f_refmod;
	bb_decl_ModuleDecl* f_langmod;
	bb_decl_ModuleDecl* f_boxesmod;
	bb_map_StringMap4* f_munged;
	bb_map_StringMap* f_modpaths;
	bb_map_StringMap* f_modexprs;
	bb_set_StringSet* f_refmods;
	bb_stack_Stack7* f_classdecls;
	bb_map_StringMap4* f_classids;
	bb_stack_StringStack* f_output;
	bb_reflector_Reflector();
	bb_reflector_Reflector* g_new();
	virtual String m_ModPath(bb_decl_ModuleDecl*);
	static bool g_MatchPath(String,String);
	virtual String m_Mung(String);
	virtual bool m_ValidClass(bb_decl_ClassDecl*);
	virtual String m_TypeExpr(bb_type_Type*,bool);
	virtual String m_DeclExpr(bb_decl_Decl*,bool);
	virtual int m_Emit(String);
	virtual bool m_ValidType(bb_type_Type*);
	virtual String m_TypeInfo(bb_type_Type*);
	virtual int m_Attrs(bb_decl_Decl*);
	virtual String m_Box(bb_type_Type*,String);
	virtual String m_Emit2(bb_decl_ConstDecl*);
	virtual String m_Unbox(bb_type_Type*,String);
	virtual String m_Emit3(bb_decl_ClassDecl*);
	virtual String m_Emit4(bb_decl_FuncDecl*);
	virtual String m_Emit5(bb_decl_FieldDecl*);
	virtual String m_Emit6(bb_decl_GlobalDecl*);
	virtual int m_Semant3(bb_decl_AppDecl*);
	void mark();
};
class bb_map_MapValues : public Object{
	public:
	bb_map_Map3* f_map;
	bb_map_MapValues();
	bb_map_MapValues* g_new(bb_map_Map3*);
	bb_map_MapValues* g_new2();
	virtual bb_map_ValueEnumerator* m_ObjectEnumerator();
	void mark();
};
class bb_map_ValueEnumerator : public Object{
	public:
	bb_map_Node3* f_node;
	bb_map_ValueEnumerator();
	bb_map_ValueEnumerator* g_new(bb_map_Node3*);
	bb_map_ValueEnumerator* g_new2();
	virtual bool m_HasNext();
	virtual bb_decl_ModuleDecl* m_NextObject();
	void mark();
};
class bb_map_Map4 : public Object{
	public:
	bb_map_Node4* f_root;
	bb_map_Map4();
	bb_map_Map4* g_new();
	virtual int m_Compare(String,String)=0;
	virtual bb_map_Node4* m_FindNode(String);
	virtual bool m_Contains(String);
	virtual int m_Get(String);
	virtual int m_RotateLeft4(bb_map_Node4*);
	virtual int m_RotateRight4(bb_map_Node4*);
	virtual int m_InsertFixup4(bb_map_Node4*);
	virtual bool m_Set4(String,int);
	void mark();
};
class bb_map_StringMap4 : public bb_map_Map4{
	public:
	bb_map_StringMap4();
	bb_map_StringMap4* g_new();
	virtual int m_Compare(String,String);
	void mark();
};
class bb_map_Node4 : public Object{
	public:
	String f_key;
	bb_map_Node4* f_right;
	bb_map_Node4* f_left;
	int f_value;
	int f_color;
	bb_map_Node4* f_parent;
	bb_map_Node4();
	bb_map_Node4* g_new(String,int,int,bb_map_Node4*);
	bb_map_Node4* g_new2();
	void mark();
};
class bb_list_Enumerator3 : public Object{
	public:
	bb_list_List7* f__list;
	bb_list_Node7* f__curr;
	bb_list_Enumerator3();
	bb_list_Enumerator3* g_new(bb_list_List7*);
	bb_list_Enumerator3* g_new2();
	virtual bool m_HasNext();
	virtual bb_decl_ClassDecl* m_NextObject();
	void mark();
};
class bb_stack_Stack7 : public Object{
	public:
	Array<bb_decl_ClassDecl* > f_data;
	int f_length;
	bb_stack_Stack7();
	bb_stack_Stack7* g_new();
	bb_stack_Stack7* g_new2(Array<bb_decl_ClassDecl* >);
	virtual int m_Length();
	virtual int m_Push19(bb_decl_ClassDecl*);
	virtual int m_Push20(Array<bb_decl_ClassDecl* >,int,int);
	virtual int m_Push21(Array<bb_decl_ClassDecl* >,int);
	virtual bb_decl_ClassDecl* m_Get2(int);
	void mark();
};
int bb_parser_ParseSource(String,bb_decl_AppDecl*,bb_decl_ModuleDecl*,int);
class bb_list_Enumerator4 : public Object{
	public:
	bb_list_List5* f__list;
	bb_list_Node5* f__curr;
	bb_list_Enumerator4();
	bb_list_Enumerator4* g_new(bb_list_List5*);
	bb_list_Enumerator4* g_new2();
	virtual bool m_HasNext();
	virtual String m_NextObject();
	void mark();
};
String bb_os_ExtractExt(String);
class bb_translator_Translator : public Object{
	public:
	bb_translator_Translator();
	virtual String m_TransApp(bb_decl_AppDecl*)=0;
	bb_translator_Translator* g_new();
	virtual String m_TransMemberVarExpr(bb_expr_MemberVarExpr*)=0;
	virtual String m_TransInvokeExpr(bb_expr_InvokeExpr*)=0;
	virtual String m_TransStmtExpr(bb_expr_StmtExpr*)=0;
	virtual String m_TransConstExpr(bb_expr_ConstExpr*)=0;
	virtual String m_TransUnaryExpr(bb_expr_UnaryExpr*)=0;
	virtual String m_TransArrayExpr(bb_expr_ArrayExpr*)=0;
	virtual String m_TransNewArrayExpr(bb_expr_NewArrayExpr*)=0;
	virtual String m_TransNewObjectExpr(bb_expr_NewObjectExpr*)=0;
	virtual String m_TransCastExpr(bb_expr_CastExpr*)=0;
	virtual String m_TransSelfExpr(bb_expr_SelfExpr*)=0;
	virtual String m_TransInvokeSuperExpr(bb_expr_InvokeSuperExpr*)=0;
	virtual String m_TransSliceExpr(bb_expr_SliceExpr*)=0;
	virtual String m_TransIndexExpr(bb_expr_IndexExpr*)=0;
	virtual String m_TransBinaryExpr(bb_expr_BinaryExpr*)=0;
	virtual String m_TransInvokeMemberExpr(bb_expr_InvokeMemberExpr*)=0;
	virtual String m_TransDeclStmt(bb_stmt_DeclStmt*)=0;
	virtual String m_TransReturnStmt(bb_stmt_ReturnStmt*)=0;
	virtual String m_TransBreakStmt(bb_stmt_BreakStmt*)=0;
	virtual String m_TransContinueStmt(bb_stmt_ContinueStmt*)=0;
	virtual String m_TransIfStmt(bb_stmt_IfStmt*)=0;
	virtual String m_TransWhileStmt(bb_stmt_WhileStmt*)=0;
	virtual String m_TransRepeatStmt(bb_stmt_RepeatStmt*)=0;
	virtual String m_TransBlock(bb_decl_BlockDecl*)=0;
	virtual String m_TransAssignStmt(bb_stmt_AssignStmt*)=0;
	virtual String m_TransForStmt(bb_stmt_ForStmt*)=0;
	virtual String m_TransVarExpr(bb_expr_VarExpr*)=0;
	virtual String m_TransTryStmt(bb_stmt_TryStmt*)=0;
	virtual String m_TransThrowStmt(bb_stmt_ThrowStmt*)=0;
	virtual String m_TransExprStmt(bb_stmt_ExprStmt*)=0;
	void mark();
};
extern bb_translator_Translator* bb_translator__trans;
Array<String > bb_os_LoadDir(String,bool,bool);
int bb_os_DeleteDir(String,bool);
int bb_os_CopyDir(String,String,bool,bool);
int bbMain();
class bb_translator_CTranslator : public bb_translator_Translator{
	public:
	bb_map_StringMap5* f_funcMungs;
	bb_map_StringMap6* f_mungedScopes;
	String f_indent;
	bb_list_StringList* f_lines;
	bool f_emitDebugInfo;
	int f_unreachable;
	int f_broken;
	bb_translator_CTranslator();
	bb_translator_CTranslator* g_new();
	virtual int m_MungMethodDecl(bb_decl_FuncDecl*);
	virtual int m_MungDecl(bb_decl_Decl*);
	virtual int m_Emit(String);
	virtual int m_BeginLocalScope();
	virtual String m_Bra(String);
	virtual int m_EmitEnter(bb_decl_FuncDecl*);
	virtual int m_EmitEnterBlock();
	virtual int m_EmitSetErr(String);
	virtual String m_TransLocalDecl(String,bb_expr_Expr*)=0;
	virtual String m_CreateLocal(bb_expr_Expr*);
	virtual String m_TransExprNS(bb_expr_Expr*);
	virtual int m_EmitLeave();
	virtual String m_TransValue(bb_type_Type*,String)=0;
	virtual int m_EmitLeaveBlock();
	virtual int m_EmitBlock(bb_decl_BlockDecl*,bool);
	virtual int m_EndLocalScope();
	virtual String m_JoinLines();
	virtual String m_Enquote(String);
	virtual String m_TransGlobal(bb_decl_GlobalDecl*)=0;
	virtual String m_TransField(bb_decl_FieldDecl*,bb_expr_Expr*)=0;
	virtual int m_ExprPri(bb_expr_Expr*);
	virtual String m_TransSubExpr(bb_expr_Expr*,int);
	virtual String m_TransStmtExpr(bb_expr_StmtExpr*);
	virtual String m_TransIntrinsicExpr(bb_decl_Decl*,bb_expr_Expr*,Array<bb_expr_Expr* >)=0;
	virtual String m_TransVarExpr(bb_expr_VarExpr*);
	virtual String m_TransMemberVarExpr(bb_expr_MemberVarExpr*);
	virtual String m_TransFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >,bb_expr_Expr*)=0;
	virtual String m_TransInvokeExpr(bb_expr_InvokeExpr*);
	virtual String m_TransInvokeMemberExpr(bb_expr_InvokeMemberExpr*);
	virtual String m_TransSuperFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >)=0;
	virtual String m_TransInvokeSuperExpr(bb_expr_InvokeSuperExpr*);
	virtual String m_TransExprStmt(bb_stmt_ExprStmt*);
	virtual String m_TransAssignOp(String);
	virtual String m_TransAssignStmt2(bb_stmt_AssignStmt*);
	virtual String m_TransAssignStmt(bb_stmt_AssignStmt*);
	virtual String m_TransReturnStmt(bb_stmt_ReturnStmt*);
	virtual String m_TransContinueStmt(bb_stmt_ContinueStmt*);
	virtual String m_TransBreakStmt(bb_stmt_BreakStmt*);
	virtual String m_TransBlock(bb_decl_BlockDecl*);
	virtual String m_TransDeclStmt(bb_stmt_DeclStmt*);
	virtual String m_TransIfStmt(bb_stmt_IfStmt*);
	virtual String m_TransWhileStmt(bb_stmt_WhileStmt*);
	virtual String m_TransRepeatStmt(bb_stmt_RepeatStmt*);
	virtual String m_TransForStmt(bb_stmt_ForStmt*);
	virtual String m_TransTryStmt(bb_stmt_TryStmt*);
	virtual String m_TransThrowStmt(bb_stmt_ThrowStmt*);
	virtual String m_TransUnaryOp(String);
	virtual String m_TransBinaryOp(String,String);
	void mark();
};
class bb_jstranslator_JsTranslator : public bb_translator_CTranslator{
	public:
	bb_jstranslator_JsTranslator();
	bb_jstranslator_JsTranslator* g_new();
	virtual int m_EmitFuncDecl(bb_decl_FuncDecl*);
	virtual int m_EmitClassDecl(bb_decl_ClassDecl*);
	virtual String m_TransApp(bb_decl_AppDecl*);
	virtual String m_TransValue(bb_type_Type*,String);
	virtual String m_TransLocalDecl(String,bb_expr_Expr*);
	virtual int m_EmitEnter(bb_decl_FuncDecl*);
	virtual int m_EmitSetErr(String);
	virtual int m_EmitLeave();
	virtual String m_TransGlobal(bb_decl_GlobalDecl*);
	virtual String m_TransField(bb_decl_FieldDecl*,bb_expr_Expr*);
	virtual String m_TransArgs(Array<bb_expr_Expr* >,String);
	virtual String m_TransFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >,bb_expr_Expr*);
	virtual String m_TransSuperFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >);
	virtual String m_TransConstExpr(bb_expr_ConstExpr*);
	virtual String m_TransNewObjectExpr(bb_expr_NewObjectExpr*);
	virtual String m_TransNewArrayExpr(bb_expr_NewArrayExpr*);
	virtual String m_TransSelfExpr(bb_expr_SelfExpr*);
	virtual String m_TransCastExpr(bb_expr_CastExpr*);
	virtual String m_TransUnaryExpr(bb_expr_UnaryExpr*);
	virtual String m_TransBinaryExpr(bb_expr_BinaryExpr*);
	virtual String m_TransIndexExpr(bb_expr_IndexExpr*);
	virtual String m_TransSliceExpr(bb_expr_SliceExpr*);
	virtual String m_TransArrayExpr(bb_expr_ArrayExpr*);
	virtual String m_TransTryStmt(bb_stmt_TryStmt*);
	virtual String m_TransAssignStmt(bb_stmt_AssignStmt*);
	virtual String m_TransIntrinsicExpr(bb_decl_Decl*,bb_expr_Expr*,Array<bb_expr_Expr* >);
	void mark();
};
bool bb_target_MatchPath(String,String);
String bb_target_ReplaceBlock(String,String,String,String);
String bb_config_Enquote(String,String);
class bb_astranslator_AsTranslator : public bb_translator_CTranslator{
	public:
	bb_astranslator_AsTranslator();
	bb_astranslator_AsTranslator* g_new();
	virtual String m_TransValue(bb_type_Type*,String);
	virtual String m_TransType(bb_type_Type*);
	virtual String m_TransLocalDecl(String,bb_expr_Expr*);
	virtual int m_EmitEnter(bb_decl_FuncDecl*);
	virtual int m_EmitSetErr(String);
	virtual int m_EmitLeave();
	virtual String m_TransValDecl(bb_decl_ValDecl*);
	virtual int m_EmitFuncDecl(bb_decl_FuncDecl*);
	virtual int m_EmitClassDecl(bb_decl_ClassDecl*);
	virtual String m_TransStatic(bb_decl_Decl*);
	virtual String m_TransGlobal(bb_decl_GlobalDecl*);
	virtual String m_TransApp(bb_decl_AppDecl*);
	virtual String m_TransField(bb_decl_FieldDecl*,bb_expr_Expr*);
	virtual String m_TransArgs2(Array<bb_expr_Expr* >);
	virtual String m_TransFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >,bb_expr_Expr*);
	virtual String m_TransSuperFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >);
	virtual String m_TransConstExpr(bb_expr_ConstExpr*);
	virtual String m_TransNewObjectExpr(bb_expr_NewObjectExpr*);
	virtual String m_TransNewArrayExpr(bb_expr_NewArrayExpr*);
	virtual String m_TransSelfExpr(bb_expr_SelfExpr*);
	virtual String m_TransCastExpr(bb_expr_CastExpr*);
	virtual String m_TransUnaryExpr(bb_expr_UnaryExpr*);
	virtual String m_TransBinaryExpr(bb_expr_BinaryExpr*);
	virtual String m_TransIndexExpr(bb_expr_IndexExpr*);
	virtual String m_TransSliceExpr(bb_expr_SliceExpr*);
	virtual String m_TransArrayExpr(bb_expr_ArrayExpr*);
	virtual String m_TransIntrinsicExpr(bb_decl_Decl*,bb_expr_Expr*,Array<bb_expr_Expr* >);
	virtual String m_TransTryStmt(bb_stmt_TryStmt*);
	virtual String m_TransAssignStmt(bb_stmt_AssignStmt*);
	void mark();
};
class bb_javatranslator_JavaTranslator : public bb_translator_CTranslator{
	public:
	int f_unsafe;
	bb_javatranslator_JavaTranslator();
	bb_javatranslator_JavaTranslator* g_new();
	virtual String m_TransType(bb_type_Type*);
	virtual String m_TransValue(bb_type_Type*,String);
	virtual String m_TransLocalDecl(String,bb_expr_Expr*);
	virtual int m_EmitEnter(bb_decl_FuncDecl*);
	virtual int m_EmitSetErr(String);
	virtual int m_EmitLeave();
	virtual String m_TransStatic(bb_decl_Decl*);
	virtual String m_TransGlobal(bb_decl_GlobalDecl*);
	virtual int m_EmitFuncDecl(bb_decl_FuncDecl*);
	virtual String m_TransDecl(bb_decl_Decl*);
	virtual int m_EmitClassDecl(bb_decl_ClassDecl*);
	virtual String m_TransApp(bb_decl_AppDecl*);
	virtual String m_TransField(bb_decl_FieldDecl*,bb_expr_Expr*);
	virtual String m_TransArgs2(Array<bb_expr_Expr* >);
	virtual String m_TransFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >,bb_expr_Expr*);
	virtual String m_TransSuperFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >);
	virtual String m_TransConstExpr(bb_expr_ConstExpr*);
	virtual String m_TransNewObjectExpr(bb_expr_NewObjectExpr*);
	virtual String m_TransNewArrayExpr(bb_expr_NewArrayExpr*);
	virtual String m_TransSelfExpr(bb_expr_SelfExpr*);
	virtual String m_TransCastExpr(bb_expr_CastExpr*);
	virtual String m_TransUnaryExpr(bb_expr_UnaryExpr*);
	virtual String m_TransBinaryExpr(bb_expr_BinaryExpr*);
	virtual String m_TransIndexExpr(bb_expr_IndexExpr*);
	virtual String m_TransSliceExpr(bb_expr_SliceExpr*);
	virtual String m_TransArrayExpr(bb_expr_ArrayExpr*);
	virtual String m_TransIntrinsicExpr(bb_decl_Decl*,bb_expr_Expr*,Array<bb_expr_Expr* >);
	virtual String m_TransTryStmt(bb_stmt_TryStmt*);
	void mark();
};
class bb_cpptranslator_CppTranslator : public bb_translator_CTranslator{
	public:
	bb_stack_Stack8* f_dbgLocals;
	String f_lastDbgInfo;
	int f_unsafe;
	bb_cpptranslator_CppTranslator();
	bb_cpptranslator_CppTranslator* g_new();
	virtual String m_TransType(bb_type_Type*);
	virtual String m_TransRefType(bb_type_Type*);
	virtual String m_TransValue(bb_type_Type*,String);
	virtual String m_TransLocalDecl(String,bb_expr_Expr*);
	virtual int m_BeginLocalScope();
	virtual int m_EndLocalScope();
	virtual int m_EmitEnter(bb_decl_FuncDecl*);
	virtual int m_EmitEnterBlock();
	virtual int m_EmitSetErr(String);
	virtual int m_EmitLeaveBlock();
	virtual String m_TransStatic(bb_decl_Decl*);
	virtual String m_TransGlobal(bb_decl_GlobalDecl*);
	virtual int m_EmitFuncProto(bb_decl_FuncDecl*);
	virtual int m_EmitClassProto(bb_decl_ClassDecl*);
	virtual int m_EmitFuncDecl(bb_decl_FuncDecl*);
	virtual String m_TransField(bb_decl_FieldDecl*,bb_expr_Expr*);
	virtual int m_EmitMark(String,bb_type_Type*,bool);
	virtual int m_EmitClassDecl(bb_decl_ClassDecl*);
	virtual String m_TransApp(bb_decl_AppDecl*);
	virtual int m_CheckSafe(bb_decl_Decl*);
	virtual String m_TransArgs3(Array<bb_expr_Expr* >,bb_decl_FuncDecl*);
	virtual String m_TransFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >,bb_expr_Expr*);
	virtual String m_TransSuperFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >);
	virtual String m_TransConstExpr(bb_expr_ConstExpr*);
	virtual String m_TransNewObjectExpr(bb_expr_NewObjectExpr*);
	virtual String m_TransNewArrayExpr(bb_expr_NewArrayExpr*);
	virtual String m_TransSelfExpr(bb_expr_SelfExpr*);
	virtual String m_TransCastExpr(bb_expr_CastExpr*);
	virtual String m_TransUnaryExpr(bb_expr_UnaryExpr*);
	virtual String m_TransBinaryExpr(bb_expr_BinaryExpr*);
	virtual String m_TransIndexExpr(bb_expr_IndexExpr*);
	virtual String m_TransSliceExpr(bb_expr_SliceExpr*);
	virtual String m_TransArrayExpr(bb_expr_ArrayExpr*);
	virtual String m_TransIntrinsicExpr(bb_decl_Decl*,bb_expr_Expr*,Array<bb_expr_Expr* >);
	virtual String m_TransTryStmt(bb_stmt_TryStmt*);
	virtual String m_TransDeclStmt(bb_stmt_DeclStmt*);
	virtual String m_TransAssignStmt2(bb_stmt_AssignStmt*);
	void mark();
};
class bb_cstranslator_CsTranslator : public bb_translator_CTranslator{
	public:
	bb_cstranslator_CsTranslator();
	bb_cstranslator_CsTranslator* g_new();
	virtual String m_TransType(bb_type_Type*);
	virtual String m_TransValue(bb_type_Type*,String);
	virtual String m_TransLocalDecl(String,bb_expr_Expr*);
	virtual int m_EmitEnter(bb_decl_FuncDecl*);
	virtual int m_EmitSetErr(String);
	virtual int m_EmitLeave();
	virtual String m_TransStatic(bb_decl_Decl*);
	virtual String m_TransGlobal(bb_decl_GlobalDecl*);
	virtual String m_TransField(bb_decl_FieldDecl*,bb_expr_Expr*);
	virtual int m_EmitFuncDecl(bb_decl_FuncDecl*);
	virtual String m_TransDecl(bb_decl_Decl*);
	virtual int m_EmitClassDecl(bb_decl_ClassDecl*);
	virtual String m_TransApp(bb_decl_AppDecl*);
	virtual String m_TransArgs2(Array<bb_expr_Expr* >);
	virtual String m_TransFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >,bb_expr_Expr*);
	virtual String m_TransSuperFunc(bb_decl_FuncDecl*,Array<bb_expr_Expr* >);
	virtual String m_TransConstExpr(bb_expr_ConstExpr*);
	virtual String m_TransNewObjectExpr(bb_expr_NewObjectExpr*);
	virtual String m_TransNewArrayExpr(bb_expr_NewArrayExpr*);
	virtual String m_TransSelfExpr(bb_expr_SelfExpr*);
	virtual String m_TransCastExpr(bb_expr_CastExpr*);
	virtual String m_TransUnaryExpr(bb_expr_UnaryExpr*);
	virtual String m_TransBinaryExpr(bb_expr_BinaryExpr*);
	virtual String m_TransIndexExpr(bb_expr_IndexExpr*);
	virtual String m_TransSliceExpr(bb_expr_SliceExpr*);
	virtual String m_TransArrayExpr(bb_expr_ArrayExpr*);
	virtual String m_TransIntrinsicExpr(bb_decl_Decl*,bb_expr_Expr*,Array<bb_expr_Expr* >);
	virtual String m_TransTryStmt(bb_stmt_TryStmt*);
	void mark();
};
class bb_list_Enumerator5 : public Object{
	public:
	bb_list_List4* f__list;
	bb_list_Node4* f__curr;
	bb_list_Enumerator5();
	bb_list_Enumerator5* g_new(bb_list_List4*);
	bb_list_Enumerator5* g_new2();
	virtual bool m_HasNext();
	virtual bb_stmt_Stmt* m_NextObject();
	void mark();
};
class bb_list_List9 : public Object{
	public:
	bb_list_Node9* f__head;
	bb_list_List9();
	bb_list_List9* g_new();
	virtual bb_list_Node9* m_AddLast9(bb_decl_ModuleDecl*);
	bb_list_List9* g_new2(Array<bb_decl_ModuleDecl* >);
	virtual bool m_IsEmpty();
	virtual bb_decl_ModuleDecl* m_RemoveLast();
	void mark();
};
class bb_list_Node9 : public Object{
	public:
	bb_list_Node9* f__succ;
	bb_list_Node9* f__pred;
	bb_decl_ModuleDecl* f__data;
	bb_list_Node9();
	bb_list_Node9* g_new(bb_list_Node9*,bb_list_Node9*,bb_decl_ModuleDecl*);
	bb_list_Node9* g_new2();
	virtual bb_list_Node9* m_GetNode();
	virtual bb_list_Node9* m_PrevNode();
	virtual int m_Remove();
	void mark();
};
class bb_list_HeadNode9 : public bb_list_Node9{
	public:
	bb_list_HeadNode9();
	bb_list_HeadNode9* g_new();
	virtual bb_list_Node9* m_GetNode();
	void mark();
};
class bb_expr_MemberVarExpr : public bb_expr_Expr{
	public:
	bb_expr_Expr* f_expr;
	bb_decl_VarDecl* f_decl;
	bb_expr_MemberVarExpr();
	bb_expr_MemberVarExpr* g_new(bb_expr_Expr*,bb_decl_VarDecl*);
	bb_expr_MemberVarExpr* g_new2();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_ToString();
	virtual bool m_SideEffects();
	virtual bb_expr_Expr* m_SemantSet(String,bb_expr_Expr*);
	virtual String m_Trans();
	virtual String m_TransVar();
	void mark();
};
class bb_expr_InvokeExpr : public bb_expr_Expr{
	public:
	bb_decl_FuncDecl* f_decl;
	Array<bb_expr_Expr* > f_args;
	bb_expr_InvokeExpr();
	bb_expr_InvokeExpr* g_new(bb_decl_FuncDecl*,Array<bb_expr_Expr* >);
	bb_expr_InvokeExpr* g_new2();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_ToString();
	virtual String m_Trans();
	virtual String m_TransStmt();
	void mark();
};
class bb_expr_StmtExpr : public bb_expr_Expr{
	public:
	bb_stmt_Stmt* f_stmt;
	bb_expr_Expr* f_expr;
	bb_expr_StmtExpr();
	bb_expr_StmtExpr* g_new(bb_stmt_Stmt*,bb_expr_Expr*);
	bb_expr_StmtExpr* g_new2();
	virtual bb_expr_Expr* m_Copy();
	virtual String m_ToString();
	virtual bb_expr_Expr* m_Semant();
	virtual String m_Trans();
	void mark();
};
extern int bb_decl__loopnest;
class bb_map_Map5 : public Object{
	public:
	bb_map_Node5* f_root;
	bb_map_Map5();
	bb_map_Map5* g_new();
	virtual int m_Compare(String,String)=0;
	virtual bb_map_Node5* m_FindNode(String);
	virtual bb_decl_FuncDeclList* m_Get(String);
	virtual int m_RotateLeft5(bb_map_Node5*);
	virtual int m_RotateRight5(bb_map_Node5*);
	virtual int m_InsertFixup5(bb_map_Node5*);
	virtual bool m_Set5(String,bb_decl_FuncDeclList*);
	void mark();
};
class bb_map_StringMap5 : public bb_map_Map5{
	public:
	bb_map_StringMap5();
	bb_map_StringMap5* g_new();
	virtual int m_Compare(String,String);
	void mark();
};
class bb_map_Node5 : public Object{
	public:
	String f_key;
	bb_map_Node5* f_right;
	bb_map_Node5* f_left;
	bb_decl_FuncDeclList* f_value;
	int f_color;
	bb_map_Node5* f_parent;
	bb_map_Node5();
	bb_map_Node5* g_new(String,bb_decl_FuncDeclList*,int,bb_map_Node5*);
	bb_map_Node5* g_new2();
	void mark();
};
class bb_map_Map6 : public Object{
	public:
	bb_map_Node6* f_root;
	bb_map_Map6();
	bb_map_Map6* g_new();
	virtual int m_Compare(String,String)=0;
	virtual bb_map_Node6* m_FindNode(String);
	virtual bb_set_StringSet* m_Get(String);
	virtual int m_RotateLeft6(bb_map_Node6*);
	virtual int m_RotateRight6(bb_map_Node6*);
	virtual int m_InsertFixup6(bb_map_Node6*);
	virtual bool m_Set6(String,bb_set_StringSet*);
	void mark();
};
class bb_map_StringMap6 : public bb_map_Map6{
	public:
	bb_map_StringMap6();
	bb_map_StringMap6* g_new();
	virtual int m_Compare(String,String);
	void mark();
};
class bb_map_Node6 : public Object{
	public:
	String f_key;
	bb_map_Node6* f_right;
	bb_map_Node6* f_left;
	bb_set_StringSet* f_value;
	int f_color;
	bb_map_Node6* f_parent;
	bb_map_Node6();
	bb_map_Node6* g_new(String,bb_set_StringSet*,int,bb_map_Node6*);
	bb_map_Node6* g_new2();
	void mark();
};
class bb_list_Enumerator6 : public Object{
	public:
	bb_list_List6* f__list;
	bb_list_Node6* f__curr;
	bb_list_Enumerator6();
	bb_list_Enumerator6* g_new(bb_list_List6*);
	bb_list_Enumerator6* g_new2();
	virtual bool m_HasNext();
	virtual bb_decl_GlobalDecl* m_NextObject();
	void mark();
};
class bb_stack_Stack8 : public Object{
	public:
	Array<bb_decl_LocalDecl* > f_data;
	int f_length;
	bb_stack_Stack8();
	bb_stack_Stack8* g_new();
	bb_stack_Stack8* g_new2(Array<bb_decl_LocalDecl* >);
	virtual int m_Clear();
	virtual bb_stack_Enumerator* m_ObjectEnumerator();
	virtual int m_Length();
	virtual bb_decl_LocalDecl* m_Get2(int);
	virtual int m_Push22(bb_decl_LocalDecl*);
	virtual int m_Push23(Array<bb_decl_LocalDecl* >,int,int);
	virtual int m_Push24(Array<bb_decl_LocalDecl* >,int);
	void mark();
};
class bb_stack_Enumerator : public Object{
	public:
	bb_stack_Stack8* f_stack;
	int f_index;
	bb_stack_Enumerator();
	bb_stack_Enumerator* g_new(bb_stack_Stack8*);
	bb_stack_Enumerator* g_new2();
	virtual bool m_HasNext();
	virtual bb_decl_LocalDecl* m_NextObject();
	void mark();
};
class bb_stack_Stack9 : public Object{
	public:
	Array<bb_decl_FuncDecl* > f_data;
	int f_length;
	bb_stack_Stack9();
	bb_stack_Stack9* g_new();
	bb_stack_Stack9* g_new2(Array<bb_decl_FuncDecl* >);
	virtual int m_Push25(bb_decl_FuncDecl*);
	virtual int m_Push26(Array<bb_decl_FuncDecl* >,int,int);
	virtual int m_Push27(Array<bb_decl_FuncDecl* >,int);
	virtual bb_stack_Enumerator2* m_ObjectEnumerator();
	virtual int m_Length();
	virtual bb_decl_FuncDecl* m_Get2(int);
	void mark();
};
class bb_stack_Enumerator2 : public Object{
	public:
	bb_stack_Stack9* f_stack;
	int f_index;
	bb_stack_Enumerator2();
	bb_stack_Enumerator2* g_new(bb_stack_Stack9*);
	bb_stack_Enumerator2* g_new2();
	virtual bool m_HasNext();
	virtual bb_decl_FuncDecl* m_NextObject();
	void mark();
};
String bb_os_ExtractDir(String t_path){
	int t_i=t_path.FindLast(String(L"/",1));
	if(t_i==-1){
		t_i=t_path.FindLast(String(L"\\",1));
	}
	if(t_i!=-1){
		return t_path.Slice(0,t_i);
	}
	return String();
}
int bb_target_Die(String t_msg){
	Print(String(L"TRANS FAILED: ",14)+t_msg);
	ExitApp(-1);
	return 0;
}
bb_map_Map::bb_map_Map(){
	f_root=0;
}
bb_map_Map* bb_map_Map::g_new(){
	return this;
}
int bb_map_Map::m_RotateLeft(bb_map_Node* t_node){
	bb_map_Node* t_child=t_node->f_right;
	gc_assign(t_node->f_right,t_child->f_left);
	if((t_child->f_left)!=0){
		gc_assign(t_child->f_left->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_left){
			gc_assign(t_node->f_parent->f_left,t_child);
		}else{
			gc_assign(t_node->f_parent->f_right,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_left,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map::m_RotateRight(bb_map_Node* t_node){
	bb_map_Node* t_child=t_node->f_left;
	gc_assign(t_node->f_left,t_child->f_right);
	if((t_child->f_right)!=0){
		gc_assign(t_child->f_right->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_right){
			gc_assign(t_node->f_parent->f_right,t_child);
		}else{
			gc_assign(t_node->f_parent->f_left,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_right,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map::m_InsertFixup(bb_map_Node* t_node){
	while(((t_node->f_parent)!=0) && t_node->f_parent->f_color==-1 && ((t_node->f_parent->f_parent)!=0)){
		if(t_node->f_parent==t_node->f_parent->f_parent->f_left){
			bb_map_Node* t_uncle=t_node->f_parent->f_parent->f_right;
			if(((t_uncle)!=0) && t_uncle->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle->f_color=1;
				t_uncle->f_parent->f_color=-1;
				t_node=t_uncle->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_right){
					t_node=t_node->f_parent;
					m_RotateLeft(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateRight(t_node->f_parent->f_parent);
			}
		}else{
			bb_map_Node* t_uncle2=t_node->f_parent->f_parent->f_left;
			if(((t_uncle2)!=0) && t_uncle2->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle2->f_color=1;
				t_uncle2->f_parent->f_color=-1;
				t_node=t_uncle2->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_left){
					t_node=t_node->f_parent;
					m_RotateRight(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateLeft(t_node->f_parent->f_parent);
			}
		}
	}
	f_root->f_color=1;
	return 0;
}
bool bb_map_Map::m_Set(String t_key,String t_value){
	bb_map_Node* t_node=f_root;
	bb_map_Node* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				t_node->f_value=t_value;
				return false;
			}
		}
	}
	t_node=(new bb_map_Node)->g_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->f_right,t_node);
		}else{
			gc_assign(t_parent->f_left,t_node);
		}
		m_InsertFixup(t_node);
	}else{
		gc_assign(f_root,t_node);
	}
	return true;
}
bb_map_Node* bb_map_Map::m_FindNode(String t_key){
	bb_map_Node* t_node=f_root;
	while((t_node)!=0){
		int t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
String bb_map_Map::m_Get(String t_key){
	bb_map_Node* t_node=m_FindNode(t_key);
	if((t_node)!=0){
		return t_node->f_value;
	}
	return String();
}
bb_map_Node* bb_map_Map::m_FirstNode(){
	if(!((f_root)!=0)){
		return 0;
	}
	bb_map_Node* t_node=f_root;
	while((t_node->f_left)!=0){
		t_node=t_node->f_left;
	}
	return t_node;
}
bb_map_NodeEnumerator* bb_map_Map::m_ObjectEnumerator(){
	return (new bb_map_NodeEnumerator)->g_new(m_FirstNode());
}
bool bb_map_Map::m_Contains(String t_key){
	return m_FindNode(t_key)!=0;
}
void bb_map_Map::mark(){
	Object::mark();
	gc_mark_q(f_root);
}
bb_map_StringMap::bb_map_StringMap(){
}
bb_map_StringMap* bb_map_StringMap::g_new(){
	bb_map_Map::g_new();
	return this;
}
int bb_map_StringMap::m_Compare(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void bb_map_StringMap::mark(){
	bb_map_Map::mark();
}
bb_map_StringMap* bb_config__cfgVars;
bb_map_Node::bb_map_Node(){
	f_key=String();
	f_right=0;
	f_left=0;
	f_value=String();
	f_color=0;
	f_parent=0;
}
bb_map_Node* bb_map_Node::g_new(String t_key,String t_value,int t_color,bb_map_Node* t_parent){
	this->f_key=t_key;
	this->f_value=t_value;
	this->f_color=t_color;
	gc_assign(this->f_parent,t_parent);
	return this;
}
bb_map_Node* bb_map_Node::g_new2(){
	return this;
}
bb_map_Node* bb_map_Node::m_NextNode(){
	bb_map_Node* t_node=0;
	if((f_right)!=0){
		t_node=f_right;
		while((t_node->f_left)!=0){
			t_node=t_node->f_left;
		}
		return t_node;
	}
	t_node=this;
	bb_map_Node* t_parent=this->f_parent;
	while(((t_parent)!=0) && t_node==t_parent->f_right){
		t_node=t_parent;
		t_parent=t_parent->f_parent;
	}
	return t_parent;
}
String bb_map_Node::m_Key(){
	return f_key;
}
String bb_map_Node::m_Value(){
	return f_value;
}
void bb_map_Node::mark(){
	Object::mark();
	gc_mark_q(f_right);
	gc_mark_q(f_left);
	gc_mark_q(f_parent);
}
int bb_config_SetCfgVar(String t_key,String t_val){
	bb_config__cfgVars->m_Set(t_key,t_val);
	return 0;
}
bb_stack_Stack::bb_stack_Stack(){
	f_data=Array<String >();
	f_length=0;
}
bb_stack_Stack* bb_stack_Stack::g_new(){
	return this;
}
bb_stack_Stack* bb_stack_Stack::g_new2(Array<String > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack::m_Push(String t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	f_data[f_length]=t_value;
	f_length+=1;
	return 0;
}
int bb_stack_Stack::m_Push2(Array<String > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack::m_Push3(Array<String > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push(t_values[t_i]);
	}
	return 0;
}
bool bb_stack_Stack::m_IsEmpty(){
	return f_length==0;
}
Array<String > bb_stack_Stack::m_ToArray(){
	Array<String > t_t=Array<String >(f_length);
	for(int t_i=0;t_i<f_length;t_i=t_i+1){
		t_t[t_i]=f_data[t_i];
	}
	return t_t;
}
int bb_stack_Stack::m_Length(){
	return f_length;
}
String bb_stack_Stack::m_Get2(int t_index){
	return f_data[t_index];
}
String bb_stack_Stack::m_Pop(){
	f_length-=1;
	return f_data[f_length];
}
void bb_stack_Stack::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
bb_stack_StringStack::bb_stack_StringStack(){
}
bb_stack_StringStack* bb_stack_StringStack::g_new(){
	bb_stack_Stack::g_new();
	return this;
}
String bb_stack_StringStack::m_Join(String t_separator){
	return t_separator.Join(m_ToArray());
}
void bb_stack_StringStack::mark(){
	bb_stack_Stack::mark();
}
String bb_config_GetCfgVar(String t_key){
	return bb_config__cfgVars->m_Get(t_key);
}
String bb_target_ReplaceEnv(String t_str){
	bb_stack_StringStack* t_bits=(new bb_stack_StringStack)->g_new();
	do{
		int t_i=t_str.Find(String(L"${",2),0);
		if(t_i==-1){
			break;
		}
		int t_e=t_str.Find(String(L"}",1),t_i+2);
		if(t_e==-1){
			break;
		}
		if(t_i>=2 && t_str.Slice(t_i-2,t_i)==String(L"//",2)){
			t_bits->m_Push(t_str.Slice(0,t_e+1));
			t_str=t_str.Slice(t_e+1);
			continue;
		}
		String t_t=t_str.Slice(t_i+2,t_e);
		String t_v=bb_config_GetCfgVar(t_t);
		if(!((t_v).Length()!=0)){
			t_v=GetEnv(t_t);
		}
		t_v=t_v.Replace(String(L"\"",1),String());
		t_bits->m_Push(t_str.Slice(0,t_i));
		t_bits->m_Push(t_v);
		t_str=t_str.Slice(t_e+1);
	}while(!(false));
	if(t_bits->m_IsEmpty()){
		return t_str;
	}
	t_bits->m_Push(t_str);
	return t_bits->m_Join(String());
}
String bb_trans2_StripQuotes(String t_str){
	if(t_str.StartsWith(String(L"\"",1)) && t_str.EndsWith(String(L"\"",1))){
		return t_str.Slice(1,-1);
	}
	return t_str;
}
String bb_target_ANDROID_PATH;
String bb_target_JDK_PATH;
String bb_target_ANT_PATH;
String bb_target_FLEX_PATH;
String bb_target_MINGW_PATH;
String bb_target_PSM_PATH;
String bb_target_MSBUILD_PATH;
String bb_target_HTML_PLAYER;
String bb_target_FLASH_PLAYER;
int bb_trans2_LoadConfig(){
	String t_CONFIG_FILE=String();
	for(int t_i=1;t_i<AppArgs().Length();t_i=t_i+1){
		if(AppArgs()[t_i].ToLower().StartsWith(String(L"-cfgfile=",9))){
			t_CONFIG_FILE=AppArgs()[t_i].Slice(9);
			break;
		}
	}
	String t_cfgpath=bb_os_ExtractDir(AppPath())+String(L"/",1);
	if((t_CONFIG_FILE).Length()!=0){
		t_cfgpath=t_cfgpath+t_CONFIG_FILE;
	}else{
		t_cfgpath=t_cfgpath+(String(L"config.",7)+HostOS()+String(L".txt",4));
	}
	if(FileType(t_cfgpath)!=1){
		bb_target_Die(String(L"Failed to open config file",26));
	}
	String t_cfg=LoadString(t_cfgpath);
	bb_config_SetCfgVar(String(L"TRANSDIR",8),bb_os_ExtractDir(AppPath()));
	Array<String > t_=t_cfg.Split(String(L"\n",1));
	int t_2=0;
	while(t_2<t_.Length()){
		String t_line=t_[t_2];
		t_2=t_2+1;
		t_line=t_line.Trim();
		if(!((t_line).Length()!=0) || t_line.StartsWith(String(L"'",1))){
			continue;
		}
		int t_i2=t_line.Find(String(L"=",1),0);
		if(t_i2==-1){
			bb_target_Die(String(L"Error in config file, line=",27)+t_line);
		}
		String t_lhs=t_line.Slice(0,t_i2).Trim();
		String t_rhs=t_line.Slice(t_i2+1).Trim();
		t_rhs=bb_target_ReplaceEnv(t_rhs);
		String t_path=bb_trans2_StripQuotes(t_rhs);
		while(t_path.EndsWith(String(L"/",1)) || t_path.EndsWith(String(L"\\",1))){
			t_path=t_path.Slice(0,-1);
		}
		String t_3=t_lhs;
		if(t_3==String(L"ANDROID_PATH",12)){
			if(!((bb_target_ANDROID_PATH).Length()!=0) && FileType(t_path)==2){
				bb_target_ANDROID_PATH=t_path;
			}
		}else{
			if(t_3==String(L"JDK_PATH",8)){
				if(!((bb_target_JDK_PATH).Length()!=0) && FileType(t_path)==2){
					bb_target_JDK_PATH=t_path;
				}
			}else{
				if(t_3==String(L"ANT_PATH",8)){
					if(!((bb_target_ANT_PATH).Length()!=0) && FileType(t_path)==2){
						bb_target_ANT_PATH=t_path;
					}
				}else{
					if(t_3==String(L"FLEX_PATH",9)){
						if(!((bb_target_FLEX_PATH).Length()!=0) && FileType(t_path)==2){
							bb_target_FLEX_PATH=t_path;
						}
					}else{
						if(t_3==String(L"MINGW_PATH",10)){
							if(!((bb_target_MINGW_PATH).Length()!=0) && FileType(t_path)==2){
								bb_target_MINGW_PATH=t_path;
							}
						}else{
							if(t_3==String(L"PSM_PATH",8)){
								if(!((bb_target_PSM_PATH).Length()!=0) && FileType(t_path)==2){
									bb_target_PSM_PATH=t_path;
								}
							}else{
								if(t_3==String(L"MSBUILD_PATH",12)){
									if(!((bb_target_MSBUILD_PATH).Length()!=0) && FileType(t_path)==1){
										bb_target_MSBUILD_PATH=t_path;
									}
								}else{
									if(t_3==String(L"HTML_PLAYER",11)){
										bb_target_HTML_PLAYER=t_rhs;
									}else{
										if(t_3==String(L"FLASH_PLAYER",12)){
											bb_target_FLASH_PLAYER=t_rhs;
										}else{
											Print(String(L"Trans: ignoring unrecognized config var: ",41)+t_lhs);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	String t_4=HostOS();
	if(t_4==String(L"winnt",5)){
		String t_path2=GetEnv(String(L"PATH",4));
		if((bb_target_ANDROID_PATH).Length()!=0){
			t_path2=t_path2+(String(L";",1)+bb_target_ANDROID_PATH+String(L"/tools",6));
		}
		if((bb_target_ANDROID_PATH).Length()!=0){
			t_path2=t_path2+(String(L";",1)+bb_target_ANDROID_PATH+String(L"/platform-tools",15));
		}
		if((bb_target_JDK_PATH).Length()!=0){
			t_path2=t_path2+(String(L";",1)+bb_target_JDK_PATH+String(L"/bin",4));
		}
		if((bb_target_ANT_PATH).Length()!=0){
			t_path2=t_path2+(String(L";",1)+bb_target_ANT_PATH+String(L"/bin",4));
		}
		if((bb_target_FLEX_PATH).Length()!=0){
			t_path2=t_path2+(String(L";",1)+bb_target_FLEX_PATH+String(L"/bin",4));
		}
		if((bb_target_MINGW_PATH).Length()!=0){
			t_path2=bb_target_MINGW_PATH+String(L"/bin;",5)+t_path2;
		}
		SetEnv(String(L"PATH",4),t_path2);
		if((bb_target_JDK_PATH).Length()!=0){
			SetEnv(String(L"JAVA_HOME",9),bb_target_JDK_PATH);
		}
	}else{
		if(t_4==String(L"macos",5)){
			String t_path3=GetEnv(String(L"PATH",4));
			if((bb_target_ANDROID_PATH).Length()!=0){
				t_path3=t_path3+(String(L":",1)+bb_target_ANDROID_PATH+String(L"/tools",6));
			}
			if((bb_target_ANDROID_PATH).Length()!=0){
				t_path3=t_path3+(String(L":",1)+bb_target_ANDROID_PATH+String(L"/platform-tools",15));
			}
			if((bb_target_FLEX_PATH).Length()!=0){
				t_path3=t_path3+(String(L":",1)+bb_target_FLEX_PATH+String(L"/bin",4));
			}
			SetEnv(String(L"PATH",4),t_path3);
		}
	}
	bb_config_SetCfgVar(String(L"TRANSDIR",8),String());
	return 1;
}
bb_target_Target::bb_target_Target(){
	f_srcPath=String();
	f_app=0;
	f_transCode=String();
	f_TEXT_FILES=String();
	f_IMAGE_FILES=String();
	f_SOUND_FILES=String();
	f_MUSIC_FILES=String();
	f_BINARY_FILES=String();
	f_DATA_FILES=String();
	f_dataFiles=(new bb_map_StringMap)->g_new();
}
bb_target_Target* bb_target_Target::g_new(){
	return this;
}
int bb_target_Target::m_AddTransCode(String t_tcode){
	if(f_transCode.Contains(String(L"${CODE}",7))){
		f_transCode=f_transCode.Replace(String(L"${CODE}",7),t_tcode);
	}else{
		f_transCode=f_transCode+t_tcode;
	}
	return 0;
}
int bb_target_Target::m_Translate(){
	if(bb_target_OPT_ACTION>=1){
		Print(String(L"Parsing...",10));
		gc_assign(f_app,bb_parser_ParseApp(f_srcPath));
		if(bb_target_OPT_ACTION>=2){
			Print(String(L"Semanting...",12));
			if((bb_config_GetCfgVar(String(L"REFLECTION_FILTER",17))).Length()!=0){
				bb_reflector_Reflector* t_r=(new bb_reflector_Reflector)->g_new();
				t_r->m_Semant3(f_app);
			}else{
				f_app->m_Semant();
			}
			if(bb_target_OPT_ACTION>=3){
				Print(String(L"Translating...",14));
				bb_list_Enumerator4* t_=f_app->f_fileImports->m_ObjectEnumerator();
				while(t_->m_HasNext()){
					String t_file=t_->m_NextObject();
					if(bb_os_ExtractExt(t_file).ToLower()==bb_config_ENV_LANG){
						m_AddTransCode(LoadString(t_file));
					}
				}
				m_AddTransCode(bb_translator__trans->m_TransApp(f_app));
			}
		}
	}
	return 0;
}
int bb_target_Target::m_Make(String t_path){
	m_Begin();
	f_srcPath=t_path;
	bb_config_SetCfgVar(String(L"HOST",4),bb_config_ENV_HOST);
	bb_config_SetCfgVar(String(L"LANG",4),bb_config_ENV_LANG);
	bb_config_SetCfgVar(String(L"TARGET",6),bb_config_ENV_TARGET);
	bb_config_SetCfgVar(String(L"CONFIG",6),bb_config_ENV_CONFIG);
	bb_config_SetCfgVar(String(L"SAFEMODE",8),String(bb_config_ENV_SAFEMODE));
	bb_config_SetCfgVar(String(L"MODPATH",7),bb_target_OPT_MODPATH);
	m_Translate();
	if(bb_target_OPT_ACTION>=4){
		Print(String(L"Building...",11));
		String t_buildPath=bb_os_StripExt(f_srcPath)+String(L".build",6);
		String t_targetPath=t_buildPath+String(L"/",1)+bb_config_ENV_TARGET;
		if((bb_target_OPT_CLEAN)!=0){
			bb_os_DeleteDir(t_targetPath,true);
			if(FileType(t_targetPath)!=0){
				bb_target_Die(String(L"Failed to clean target dir",26));
			}
		}
		if(FileType(t_targetPath)==0){
			if(FileType(t_buildPath)==0){
				CreateDir(t_buildPath);
			}
			if(FileType(t_buildPath)!=2){
				bb_target_Die(String(L"Failed to create build dir: ",28)+t_buildPath);
			}
			if(!((bb_os_CopyDir(bb_os_ExtractDir(AppPath())+String(L"/../targets/",12)+bb_config_ENV_TARGET,t_targetPath,true,false))!=0)){
				bb_target_Die(String(L"Failed to copy target dir",25));
			}
		}
		if(FileType(t_targetPath)!=2){
			bb_target_Die(String(L"Failed to create target dir: ",29)+t_targetPath);
		}
		String t_cfgPath=t_targetPath+String(L"/CONFIG.MONKEY",14);
		if(FileType(t_cfgPath)==1){
			bb_preprocessor_PreProcess(t_cfgPath);
		}
		f_TEXT_FILES=bb_config_GetCfgVar(String(L"TEXT_FILES",10));
		f_IMAGE_FILES=bb_config_GetCfgVar(String(L"IMAGE_FILES",11));
		f_SOUND_FILES=bb_config_GetCfgVar(String(L"SOUND_FILES",11));
		f_MUSIC_FILES=bb_config_GetCfgVar(String(L"MUSIC_FILES",11));
		f_BINARY_FILES=bb_config_GetCfgVar(String(L"BINARY_FILES",12));
		f_DATA_FILES=f_TEXT_FILES;
		if((f_IMAGE_FILES).Length()!=0){
			f_DATA_FILES=f_DATA_FILES+(String(L"|",1)+f_IMAGE_FILES);
		}
		if((f_SOUND_FILES).Length()!=0){
			f_DATA_FILES=f_DATA_FILES+(String(L"|",1)+f_SOUND_FILES);
		}
		if((f_MUSIC_FILES).Length()!=0){
			f_DATA_FILES=f_DATA_FILES+(String(L"|",1)+f_MUSIC_FILES);
		}
		if((f_BINARY_FILES).Length()!=0){
			f_DATA_FILES=f_DATA_FILES+(String(L"|",1)+f_BINARY_FILES);
		}
		String t_cd=CurrentDir();
		ChangeDir(t_targetPath);
		m_MakeTarget();
		ChangeDir(t_cd);
	}
	return 0;
}
int bb_target_Target::m_CreateDataDir(String t_dir){
	t_dir=RealPath(t_dir);
	bb_os_DeleteDir(t_dir,true);
	CreateDir(t_dir);
	String t_dataPath=bb_os_StripExt(f_srcPath)+String(L".data",5);
	if(FileType(t_dataPath)==2){
		bb_stack_StringStack* t_srcs=(new bb_stack_StringStack)->g_new();
		t_srcs->m_Push(t_dataPath);
		while(!t_srcs->m_IsEmpty()){
			String t_src=t_srcs->m_Pop();
			Array<String > t_=LoadDir(t_src);
			int t_2=0;
			while(t_2<t_.Length()){
				String t_f=t_[t_2];
				t_2=t_2+1;
				if(t_f.StartsWith(String(L".",1))){
					continue;
				}
				String t_p=t_src+String(L"/",1)+t_f;
				String t_r=t_p.Slice(t_dataPath.Length()+1);
				String t_t=t_dir+String(L"/",1)+t_r;
				int t_3=FileType(t_p);
				if(t_3==1){
					if(bb_target_MatchPath(t_r,f_DATA_FILES)){
						CopyFile(t_p,t_t);
						f_dataFiles->m_Set(t_p,t_r);
					}
				}else{
					if(t_3==2){
						CreateDir(t_t);
						t_srcs->m_Push(t_p);
					}
				}
			}
		}
	}
	bb_list_Enumerator4* t_4=f_app->f_fileImports->m_ObjectEnumerator();
	while(t_4->m_HasNext()){
		String t_p2=t_4->m_NextObject();
		String t_r2=bb_os_StripDir(t_p2);
		String t_t2=t_dir+String(L"/",1)+t_r2;
		if(bb_target_MatchPath(t_r2,f_DATA_FILES)){
			CopyFile(t_p2,t_t2);
			f_dataFiles->m_Set(t_p2,t_r2);
		}
	}
	return 0;
}
int bb_target_Target::m_Execute(String t_cmd,int t_failHard){
	int t_r=Execute(t_cmd);
	if(!((t_r)!=0)){
		return 1;
	}
	if((t_failHard)!=0){
		bb_target_Die(String(L"TRANS Failed to execute '",25)+t_cmd+String(L"', return code=",15)+String(t_r));
	}
	return 0;
}
void bb_target_Target::mark(){
	Object::mark();
	gc_mark_q(f_app);
	gc_mark_q(f_dataFiles);
}
bb_html5_Html5Target::bb_html5_Html5Target(){
}
int bb_html5_Html5Target::g_IsValid(){
	if(FileType(String(L"html5",5))!=2){
		return 0;
	}
	return ((bb_target_HTML_PLAYER!=String())?1:0);
}
bb_html5_Html5Target* bb_html5_Html5Target::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_html5_Html5Target::m_Begin(){
	bb_config_ENV_TARGET=String(L"html5",5);
	bb_config_ENV_LANG=String(L"js",2);
	gc_assign(bb_translator__trans,((new bb_jstranslator_JsTranslator)->g_new()));
	return 0;
}
String bb_html5_Html5Target::m_MetaData(){
	bb_stack_StringStack* t_meta=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=f_dataFiles->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		String t_src=t_kv->m_Key();
		String t_ext=bb_os_ExtractExt(t_src).ToLower();
		String t_2=t_ext;
		if(t_2==String(L"png",3) || t_2==String(L"jpg",3) || t_2==String(L"gif",3)){
			info_width=0;
			info_height=0;
			String t_3=t_ext;
			if(t_3==String(L"png",3)){
				get_info_png(t_src);
			}else{
				if(t_3==String(L"jpg",3)){
					get_info_jpg(t_src);
				}else{
					if(t_3==String(L"gif",3)){
						get_info_gif(t_src);
					}
				}
			}
			if(info_width==0 || info_height==0){
				bb_target_Die(String(L"Unable to load image file '",27)+t_src+String(L"'.",2));
			}
			t_meta->m_Push(String(L"[",1)+t_kv->m_Value()+String(L"];type=image/",13)+t_ext+String(L";",1));
			t_meta->m_Push(String(L"width=",6)+String(info_width)+String(L";",1));
			t_meta->m_Push(String(L"height=",7)+String(info_height)+String(L";",1));
			t_meta->m_Push(String(L"\\n",2));
		}
	}
	return t_meta->m_Join(String());
}
String bb_html5_Html5Target::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"CFG_",4)+t_kv->m_Key()+String(L"=",1)+bb_config_Enquote(t_kv->m_Value(),String(L"js",2))+String(L";",1));
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_html5_Html5Target::m_MakeTarget(){
	m_CreateDataDir(String(L"data",4));
	String t_meta=String(L"var META_DATA=\"",15)+m_MetaData()+String(L"\";\n",3);
	String t_main=LoadString(String(L"main.js",7));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"METADATA",8),t_meta,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"main.js",7));
	if(bb_target_OPT_ACTION>=6){
		String t_p=RealPath(String(L"MonkeyGame.html",15));
		String t_t=bb_target_HTML_PLAYER+String(L" \"",2)+t_p+String(L"\"",1);
		m_Execute(t_t,0);
	}
	return 0;
}
void bb_html5_Html5Target::mark(){
	bb_target_Target::mark();
}
bb_flash_FlashTarget::bb_flash_FlashTarget(){
}
int bb_flash_FlashTarget::g_IsValid(){
	if(FileType(String(L"flash",5))!=2){
		return 0;
	}
	return ((bb_target_FLEX_PATH!=String() && (bb_target_FLASH_PLAYER!=String() || bb_target_HTML_PLAYER!=String()))?1:0);
}
bb_flash_FlashTarget* bb_flash_FlashTarget::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_flash_FlashTarget::m_Begin(){
	bb_config_ENV_TARGET=String(L"flash",5);
	bb_config_ENV_LANG=String(L"as",2);
	gc_assign(bb_translator__trans,((new bb_astranslator_AsTranslator)->g_new()));
	return 0;
}
String bb_flash_FlashTarget::m_Assets(){
	bb_stack_StringStack* t_assets=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=f_dataFiles->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		String t_ext=bb_os_ExtractExt(t_kv->m_Value());
		String t_munged=String(L"_",1);
		Array<String > t_2=bb_os_StripExt(t_kv->m_Value()).Split(String(L"/",1));
		int t_3=0;
		while(t_3<t_2.Length()){
			String t_q=t_2[t_3];
			t_3=t_3+1;
			for(int t_i=0;t_i<t_q.Length();t_i=t_i+1){
				if(((bb_config_IsAlpha((int)t_q[t_i]))!=0) || ((bb_config_IsDigit((int)t_q[t_i]))!=0) || (int)t_q[t_i]==95){
					continue;
				}
				bb_target_Die(String(L"Invalid character in flash filename: ",37)+t_kv->m_Value()+String(L".",1));
			}
			t_munged=t_munged+(String(t_q.Length())+t_q);
		}
		t_munged=t_munged+(String(t_ext.Length())+t_ext);
		String t_4=t_ext.ToLower();
		if(t_4==String(L"png",3) || t_4==String(L"jpg",3) || t_4==String(L"mp3",3)){
			t_assets->m_Push(String(L"[Embed(source=\"data/",20)+t_kv->m_Value()+String(L"\")]",3));
			t_assets->m_Push(String(L"public static var ",18)+t_munged+String(L":Class;",7));
		}else{
			t_assets->m_Push(String(L"[Embed(source=\"data/",20)+t_kv->m_Value()+String(L"\",mimeType=\"application/octet-stream\")]",39));
			t_assets->m_Push(String(L"public static var ",18)+t_munged+String(L":Class;",7));
		}
	}
	return t_assets->m_Join(String(L"\n",1));
}
String bb_flash_FlashTarget::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"internal static var ",20)+t_kv->m_Key()+String(L":String=",8)+bb_config_Enquote(t_kv->m_Value(),String(L"as",2)));
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_flash_FlashTarget::m_MakeTarget(){
	m_CreateDataDir(String(L"data",4));
	String t_main=LoadString(String(L"MonkeyGame.as",13));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"ASSETS",6),m_Assets(),String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"MonkeyGame.as",13));
	if(bb_target_OPT_ACTION>=5){
		DeleteFile(String(L"main.swf",8));
		m_Execute(String(L"mxmlc -static-link-runtime-shared-libraries=true -frame=MonkeyGame,MonkeyGame Preloader.as",90),1);
		if(bb_target_OPT_ACTION>=6){
			if((bb_target_FLASH_PLAYER).Length()!=0){
				m_Execute(bb_target_FLASH_PLAYER+String(L" \"",2)+RealPath(String(L"Preloader.swf",13))+String(L"\"",1),0);
			}else{
				if((bb_target_HTML_PLAYER).Length()!=0){
					m_Execute(bb_target_HTML_PLAYER+String(L" \"",2)+RealPath(String(L"MonkeyGame.html",15))+String(L"\"",1),0);
				}
			}
		}
	}
	return 0;
}
void bb_flash_FlashTarget::mark(){
	bb_target_Target::mark();
}
bb_android_AndroidTarget::bb_android_AndroidTarget(){
}
int bb_android_AndroidTarget::g_IsValid(){
	if(FileType(String(L"android",7))!=2){
		return 0;
	}
	String t_=HostOS();
	if(t_==String(L"winnt",5)){
		if(((bb_target_ANDROID_PATH).Length()!=0) && ((bb_target_JDK_PATH).Length()!=0) && ((bb_target_ANT_PATH).Length()!=0)){
			return 1;
		}
	}else{
		if(t_==String(L"macos",5)){
			if((bb_target_ANDROID_PATH).Length()!=0){
				return 1;
			}
		}
	}
	return 0;
}
bb_android_AndroidTarget* bb_android_AndroidTarget::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_android_AndroidTarget::m_Begin(){
	bb_config_ENV_TARGET=String(L"android",7);
	bb_config_ENV_LANG=String(L"java",4);
	gc_assign(bb_translator__trans,((new bb_javatranslator_JavaTranslator)->g_new()));
	return 0;
}
String bb_android_AndroidTarget::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"static final String ",20)+t_kv->m_Key()+String(L"=",1)+bb_config_Enquote(t_kv->m_Value(),String(L"java",4))+String(L";",1));
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_android_AndroidTarget::m_MakeTarget(){
	m_CreateDataDir(String(L"assets/monkey",13));
	String t_app_label=bb_config_GetCfgVar(String(L"ANDROID_APP_LABEL",17));
	String t_app_package=bb_config_GetCfgVar(String(L"ANDROID_APP_PACKAGE",19));
	bb_config_SetCfgVar(String(L"ANDROID_SDK_DIR",15),bb_target_ANDROID_PATH.Replace(String(L"\\",1),String(L"\\\\",2)));
	Array<String > t_=bb_os_LoadDir(String(L"templates",9),true,false);
	int t_2=0;
	while(t_2<t_.Length()){
		String t_file=t_[t_2];
		t_2=t_2+1;
		String t_str=LoadString(String(L"templates/",10)+t_file);
		t_str=bb_target_ReplaceEnv(t_str);
		SaveString(t_str,t_file);
	}
	String t_jpath=String(L"src",3);
	bb_os_DeleteDir(t_jpath,true);
	CreateDir(t_jpath);
	Array<String > t_3=t_app_package.Split(String(L".",1));
	int t_4=0;
	while(t_4<t_3.Length()){
		String t_t=t_3[t_4];
		t_4=t_4+1;
		t_jpath=t_jpath+(String(L"/",1)+t_t);
		CreateDir(t_jpath);
	}
	t_jpath=t_jpath+String(L"/MonkeyGame.java",16);
	String t_main=LoadString(String(L"MonkeyGame.java",15));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	bb_stack_StringStack* t_imps=(new bb_stack_StringStack)->g_new();
	bb_set_StringSet* t_done=(new bb_set_StringSet)->g_new();
	bb_stack_StringStack* t_out=(new bb_stack_StringStack)->g_new();
	Array<String > t_5=t_main.Split(String(L"\n",1));
	int t_6=0;
	while(t_6<t_5.Length()){
		String t_line=t_5[t_6];
		t_6=t_6+1;
		if(t_line.StartsWith(String(L"import ",7))){
			int t_i=t_line.Find(String(L";",1),7);
			if(t_i!=-1){
				String t_id=t_line.Slice(7,t_i+1);
				if(!t_done->m_Contains(t_id)){
					t_done->m_Insert(t_id);
					t_imps->m_Push(String(L"import ",7)+t_id);
				}
			}
		}else{
			t_out->m_Push(t_line);
		}
	}
	t_main=t_out->m_Join(String(L"\n",1));
	t_main=bb_target_ReplaceBlock(t_main,String(L"IMPORTS",7),t_imps->m_Join(String(L"\n",1)),String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"PACKAGE",7),String(L"package ",8)+t_app_package+String(L";",1),String(L"\n//",3));
	SaveString(t_main,t_jpath);
	if(bb_config_GetCfgVar(String(L"ANDROID_NATIVE_GL_ENABLED",25))==String(L"1",1)){
		bb_os_CopyDir(String(L"nativegl/libs",13),String(L"libs",4),true,false);
		CreateDir(String(L"src/com",7));
		CreateDir(String(L"src/com/monkey",14));
		CopyFile(String(L"nativegl/NativeGL.java",22),String(L"src/com/monkey/NativeGL.java",28));
	}else{
		DeleteFile(String(L"libs/armeabi/libnativegl.so",27));
		DeleteFile(String(L"libs/armeabi-v7a/libnativegl.so",31));
		DeleteFile(String(L"libs/x86/libnativegl.so",23));
	}
	if(bb_target_OPT_ACTION>=5){
		m_Execute(String(L"adb start-server",16),1);
		int t_r=((((m_Execute(String(L"ant clean",9),0))!=0) && ((m_Execute(String(L"ant debug install",17),0))!=0))?1:0);
		m_Execute(String(L"adb kill-server",15),0);
		if(!((t_r)!=0)){
			bb_target_Die(String(L"Android build failed.",21));
		}else{
			if(bb_target_OPT_ACTION>=6){
				m_Execute(String(L"adb shell am start -n ",22)+t_app_package+String(L"/",1)+t_app_package+String(L".MonkeyGame",11),0);
				m_Execute(String(L"adb kill-server",15),0);
			}
		}
	}
	return 0;
}
void bb_android_AndroidTarget::mark(){
	bb_target_Target::mark();
}
bb_glfw_GlfwTarget::bb_glfw_GlfwTarget(){
}
int bb_glfw_GlfwTarget::g_IsValid(){
	if(FileType(String(L"glfw",4))!=2){
		return 0;
	}
	String t_=HostOS();
	if(t_==String(L"winnt",5)){
		if((bb_target_MINGW_PATH).Length()!=0){
			return 1;
		}
		if((bb_target_MSBUILD_PATH).Length()!=0){
			return 1;
		}
	}else{
		if(t_==String(L"macos",5)){
			return 1;
		}
	}
	return 0;
}
bb_glfw_GlfwTarget* bb_glfw_GlfwTarget::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_glfw_GlfwTarget::m_Begin(){
	bb_config_ENV_TARGET=String(L"glfw",4);
	bb_config_ENV_LANG=String(L"cpp",3);
	gc_assign(bb_translator__trans,((new bb_cpptranslator_CppTranslator)->g_new()));
	return 0;
}
String bb_glfw_GlfwTarget::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"#define CFG_",12)+t_kv->m_Key()+String(L" ",1)+t_kv->m_Value());
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_glfw_GlfwTarget::m_MakeMingw(){
	CreateDir(String(L"mingw/",6)+bb_target_CASED_CONFIG);
	CreateDir(String(L"mingw/",6)+bb_target_CASED_CONFIG+String(L"/internal",9));
	CreateDir(String(L"mingw/",6)+bb_target_CASED_CONFIG+String(L"/external",9));
	m_CreateDataDir(String(L"mingw/",6)+bb_target_CASED_CONFIG+String(L"/data",5));
	String t_main=LoadString(String(L"main.cpp",8));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"main.cpp",8));
	if(bb_target_OPT_ACTION>=5){
		ChangeDir(String(L"mingw",5));
		String t_ccopts=String();
		String t_=bb_config_ENV_CONFIG;
		if(t_==String(L"release",7)){
			t_ccopts=String(L"-O3 -DNDEBUG",12);
		}
		m_Execute(String(L"mingw32-make CCOPTS=\"",21)+t_ccopts+String(L"\" OUT=\"",7)+bb_target_CASED_CONFIG+String(L"/MonkeyGame\"",12),1);
		if(bb_target_OPT_ACTION>=6){
			ChangeDir(bb_target_CASED_CONFIG);
			m_Execute(String(L"MonkeyGame",10),1);
		}
	}
	return 0;
}
int bb_glfw_GlfwTarget::m_MakeVc2010(){
	CreateDir(String(L"vc2010/",7)+bb_target_CASED_CONFIG);
	CreateDir(String(L"vc2010/",7)+bb_target_CASED_CONFIG+String(L"/internal",9));
	CreateDir(String(L"vc2010/",7)+bb_target_CASED_CONFIG+String(L"/external",9));
	m_CreateDataDir(String(L"vc2010/",7)+bb_target_CASED_CONFIG+String(L"/data",5));
	String t_main=LoadString(String(L"main.cpp",8));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"main.cpp",8));
	if(bb_target_OPT_ACTION>=5){
		ChangeDir(String(L"vc2010",6));
		m_Execute(bb_target_MSBUILD_PATH+String(L" /p:Configuration=",18)+bb_target_CASED_CONFIG+String(L";Platform=\"win32\" MonkeyGame.sln",32),1);
		if(bb_target_OPT_ACTION>=6){
			ChangeDir(bb_target_CASED_CONFIG);
			m_Execute(String(L"MonkeyGame",10),1);
		}
	}
	return 0;
}
int bb_glfw_GlfwTarget::m_MakeXcode(){
	m_CreateDataDir(String(L"xcode/data",10));
	String t_main=LoadString(String(L"main.cpp",8));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"main.cpp",8));
	if(bb_target_OPT_ACTION>=5){
		ChangeDir(String(L"xcode",5));
		m_Execute(String(L"xcodebuild -configuration ",26)+bb_target_CASED_CONFIG,1);
		if(bb_target_OPT_ACTION>=6){
			ChangeDir(String(L"build/",6)+bb_target_CASED_CONFIG);
			ChangeDir(String(L"MonkeyGame.app/Contents/MacOS",29));
			m_Execute(String(L"./MonkeyGame",12),1);
		}
	}
	return 0;
}
int bb_glfw_GlfwTarget::m_MakeTarget(){
	String t_=bb_config_ENV_HOST;
	if(t_==String(L"winnt",5)){
		if(((bb_target_MINGW_PATH).Length()!=0) && ((bb_target_MSBUILD_PATH).Length()!=0)){
			if(bb_config_GetCfgVar(String(L"GLFW_USE_MINGW",14))==String(L"1",1)){
				m_MakeMingw();
			}else{
				m_MakeVc2010();
			}
		}else{
			if((bb_target_MINGW_PATH).Length()!=0){
				m_MakeMingw();
			}else{
				if((bb_target_MSBUILD_PATH).Length()!=0){
					m_MakeVc2010();
				}
			}
		}
	}else{
		if(t_==String(L"macos",5)){
			m_MakeXcode();
		}
	}
	return 0;
}
void bb_glfw_GlfwTarget::mark(){
	bb_target_Target::mark();
}
bb_xna_XnaTarget::bb_xna_XnaTarget(){
}
int bb_xna_XnaTarget::g_IsValid(){
	if(FileType(String(L"xna",3))!=2){
		return 0;
	}
	String t_=HostOS();
	if(t_==String(L"winnt",5)){
		if((bb_target_MSBUILD_PATH).Length()!=0){
			return 1;
		}
	}
	return 0;
}
bb_xna_XnaTarget* bb_xna_XnaTarget::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_xna_XnaTarget::m_Begin(){
	bb_config_ENV_TARGET=String(L"xna",3);
	bb_config_ENV_LANG=String(L"cs",2);
	gc_assign(bb_translator__trans,((new bb_cstranslator_CsTranslator)->g_new()));
	return 0;
}
String bb_xna_XnaTarget::m_Content(){
	bb_stack_StringStack* t_cont=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=f_dataFiles->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		String t_p=t_kv->m_Key();
		String t_r=t_kv->m_Value();
		String t_f=bb_os_StripDir(t_r);
		String t_t=(String(L"monkey/",7)+t_r).Replace(String(L"/",1),String(L"\\",1));
		String t_ext=bb_os_ExtractExt(t_r).ToLower();
		if(bb_target_MatchPath(t_r,f_TEXT_FILES)){
			t_cont->m_Push(String(L"  <ItemGroup>",13));
			t_cont->m_Push(String(L"    <Content Include=\"",22)+t_t+String(L"\">",2));
			t_cont->m_Push(String(L"      <Name>",12)+t_f+String(L"</Name>",7));
			t_cont->m_Push(String(L"      <CopyToOutputDirectory>Always</CopyToOutputDirectory>",59));
			t_cont->m_Push(String(L"    </Content>",14));
			t_cont->m_Push(String(L"  </ItemGroup>",14));
		}else{
			if(bb_target_MatchPath(t_r,f_IMAGE_FILES)){
				String t_2=t_ext;
				if(t_2==String(L"bmp",3) || t_2==String(L"dds",3) || t_2==String(L"dib",3) || t_2==String(L"hdr",3) || t_2==String(L"jpg",3) || t_2==String(L"pfm",3) || t_2==String(L"png",3) || t_2==String(L"ppm",3) || t_2==String(L"tga",3)){
					t_cont->m_Push(String(L"  <ItemGroup>",13));
					t_cont->m_Push(String(L"    <Compile Include=\"",22)+t_t+String(L"\">",2));
					t_cont->m_Push(String(L"      <Name>",12)+t_f+String(L"</Name>",7));
					t_cont->m_Push(String(L"      <Importer>TextureImporter</Importer>",42));
					t_cont->m_Push(String(L"      <Processor>TextureProcessor</Processor>",45));
					t_cont->m_Push(String(L"      <ProcessorParameters_ColorKeyEnabled>False</ProcessorParameters_ColorKeyEnabled>",86));
					t_cont->m_Push(String(L"      <ProcessorParameters_PremultiplyAlpha>False</ProcessorParameters_PremultiplyAlpha>",88));
					t_cont->m_Push(String(L"\t   </Compile>",14));
					t_cont->m_Push(String(L"  </ItemGroup>",14));
				}else{
					bb_target_Die(String(L"Invalid image file type",23));
				}
			}else{
				if(bb_target_MatchPath(t_r,f_SOUND_FILES)){
					String t_3=t_ext;
					if(t_3==String(L"wav",3) || t_3==String(L"mp3",3) || t_3==String(L"wma",3)){
						String t_imp=t_ext.Slice(0,1).ToUpper()+t_ext.Slice(1)+String(L"Importer",8);
						t_cont->m_Push(String(L"  <ItemGroup>",13));
						t_cont->m_Push(String(L"    <Compile Include=\"",22)+t_t+String(L"\">",2));
						t_cont->m_Push(String(L"      <Name>",12)+t_f+String(L"</Name>",7));
						t_cont->m_Push(String(L"      <Importer>",16)+t_imp+String(L"</Importer>",11));
						t_cont->m_Push(String(L"      <Processor>SoundEffectProcessor</Processor>",49));
						t_cont->m_Push(String(L"\t   </Compile>",14));
						t_cont->m_Push(String(L"  </ItemGroup>",14));
					}else{
						bb_target_Die(String(L"Invalid sound file type",23));
					}
				}else{
					if(bb_target_MatchPath(t_r,f_MUSIC_FILES)){
						String t_4=t_ext;
						if(t_4==String(L"wav",3) || t_4==String(L"mp3",3) || t_4==String(L"wma",3)){
							String t_imp2=t_ext.Slice(0,1).ToUpper()+t_ext.Slice(1)+String(L"Importer",8);
							t_cont->m_Push(String(L"  <ItemGroup>",13));
							t_cont->m_Push(String(L"    <Compile Include=\"",22)+t_t+String(L"\">",2));
							t_cont->m_Push(String(L"      <Name>",12)+t_f+String(L"</Name>",7));
							t_cont->m_Push(String(L"      <Importer>",16)+t_imp2+String(L"</Importer>",11));
							t_cont->m_Push(String(L"      <Processor>SongProcessor</Processor>",42));
							t_cont->m_Push(String(L"\t   </Compile>",14));
							t_cont->m_Push(String(L"  </ItemGroup>",14));
						}else{
							bb_target_Die(String(L"Invalid music file type",23));
						}
					}else{
						if(bb_target_MatchPath(t_r,f_BINARY_FILES)){
							t_cont->m_Push(String(L"  <ItemGroup>",13));
							t_cont->m_Push(String(L"    <Content Include=\"",22)+t_t+String(L"\">",2));
							t_cont->m_Push(String(L"      <Name>",12)+t_f+String(L"</Name>",7));
							t_cont->m_Push(String(L"      <CopyToOutputDirectory>Always</CopyToOutputDirectory>",59));
							t_cont->m_Push(String(L"    </Content>",14));
							t_cont->m_Push(String(L"  </ItemGroup>",14));
						}
					}
				}
			}
		}
	}
	return t_cont->m_Join(String(L"\n",1));
}
String bb_xna_XnaTarget::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"public const String ",20)+t_kv->m_Key()+String(L"=",1)+bb_config_Enquote(t_kv->m_Value(),String(L"cs",2))+String(L";",1));
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_xna_XnaTarget::m_MakeTarget(){
	m_CreateDataDir(String(L"MonkeyGame/MonkeyGameContent/monkey",35));
	String t_contproj=LoadString(String(L"MonkeyGame/MonkeyGameContent/MonkeyGameContent.contentproj",58));
	t_contproj=bb_target_ReplaceBlock(t_contproj,String(L"CONTENT",7),m_Content(),String(L"\n<!-- ",6));
	SaveString(t_contproj,String(L"MonkeyGame/MonkeyGameContent/MonkeyGameContent.contentproj",58));
	String t_main=LoadString(String(L"MonkeyGame/MonkeyGame/Program.cs",32));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"MonkeyGame/MonkeyGame/Program.cs",32));
	if(bb_target_OPT_ACTION>=5){
		m_Execute(bb_target_MSBUILD_PATH+String(L" /t:MonkeyGame /p:Configuration=",32)+bb_target_CASED_CONFIG+String(L" MonkeyGame.sln",15),1);
		if(bb_target_OPT_ACTION>=6){
			ChangeDir(String(L"MonkeyGame/MonkeyGame/bin/x86/",30)+bb_target_CASED_CONFIG);
			m_Execute(String(L"MonkeyGame",10),0);
		}
	}
	return 0;
}
void bb_xna_XnaTarget::mark(){
	bb_target_Target::mark();
}
bb_ios_IosTarget::bb_ios_IosTarget(){
}
int bb_ios_IosTarget::g_IsValid(){
	if(FileType(String(L"ios",3))!=2){
		return 0;
	}
	String t_=HostOS();
	if(t_==String(L"macos",5)){
		return 1;
	}
	return 0;
}
bb_ios_IosTarget* bb_ios_IosTarget::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_ios_IosTarget::m_Begin(){
	bb_config_ENV_TARGET=String(L"ios",3);
	bb_config_ENV_LANG=String(L"cpp",3);
	gc_assign(bb_translator__trans,((new bb_cpptranslator_CppTranslator)->g_new()));
	return 0;
}
String bb_ios_IosTarget::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"#define CFG_",12)+t_kv->m_Key()+String(L" ",1)+t_kv->m_Value());
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_ios_IosTarget::m_MakeTarget(){
	m_CreateDataDir(String(L"data",4));
	String t_main=LoadString(String(L"main.mm",7));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"main.mm",7));
	if(bb_target_OPT_ACTION>=5){
		m_Execute(String(L"xcodebuild -configuration ",26)+bb_target_CASED_CONFIG+String(L" -sdk iphonesimulator",21),1);
		if(bb_target_OPT_ACTION>=6){
			String t_home=GetEnv(String(L"HOME",4));
			String t_uuid=String(L"00C69C9A-C9DE-11DF-B3BE-5540E0D72085",36);
			String t_src=String(L"build/",6)+bb_target_CASED_CONFIG+String(L"-iphonesimulator/MonkeyGame.app",31);
			if(FileType(String(L"/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/Applications/iPhone Simulator.app",121))==2){
				String t_dst=String();
				t_dst=t_home+String(L"/Library/Application Support/iPhone Simulator/6.0",49);
				if(FileType(t_dst)!=2){
					t_dst=t_home+String(L"/Library/Application Support/iPhone Simulator/5.1",49);
					if(FileType(t_dst)!=2){
						bb_target_Die(String(L"Can't find dir:",15)+t_dst);
					}
				}
				t_dst=t_dst+String(L"/Applications",13);
				CreateDir(t_dst);
				if(FileType(t_dst)!=2){
					bb_target_Die(String(L"Failed to create dir:",21)+t_dst);
				}
				t_dst=t_dst+(String(L"/",1)+t_uuid);
				if(!((bb_os_DeleteDir(t_dst,true))!=0)){
					bb_target_Die(String(L"Failed to delete dir:",21)+t_dst);
				}
				if(!((CreateDir(t_dst))!=0)){
					bb_target_Die(String(L"Failed to create dir:",21)+t_dst);
				}
				m_Execute(String(L"cp -r \"",7)+t_src+String(L"\" \"",3)+t_dst+String(L"/MonkeyGame.app\"",16),1);
				CreateDir(t_dst+String(L"/Documents",10));
				m_Execute(String(L"killall \"iPhone Simulator\" 2>/dev/null",38),0);
				m_Execute(String(L"open \"/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/Applications/iPhone Simulator.app\"",128),1);
			}else{
				if(FileType(String(L"/Developer/Platforms/iPhoneSimulator.platform/Developer/Applications/iPhone Simulator.app",89))==2){
					String t_dst2=t_home+String(L"/Library/Application Support/iPhone Simulator/4.3.2",51);
					if(FileType(t_dst2)==0){
						t_dst2=t_home+String(L"/Library/Application Support/iPhone Simulator/4.3",49);
						if(FileType(t_dst2)==0){
							t_dst2=t_home+String(L"/Library/Application Support/iPhone Simulator/4.2",49);
						}
					}
					CreateDir(t_dst2);
					t_dst2=t_dst2+String(L"/Applications",13);
					CreateDir(t_dst2);
					t_dst2=t_dst2+(String(L"/",1)+t_uuid);
					if(!((bb_os_DeleteDir(t_dst2,true))!=0)){
						bb_target_Die(String(L"Failed to delete dir:",21)+t_dst2);
					}
					if(!((CreateDir(t_dst2))!=0)){
						bb_target_Die(String(L"Failed to create dir:",21)+t_dst2);
					}
					m_Execute(String(L"cp -r \"",7)+t_src+String(L"\" \"",3)+t_dst2+String(L"/MonkeyGame.app\"",16),1);
					m_Execute(String(L"killall \"iPhone Simulator\" 2>/dev/null",38),0);
					m_Execute(String(L"open \"/Developer/Platforms/iPhoneSimulator.platform/Developer/Applications/iPhone Simulator.app\"",96),1);
				}
			}
		}
	}
	return 0;
}
void bb_ios_IosTarget::mark(){
	bb_target_Target::mark();
}
bb_stdcpp_StdcppTarget::bb_stdcpp_StdcppTarget(){
}
int bb_stdcpp_StdcppTarget::g_IsValid(){
	if(FileType(String(L"stdcpp",6))!=2){
		return 0;
	}
	String t_=HostOS();
	if(t_==String(L"winnt",5)){
		if((bb_target_MINGW_PATH).Length()!=0){
			return 1;
		}
	}else{
		if(t_==String(L"macos",5)){
			return 1;
		}else{
			if(t_==String(L"linux",5)){
				return 1;
			}
		}
	}
	return 0;
}
bb_stdcpp_StdcppTarget* bb_stdcpp_StdcppTarget::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_stdcpp_StdcppTarget::m_Begin(){
	bb_config_ENV_TARGET=String(L"stdcpp",6);
	bb_config_ENV_LANG=String(L"cpp",3);
	gc_assign(bb_translator__trans,((new bb_cpptranslator_CppTranslator)->g_new()));
	return 0;
}
String bb_stdcpp_StdcppTarget::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"#define CFG_",12)+t_kv->m_Key()+String(L" ",1)+t_kv->m_Value());
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_stdcpp_StdcppTarget::m_MakeTarget(){
	String t_=bb_config_ENV_CONFIG;
	if(t_==String(L"debug",5)){
		bb_config_SetCfgVar(String(L"DEBUG",5),String(L"1",1));
	}else{
		if(t_==String(L"release",7)){
			bb_config_SetCfgVar(String(L"RELEASE",7),String(L"1",1));
		}else{
			if(t_==String(L"profile",7)){
				bb_config_SetCfgVar(String(L"PROFILE",7),String(L"1",1));
			}
		}
	}
	String t_main=LoadString(String(L"main.cpp",8));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"main.cpp",8));
	if(bb_target_OPT_ACTION>=5){
		String t_out=String(L"main_",5)+HostOS();
		DeleteFile(t_out);
		String t_OPTS=String();
		String t_LIBS=String();
		String t_2=bb_config_ENV_HOST;
		if(t_2==String(L"macos",5)){
			t_OPTS=t_OPTS+String(L" -arch i386 -read_only_relocs suppress -mmacosx-version-min=10.3",64);
		}else{
			if(t_2==String(L"winnt",5)){
				t_LIBS=t_LIBS+String(L" -lwinmm -lws2_32",17);
			}
		}
		String t_3=bb_config_ENV_CONFIG;
		if(t_3==String(L"release",7)){
			t_OPTS=t_OPTS+String(L" -O3 -DNDEBUG",13);
		}
		m_Execute(String(L"g++",3)+t_OPTS+String(L" -o ",4)+t_out+String(L" main.cpp",9)+t_LIBS,1);
		if(bb_target_OPT_ACTION>=6){
			m_Execute(String(L"\"",1)+RealPath(t_out)+String(L"\"",1),1);
		}
	}
	return 0;
}
void bb_stdcpp_StdcppTarget::mark(){
	bb_target_Target::mark();
}
bb_psm_PsmTarget::bb_psm_PsmTarget(){
}
int bb_psm_PsmTarget::g_IsValid(){
	if(FileType(String(L"psm",3))!=2){
		return 0;
	}
	String t_=HostOS();
	if(t_==String(L"winnt",5)){
		return ((((bb_target_PSM_PATH).Length()!=0) && FileType(bb_target_PSM_PATH+String(L"/tools/PsmStudio/bin/mdtool.exe",31))==1)?1:0);
	}
	return 0;
}
bb_psm_PsmTarget* bb_psm_PsmTarget::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_psm_PsmTarget::m_Begin(){
	bb_config_ENV_TARGET=String(L"psm",3);
	bb_config_ENV_LANG=String(L"cs",2);
	gc_assign(bb_translator__trans,((new bb_cstranslator_CsTranslator)->g_new()));
	return 0;
}
String bb_psm_PsmTarget::m_Content(){
	bb_stack_StringStack* t_cont=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=f_dataFiles->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		String t_p=t_kv->m_Key();
		String t_r=t_kv->m_Value();
		String t_f=bb_os_StripDir(t_r);
		String t_t=(String(L"data/",5)+t_r).Replace(String(L"/",1),String(L"\\",1));
		String t_ext=bb_os_ExtractExt(t_r).ToLower();
		if(bb_target_MatchPath(t_r,f_TEXT_FILES)){
			String t_2=t_ext;
			if(t_2==String(L"txt",3) || t_2==String(L"xml",3) || t_2==String(L"json",4)){
				t_cont->m_Push(String(L"    <Content Include=\"",22)+t_t+String(L"\">",2));
				t_cont->m_Push(String(L"      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>",67));
				t_cont->m_Push(String(L"    </Content>",14));
			}else{
				bb_target_Die(String(L"Invalid text file type",22));
			}
		}else{
			if(bb_target_MatchPath(t_r,f_IMAGE_FILES)){
				String t_3=t_ext;
				if(t_3==String(L"png",3) || t_3==String(L"jpg",3) || t_3==String(L"bmp",3) || t_3==String(L"gif",3)){
					t_cont->m_Push(String(L"    <Content Include=\"",22)+t_t+String(L"\">",2));
					t_cont->m_Push(String(L"      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>",67));
					t_cont->m_Push(String(L"    </Content>",14));
				}else{
					bb_target_Die(String(L"Invalid image file type",23));
				}
			}else{
				if(bb_target_MatchPath(t_r,f_SOUND_FILES)){
					String t_4=t_ext;
					if(t_4==String(L"wav",3)){
						t_cont->m_Push(String(L"    <Content Include=\"",22)+t_t+String(L"\">",2));
						t_cont->m_Push(String(L"      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>",67));
						t_cont->m_Push(String(L"    </Content>",14));
					}else{
						bb_target_Die(String(L"Invalid sound file type",23));
					}
				}else{
					if(bb_target_MatchPath(t_r,f_MUSIC_FILES)){
						String t_5=t_ext;
						if(t_5==String(L"mp3",3)){
							t_cont->m_Push(String(L"    <Content Include=\"",22)+t_t+String(L"\">",2));
							t_cont->m_Push(String(L"      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>",67));
							t_cont->m_Push(String(L"    </Content>",14));
						}else{
							bb_target_Die(String(L"Invalid music file type",23));
						}
					}else{
						if(bb_target_MatchPath(t_r,f_BINARY_FILES)){
							t_cont->m_Push(String(L"    <Content Include=\"",22)+t_t+String(L"\">",2));
							t_cont->m_Push(String(L"      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>",67));
							t_cont->m_Push(String(L"    </Content>",14));
						}
					}
				}
			}
		}
	}
	return t_cont->m_Join(String(L"\n",1));
}
String bb_psm_PsmTarget::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"public const String ",20)+t_kv->m_Key()+String(L"=",1)+bb_config_Enquote(t_kv->m_Value(),String(L"cs",2))+String(L";",1));
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_psm_PsmTarget::m_MakeTarget(){
	m_CreateDataDir(String(L"data",4));
	String t_proj=LoadString(String(L"MonkeyGame.csproj",17));
	t_proj=bb_target_ReplaceBlock(t_proj,String(L"CONTENT",7),m_Content(),String(L"\n<!-- ",6));
	SaveString(t_proj,String(L"MonkeyGame.csproj",17));
	String t_main=LoadString(String(L"MonkeyGame.cs",13));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"MonkeyGame.cs",13));
	if(bb_target_OPT_ACTION>=5){
		if(bb_target_OPT_ACTION>=6){
			m_Execute(String(L"\"",1)+bb_target_PSM_PATH+String(L"/tools/PsmStudio/bin/mdtool\" psm windows run-project MonkeyGame.sln",67),1);
		}
	}
	return 0;
}
void bb_psm_PsmTarget::mark(){
	bb_target_Target::mark();
}
bb_win8_Win8Target::bb_win8_Win8Target(){
}
int bb_win8_Win8Target::g_IsValid(){
	if(FileType(String(L"win8",4))!=2 || HostOS()!=String(L"winnt",5) || !((bb_target_MSBUILD_PATH).Length()!=0)){
		return 0;
	}
	return 1;
}
bb_win8_Win8Target* bb_win8_Win8Target::g_new(){
	bb_target_Target::g_new();
	return this;
}
int bb_win8_Win8Target::m_Begin(){
	bb_config_ENV_TARGET=String(L"win8",4);
	bb_config_ENV_LANG=String(L"cpp",3);
	gc_assign(bb_translator__trans,((new bb_cpptranslator_CppTranslator)->g_new()));
	return 0;
}
String bb_win8_Win8Target::m_Config(){
	bb_stack_StringStack* t_config=(new bb_stack_StringStack)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_config->m_Push(String(L"#define CFG_",12)+t_kv->m_Key()+String(L" ",1)+t_kv->m_Value());
	}
	return t_config->m_Join(String(L"\n",1));
}
int bb_win8_Win8Target::m_MakeTarget(){
	m_CreateDataDir(String(L"MonkeyGame/MonkeyGam/Assets/monkey",34));
	String t_main=LoadString(String(L"MonkeyGame/MonkeyGame/MonkeyGame.cpp",36));
	t_main=bb_target_ReplaceBlock(t_main,String(L"TRANSCODE",9),f_transCode,String(L"\n//",3));
	t_main=bb_target_ReplaceBlock(t_main,String(L"CONFIG",6),m_Config(),String(L"\n//",3));
	SaveString(t_main,String(L"MonkeyGame/MonkeyGame/MonkeyGame.cpp",36));
	if(bb_target_OPT_ACTION>=5){
		m_Execute(bb_target_MSBUILD_PATH+String(L" /t:MonkeyGame /p:Configuration=",32)+bb_target_CASED_CONFIG+String(L" /p:Platform=Win32 MonkeyGame\\MonkeyGame.sln",44),1);
	}
	return 0;
}
void bb_win8_Win8Target::mark(){
	bb_target_Target::mark();
}
String bb_targets_ValidTargets(){
	bb_stack_StringStack* t_valid=(new bb_stack_StringStack)->g_new();
	String t_cd=CurrentDir();
	ChangeDir(bb_os_ExtractDir(AppPath())+String(L"/../targets",11));
	if((bb_html5_Html5Target::g_IsValid())!=0){
		t_valid->m_Push(String(L"html5",5));
	}
	if((bb_flash_FlashTarget::g_IsValid())!=0){
		t_valid->m_Push(String(L"flash",5));
	}
	if((bb_android_AndroidTarget::g_IsValid())!=0){
		t_valid->m_Push(String(L"android",7));
	}
	if((bb_glfw_GlfwTarget::g_IsValid())!=0){
		t_valid->m_Push(String(L"glfw",4));
	}
	if((bb_xna_XnaTarget::g_IsValid())!=0){
		t_valid->m_Push(String(L"xna",3));
	}
	if((bb_ios_IosTarget::g_IsValid())!=0){
		t_valid->m_Push(String(L"ios",3));
	}
	if((bb_stdcpp_StdcppTarget::g_IsValid())!=0){
		t_valid->m_Push(String(L"stdcpp",6));
	}
	if((bb_psm_PsmTarget::g_IsValid())!=0){
		t_valid->m_Push(String(L"psm",3));
	}
	if((bb_win8_Win8Target::g_IsValid())!=0){
		t_valid->m_Push(String(L"win8",4));
	}
	ChangeDir(t_cd);
	return t_valid->m_Join(String(L" ",1));
}
String bb_config_ENV_HOST;
String bb_target_OPT_MODPATH;
int bb_config_ENV_SAFEMODE;
int bb_target_OPT_CLEAN;
int bb_target_OPT_ACTION;
String bb_target_OPT_OUTPUT;
String bb_target_CASED_CONFIG;
bb_target_Target* bb_targets_SelectTarget(String t_target){
	if(!(String(L" ",1)+bb_targets_ValidTargets()+String(L" ",1)).Contains(String(L" ",1)+t_target+String(L" ",1))){
		bb_target_Die(String(L"'",1)+t_target+String(L"' is not a valid target.",24));
	}
	String t_=t_target;
	if(t_==String(L"html5",5)){
		return ((new bb_html5_Html5Target)->g_new());
	}else{
		if(t_==String(L"flash",5)){
			return ((new bb_flash_FlashTarget)->g_new());
		}else{
			if(t_==String(L"android",7)){
				return ((new bb_android_AndroidTarget)->g_new());
			}else{
				if(t_==String(L"glfw",4)){
					return ((new bb_glfw_GlfwTarget)->g_new());
				}else{
					if(t_==String(L"xna",3)){
						return ((new bb_xna_XnaTarget)->g_new());
					}else{
						if(t_==String(L"ios",3)){
							return ((new bb_ios_IosTarget)->g_new());
						}else{
							if(t_==String(L"stdcpp",6)){
								return ((new bb_stdcpp_StdcppTarget)->g_new());
							}else{
								if(t_==String(L"win8",4)){
									return ((new bb_win8_Win8Target)->g_new());
								}else{
									if(t_==String(L"psm",3)){
										return ((new bb_psm_PsmTarget)->g_new());
									}else{
										bb_target_Die(String(L"Unknown target '",16)+t_target+String(L"'",1));
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}
String bb_config_ENV_CONFIG;
String bb_config_ENV_LANG;
String bb_config_ENV_TARGET;
bb_decl_Decl::bb_decl_Decl(){
	f_errInfo=String();
	f_ident=String();
	f_munged=String();
	f_attrs=0;
	f_scope=0;
}
bb_decl_Decl* bb_decl_Decl::g_new(){
	f_errInfo=bb_config__errInfo;
	return this;
}
int bb_decl_Decl::m_IsSemanted(){
	return (((f_attrs&1048576)!=0)?1:0);
}
int bb_decl_Decl::m_IsAbstract(){
	return (((f_attrs&1024)!=0)?1:0);
}
int bb_decl_Decl::m_IsExtern(){
	return (((f_attrs&256)!=0)?1:0);
}
String bb_decl_Decl::m_ToString(){
	if((dynamic_cast<bb_decl_ClassDecl*>(f_scope))!=0){
		return f_scope->m_ToString()+String(L".",1)+f_ident;
	}
	return f_ident;
}
int bb_decl_Decl::m_IsPrivate(){
	return (((f_attrs&512)!=0)?1:0);
}
bb_decl_ModuleDecl* bb_decl_Decl::m_ModuleScope(){
	if((dynamic_cast<bb_decl_ModuleDecl*>(this))!=0){
		return dynamic_cast<bb_decl_ModuleDecl*>(this);
	}
	if((f_scope)!=0){
		return f_scope->m_ModuleScope();
	}
	return 0;
}
bb_decl_FuncDecl* bb_decl_Decl::m_FuncScope(){
	if((dynamic_cast<bb_decl_FuncDecl*>(this))!=0){
		return dynamic_cast<bb_decl_FuncDecl*>(this);
	}
	if((f_scope)!=0){
		return f_scope->m_FuncScope();
	}
	return 0;
}
int bb_decl_Decl::m_CheckAccess(){
	if(((m_IsPrivate())!=0) && m_ModuleScope()!=bb_decl__env->m_ModuleScope()){
		bb_decl_FuncDecl* t_fdecl=bb_decl__env->m_FuncScope();
		if(((t_fdecl)!=0) && ((t_fdecl->f_attrs&8388608)!=0)){
			return 1;
		}
		return 0;
	}
	return 1;
}
int bb_decl_Decl::m_IsSemanting(){
	return (((f_attrs&2097152)!=0)?1:0);
}
bb_decl_AppDecl* bb_decl_Decl::m_AppScope(){
	if((dynamic_cast<bb_decl_AppDecl*>(this))!=0){
		return dynamic_cast<bb_decl_AppDecl*>(this);
	}
	if((f_scope)!=0){
		return f_scope->m_AppScope();
	}
	return 0;
}
int bb_decl_Decl::m_Semant(){
	if((m_IsSemanted())!=0){
		return 0;
	}
	if((m_IsSemanting())!=0){
		bb_config_Err(String(L"Cyclic declaration of '",23)+f_ident+String(L"'.",2));
	}
	bb_decl_ClassDecl* t_cscope=dynamic_cast<bb_decl_ClassDecl*>(f_scope);
	if((t_cscope)!=0){
		t_cscope->f_attrs&=-5;
	}
	bb_config_PushErr(f_errInfo);
	if((f_scope)!=0){
		bb_decl_PushEnv(f_scope);
	}
	f_attrs|=2097152;
	m_OnSemant();
	f_attrs&=-2097153;
	f_attrs|=1048576;
	if((f_scope)!=0){
		if((m_IsExtern())!=0){
			if((dynamic_cast<bb_decl_ModuleDecl*>(f_scope))!=0){
				m_AppScope()->f_allSemantedDecls->m_AddLast(this);
			}
		}else{
			f_scope->f_semanted->m_AddLast(this);
			if((dynamic_cast<bb_decl_GlobalDecl*>(this))!=0){
				m_AppScope()->f_semantedGlobals->m_AddLast6(dynamic_cast<bb_decl_GlobalDecl*>(this));
			}
			if((dynamic_cast<bb_decl_ModuleDecl*>(f_scope))!=0){
				m_AppScope()->f_semanted->m_AddLast(this);
				m_AppScope()->f_allSemantedDecls->m_AddLast(this);
			}
		}
		bb_decl_PopEnv();
	}
	bb_config_PopErr();
	return 0;
}
int bb_decl_Decl::m_AssertAccess(){
	if(!((m_CheckAccess())!=0)){
		bb_config_Err(m_ToString()+String(L" is private.",12));
	}
	return 0;
}
bb_decl_ClassDecl* bb_decl_Decl::m_ClassScope(){
	if((dynamic_cast<bb_decl_ClassDecl*>(this))!=0){
		return dynamic_cast<bb_decl_ClassDecl*>(this);
	}
	if((f_scope)!=0){
		return f_scope->m_ClassScope();
	}
	return 0;
}
bb_decl_Decl* bb_decl_Decl::m_Copy(){
	bb_decl_Decl* t_t=m_OnCopy();
	t_t->f_munged=f_munged;
	t_t->f_errInfo=f_errInfo;
	return t_t;
}
int bb_decl_Decl::m_IsFinal(){
	return (((f_attrs&2048)!=0)?1:0);
}
void bb_decl_Decl::mark(){
	Object::mark();
	gc_mark_q(f_scope);
}
bb_decl_ScopeDecl::bb_decl_ScopeDecl(){
	f_decls=(new bb_list_List)->g_new();
	f_declsMap=(new bb_map_StringMap2)->g_new();
	f_semanted=(new bb_list_List)->g_new();
}
bb_decl_ScopeDecl* bb_decl_ScopeDecl::g_new(){
	bb_decl_Decl::g_new();
	return this;
}
int bb_decl_ScopeDecl::m_InsertDecl(bb_decl_Decl* t_decl){
	if((t_decl->f_scope)!=0){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	String t_ident=t_decl->f_ident;
	if(!((t_ident).Length()!=0)){
		return 0;
	}
	gc_assign(t_decl->f_scope,this);
	f_decls->m_AddLast(t_decl);
	bb_map_StringMap2* t_decls=0;
	Object* t_tdecl=f_declsMap->m_Get(t_ident);
	if((dynamic_cast<bb_decl_FuncDecl*>(t_decl))!=0){
		bb_decl_FuncDeclList* t_funcs=dynamic_cast<bb_decl_FuncDeclList*>(t_tdecl);
		if(((t_funcs)!=0) || !((t_tdecl)!=0)){
			if(!((t_funcs)!=0)){
				t_funcs=(new bb_decl_FuncDeclList)->g_new();
				f_declsMap->m_Insert2(t_ident,(t_funcs));
			}
			t_funcs->m_AddLast2(dynamic_cast<bb_decl_FuncDecl*>(t_decl));
		}else{
			bb_config_Err(String(L"Duplicate identifier '",22)+t_ident+String(L"'.",2));
		}
	}else{
		if(!((t_tdecl)!=0)){
			f_declsMap->m_Insert2(t_ident,(t_decl));
		}else{
			bb_config_Err(String(L"Duplicate identifier '",22)+t_ident+String(L"'.",2));
		}
	}
	if((t_decl->m_IsSemanted())!=0){
		f_semanted->m_AddLast(t_decl);
	}
	return 0;
}
Object* bb_decl_ScopeDecl::m_GetDecl(String t_ident){
	Object* t_decl=f_declsMap->m_Get(t_ident);
	if(!((t_decl)!=0)){
		return 0;
	}
	bb_decl_AliasDecl* t_adecl=dynamic_cast<bb_decl_AliasDecl*>(t_decl);
	if(!((t_adecl)!=0)){
		return t_decl;
	}
	if((t_adecl->m_CheckAccess())!=0){
		return t_adecl->f_decl;
	}
	return 0;
}
Object* bb_decl_ScopeDecl::m_FindDecl(String t_ident){
	if(bb_decl__env!=this){
		return m_GetDecl(t_ident);
	}
	bb_decl_ScopeDecl* t_tscope=this;
	while((t_tscope)!=0){
		Object* t_decl=t_tscope->m_GetDecl(t_ident);
		if((t_decl)!=0){
			return t_decl;
		}
		t_tscope=t_tscope->f_scope;
	}
	return 0;
}
bb_decl_FuncDecl* bb_decl_ScopeDecl::m_FindFuncDecl(String t_ident,Array<bb_expr_Expr* > t_argExprs,int t_explicit){
	bb_decl_FuncDeclList* t_funcs=dynamic_cast<bb_decl_FuncDeclList*>(m_FindDecl(t_ident));
	if(!((t_funcs)!=0)){
		return 0;
	}
	bb_list_Enumerator* t_=t_funcs->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_FuncDecl* t_func=t_->m_NextObject();
		t_func->m_Semant();
	}
	bb_decl_FuncDecl* t_match=0;
	int t_isexact=0;
	String t_err=String();
	bb_list_Enumerator* t_2=t_funcs->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_FuncDecl* t_func2=t_2->m_NextObject();
		if(!((t_func2->m_CheckAccess())!=0)){
			continue;
		}
		Array<bb_decl_ArgDecl* > t_argDecls=t_func2->f_argDecls;
		if(t_argExprs.Length()>t_argDecls.Length()){
			continue;
		}
		int t_exact=1;
		int t_possible=1;
		for(int t_i=0;t_i<t_argDecls.Length();t_i=t_i+1){
			if(t_i<t_argExprs.Length() && ((t_argExprs[t_i])!=0)){
				bb_type_Type* t_declTy=t_argDecls[t_i]->f_type;
				bb_type_Type* t_exprTy=t_argExprs[t_i]->f_exprType;
				if((t_exprTy->m_EqualsType(t_declTy))!=0){
					continue;
				}
				t_exact=0;
				if(!((t_explicit)!=0) && ((t_exprTy->m_ExtendsType(t_declTy))!=0)){
					continue;
				}
			}else{
				if((t_argDecls[t_i]->f_init)!=0){
					t_exact=0;
					if(!((t_explicit)!=0)){
						continue;
					}
				}
			}
			t_possible=0;
			break;
		}
		if(!((t_possible)!=0)){
			continue;
		}
		if((t_exact)!=0){
			if((t_isexact)!=0){
				bb_config_Err(String(L"Unable to determine overload to use: ",37)+t_match->m_ToString()+String(L" or ",4)+t_func2->m_ToString()+String(L".",1));
			}else{
				t_err=String();
				t_match=t_func2;
				t_isexact=1;
			}
		}else{
			if(!((t_isexact)!=0)){
				if((t_match)!=0){
					t_err=String(L"Unable to determine overload to use: ",37)+t_match->m_ToString()+String(L" or ",4)+t_func2->m_ToString()+String(L".",1);
				}else{
					t_match=t_func2;
				}
			}
		}
	}
	if(!((t_isexact)!=0)){
		if((t_err).Length()!=0){
			bb_config_Err(t_err);
		}
		if((t_explicit)!=0){
			return 0;
		}
	}
	if(!((t_match)!=0)){
		String t_t=String();
		for(int t_i2=0;t_i2<t_argExprs.Length();t_i2=t_i2+1){
			if((t_t).Length()!=0){
				t_t=t_t+String(L",",1);
			}
			if((t_argExprs[t_i2])!=0){
				t_t=t_t+t_argExprs[t_i2]->f_exprType->m_ToString();
			}
		}
		bb_config_Err(String(L"Unable to find overload for ",28)+t_ident+String(L"(",1)+t_t+String(L").",2));
	}
	t_match->m_AssertAccess();
	return t_match;
}
int bb_decl_ScopeDecl::m_InsertDecls(bb_list_List* t_decls){
	bb_list_Enumerator2* t_=t_decls->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		m_InsertDecl(t_decl);
	}
	return 0;
}
bb_list_List* bb_decl_ScopeDecl::m_Decls(){
	return f_decls;
}
bb_type_Type* bb_decl_ScopeDecl::m_FindType(String t_ident,Array<bb_type_Type* > t_args){
	Object* t_decl=m_GetDecl(t_ident);
	if((t_decl)!=0){
		bb_type_Type* t_type=dynamic_cast<bb_type_Type*>(t_decl);
		if((t_type)!=0){
			if((t_args.Length())!=0){
				bb_config_Err(String(L"Wrong number of type arguments",30));
			}
			return t_type;
		}
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl);
		if((t_cdecl)!=0){
			t_cdecl->m_AssertAccess();
			t_cdecl=t_cdecl->m_GenClassInstance(t_args);
			t_cdecl->m_Semant();
			return (t_cdecl->f_objectType);
		}
	}
	if((f_scope)!=0){
		return f_scope->m_FindType(t_ident,t_args);
	}
	return 0;
}
bb_list_List2* bb_decl_ScopeDecl::m_MethodDecls(String t_id){
	bb_list_List2* t_fdecls=(new bb_list_List2)->g_new();
	bb_list_Enumerator2* t_=f_decls->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		if(((t_id).Length()!=0) && t_decl->f_ident!=t_id){
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
		if(((t_fdecl)!=0) && t_fdecl->m_IsMethod()){
			t_fdecls->m_AddLast2(t_fdecl);
		}
	}
	return t_fdecls;
}
bb_list_List* bb_decl_ScopeDecl::m_Semanted(){
	return f_semanted;
}
bb_list_List2* bb_decl_ScopeDecl::m_SemantedMethods(String t_id){
	bb_list_List2* t_fdecls=(new bb_list_List2)->g_new();
	bb_list_Enumerator2* t_=f_semanted->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		if(((t_id).Length()!=0) && t_decl->f_ident!=t_id){
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
		if(((t_fdecl)!=0) && t_fdecl->m_IsMethod()){
			t_fdecls->m_AddLast2(t_fdecl);
		}
	}
	return t_fdecls;
}
bb_decl_ValDecl* bb_decl_ScopeDecl::m_FindValDecl(String t_ident){
	bb_decl_ValDecl* t_decl=dynamic_cast<bb_decl_ValDecl*>(m_FindDecl(t_ident));
	if(!((t_decl)!=0)){
		return 0;
	}
	t_decl->m_AssertAccess();
	t_decl->m_Semant();
	return t_decl;
}
bb_decl_Decl* bb_decl_ScopeDecl::m_OnCopy(){
	bb_config_InternalErr(String(L"Internal error",14));
	return 0;
}
int bb_decl_ScopeDecl::m_OnSemant(){
	return 0;
}
bb_list_List2* bb_decl_ScopeDecl::m_SemantedFuncs(String t_id){
	bb_list_List2* t_fdecls=(new bb_list_List2)->g_new();
	bb_list_Enumerator2* t_=f_semanted->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		if(((t_id).Length()!=0) && t_decl->f_ident!=t_id){
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
		if((t_fdecl)!=0){
			t_fdecls->m_AddLast2(t_fdecl);
		}
	}
	return t_fdecls;
}
bb_decl_ModuleDecl* bb_decl_ScopeDecl::m_FindModuleDecl(String t_ident){
	bb_decl_ModuleDecl* t_decl=dynamic_cast<bb_decl_ModuleDecl*>(m_GetDecl(t_ident));
	if((t_decl)!=0){
		t_decl->m_AssertAccess();
		t_decl->m_Semant();
		return t_decl;
	}
	if((f_scope)!=0){
		return f_scope->m_FindModuleDecl(t_ident);
	}
	return 0;
}
bb_decl_ScopeDecl* bb_decl_ScopeDecl::m_FindScopeDecl(String t_ident){
	Object* t_decl=m_FindDecl(t_ident);
	bb_type_Type* t_type=dynamic_cast<bb_type_Type*>(t_decl);
	if((t_type)!=0){
		if(!((dynamic_cast<bb_type_ObjectType*>(t_type))!=0)){
			return 0;
		}
		return (t_type->m_GetClass());
	}
	bb_decl_ScopeDecl* t_scope=dynamic_cast<bb_decl_ScopeDecl*>(t_decl);
	if((t_scope)!=0){
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_scope);
		if(((t_cdecl)!=0) && ((t_cdecl->f_args).Length()!=0)){
			return 0;
		}
		t_scope->m_AssertAccess();
		t_scope->m_Semant();
		return t_scope;
	}
	return 0;
}
bb_list_List2* bb_decl_ScopeDecl::m_FuncDecls(String t_id){
	bb_list_List2* t_fdecls=(new bb_list_List2)->g_new();
	bb_list_Enumerator2* t_=f_decls->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		if(((t_id).Length()!=0) && t_decl->f_ident!=t_id){
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
		if((t_fdecl)!=0){
			t_fdecls->m_AddLast2(t_fdecl);
		}
	}
	return t_fdecls;
}
void bb_decl_ScopeDecl::mark(){
	bb_decl_Decl::mark();
	gc_mark_q(f_decls);
	gc_mark_q(f_declsMap);
	gc_mark_q(f_semanted);
}
bb_decl_AppDecl::bb_decl_AppDecl(){
	f_allSemantedDecls=(new bb_list_List)->g_new();
	f_semantedGlobals=(new bb_list_List6)->g_new();
	f_imported=(new bb_map_StringMap3)->g_new();
	f_mainModule=0;
	f_fileImports=(new bb_list_StringList)->g_new();
	f_semantedClasses=(new bb_list_List7)->g_new();
	f_mainFunc=0;
}
bb_decl_AppDecl* bb_decl_AppDecl::g_new(){
	bb_decl_ScopeDecl::g_new();
	return this;
}
int bb_decl_AppDecl::m_InsertModule(bb_decl_ModuleDecl* t_mdecl){
	gc_assign(t_mdecl->f_scope,(this));
	f_imported->m_Insert3(t_mdecl->f_filepath,t_mdecl);
	if(!((f_mainModule)!=0)){
		gc_assign(f_mainModule,t_mdecl);
	}
	return 0;
}
int bb_decl_AppDecl::m_FinalizeClasses(){
	bb_decl__env=0;
	do{
		int t_more=0;
		bb_list_Enumerator3* t_=f_semantedClasses->m_ObjectEnumerator();
		while(t_->m_HasNext()){
			bb_decl_ClassDecl* t_cdecl=t_->m_NextObject();
			t_more+=t_cdecl->m_UpdateLiveMethods();
		}
		if(!((t_more)!=0)){
			break;
		}
	}while(!(false));
	bb_list_Enumerator3* t_2=f_semantedClasses->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_ClassDecl* t_cdecl2=t_2->m_NextObject();
		t_cdecl2->m_FinalizeClass();
	}
	return 0;
}
int bb_decl_AppDecl::m_OnSemant(){
	bb_decl__env=0;
	gc_assign(f_mainFunc,f_mainModule->m_FindFuncDecl(String(L"Main",4),Array<bb_expr_Expr* >(),0));
	if(!((f_mainFunc)!=0)){
		bb_config_Err(String(L"Function 'Main' not found.",26));
	}
	if(!((dynamic_cast<bb_type_IntType*>(f_mainFunc->f_retType))!=0) || ((f_mainFunc->f_argDecls.Length())!=0)){
		bb_config_Err(String(L"Main function must be of type Main:Int()",40));
	}
	m_FinalizeClasses();
	return 0;
}
void bb_decl_AppDecl::mark(){
	bb_decl_ScopeDecl::mark();
	gc_mark_q(f_allSemantedDecls);
	gc_mark_q(f_semantedGlobals);
	gc_mark_q(f_imported);
	gc_mark_q(f_mainModule);
	gc_mark_q(f_fileImports);
	gc_mark_q(f_semantedClasses);
	gc_mark_q(f_mainFunc);
}
String bb_config__errInfo;
bb_toker_Toker::bb_toker_Toker(){
	f__path=String();
	f__line=0;
	f__source=String();
	f__length=0;
	f__toke=String();
	f__tokeType=0;
	f__tokePos=0;
}
bb_set_StringSet* bb_toker_Toker::g__keywords;
bb_set_StringSet* bb_toker_Toker::g__symbols;
int bb_toker_Toker::m__init(){
	if((bb_toker_Toker::g__keywords)!=0){
		return 0;
	}
	gc_assign(bb_toker_Toker::g__keywords,(new bb_set_StringSet)->g_new());
	Array<String > t_=String(L"void strict public private property bool int float string array object mod continue exit include import module extern new self super eachin true false null not extends abstract final select case default const local global field method function class and or shl shr end if then else elseif endif while wend repeat until forever for to step next return interface implements inline alias try catch throw throwable",410).Split(String(L" ",1));
	int t_2=0;
	while(t_2<t_.Length()){
		String t_t=t_[t_2];
		t_2=t_2+1;
		bb_toker_Toker::g__keywords->m_Insert(t_t);
	}
	gc_assign(bb_toker_Toker::g__symbols,(new bb_set_StringSet)->g_new());
	bb_toker_Toker::g__symbols->m_Insert(String(L"..",2));
	bb_toker_Toker::g__symbols->m_Insert(String(L":=",2));
	bb_toker_Toker::g__symbols->m_Insert(String(L"*=",2));
	bb_toker_Toker::g__symbols->m_Insert(String(L"/=",2));
	bb_toker_Toker::g__symbols->m_Insert(String(L"+=",2));
	bb_toker_Toker::g__symbols->m_Insert(String(L"-=",2));
	bb_toker_Toker::g__symbols->m_Insert(String(L"|=",2));
	bb_toker_Toker::g__symbols->m_Insert(String(L"&=",2));
	bb_toker_Toker::g__symbols->m_Insert(String(L"~=",2));
	return 0;
}
bb_toker_Toker* bb_toker_Toker::g_new(String t_path,String t_source){
	m__init();
	f__path=t_path;
	f__line=1;
	f__source=t_source;
	f__length=f__source.Length();
	f__toke=String();
	f__tokeType=0;
	f__tokePos=0;
	return this;
}
bb_toker_Toker* bb_toker_Toker::g_new2(bb_toker_Toker* t_toker){
	m__init();
	f__path=t_toker->f__path;
	f__line=t_toker->f__line;
	f__source=t_toker->f__source;
	f__length=f__source.Length();
	f__toke=t_toker->f__toke;
	f__tokeType=t_toker->f__tokeType;
	f__tokePos=t_toker->f__tokePos;
	return this;
}
bb_toker_Toker* bb_toker_Toker::g_new3(){
	return this;
}
int bb_toker_Toker::m_TCHR(int t_i){
	t_i+=f__tokePos;
	if(t_i<f__length){
		return (int)f__source[t_i];
	}
	return 0;
}
String bb_toker_Toker::m_TSTR(int t_i){
	t_i+=f__tokePos;
	if(t_i<f__length){
		return f__source.Slice(t_i,t_i+1);
	}
	return String();
}
String bb_toker_Toker::m_NextToke(){
	f__toke=String();
	if(f__tokePos==f__length){
		f__tokeType=0;
		return f__toke;
	}
	int t_chr=m_TCHR(0);
	String t_str=m_TSTR(0);
	int t_start=f__tokePos;
	f__tokePos+=1;
	if(t_str==String(L"\n",1)){
		f__tokeType=8;
		f__line+=1;
	}else{
		if((bb_config_IsSpace(t_chr))!=0){
			f__tokeType=1;
			while(f__tokePos<f__length && ((bb_config_IsSpace(m_TCHR(0)))!=0) && m_TSTR(0)!=String(L"\n",1)){
				f__tokePos+=1;
			}
		}else{
			if(t_str==String(L"_",1) || ((bb_config_IsAlpha(t_chr))!=0)){
				f__tokeType=2;
				while(f__tokePos<f__length){
					int t_chr2=(int)f__source[f__tokePos];
					if(t_chr2!=95 && !((bb_config_IsAlpha(t_chr2))!=0) && !((bb_config_IsDigit(t_chr2))!=0)){
						break;
					}
					f__tokePos+=1;
				}
				f__toke=f__source.Slice(t_start,f__tokePos);
				if(bb_toker_Toker::g__keywords->m_Contains(f__toke.ToLower())){
					f__tokeType=3;
				}
			}else{
				if(((bb_config_IsDigit(t_chr))!=0) || t_str==String(L".",1) && ((bb_config_IsDigit(m_TCHR(0)))!=0)){
					f__tokeType=4;
					if(t_str==String(L".",1)){
						f__tokeType=5;
					}
					while((bb_config_IsDigit(m_TCHR(0)))!=0){
						f__tokePos+=1;
					}
					if(f__tokeType==4 && m_TSTR(0)==String(L".",1) && ((bb_config_IsDigit(m_TCHR(1)))!=0)){
						f__tokeType=5;
						f__tokePos+=2;
						while((bb_config_IsDigit(m_TCHR(0)))!=0){
							f__tokePos+=1;
						}
					}
					if(m_TSTR(0).ToLower()==String(L"e",1)){
						f__tokeType=5;
						f__tokePos+=1;
						if(m_TSTR(0)==String(L"+",1) || m_TSTR(0)==String(L"-",1)){
							f__tokePos+=1;
						}
						while((bb_config_IsDigit(m_TCHR(0)))!=0){
							f__tokePos+=1;
						}
					}
				}else{
					if(t_str==String(L"%",1) && ((bb_config_IsBinDigit(m_TCHR(0)))!=0)){
						f__tokeType=4;
						f__tokePos+=1;
						while((bb_config_IsBinDigit(m_TCHR(0)))!=0){
							f__tokePos+=1;
						}
					}else{
						if(t_str==String(L"$",1) && ((bb_config_IsHexDigit(m_TCHR(0)))!=0)){
							f__tokeType=4;
							f__tokePos+=1;
							while((bb_config_IsHexDigit(m_TCHR(0)))!=0){
								f__tokePos+=1;
							}
						}else{
							if(t_str==String(L"\"",1)){
								f__tokeType=6;
								while(f__tokePos<f__length && m_TSTR(0)!=String(L"\"",1)){
									f__tokePos+=1;
								}
								if(f__tokePos<f__length){
									f__tokePos+=1;
								}else{
									f__tokeType=7;
								}
							}else{
								if(t_str==String(L"'",1)){
									f__tokeType=9;
									while(f__tokePos<f__length && m_TSTR(0)!=String(L"\n",1)){
										f__tokePos+=1;
									}
									if(f__tokePos<f__length){
										f__tokePos+=1;
										f__line+=1;
									}
								}else{
									if(t_str==String(L"[",1)){
										f__tokeType=8;
										int t_i=0;
										while(f__tokePos+t_i<f__length){
											if(m_TSTR(t_i)==String(L"]",1)){
												f__tokePos+=t_i+1;
												break;
											}
											if(m_TSTR(t_i)==String(L"\n",1) || !((bb_config_IsSpace(m_TCHR(t_i)))!=0)){
												break;
											}
											t_i+=1;
										}
									}else{
										f__tokeType=8;
										if(bb_toker_Toker::g__symbols->m_Contains(f__source.Slice(f__tokePos-1,f__tokePos+1))){
											f__tokePos+=1;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if(!((f__toke).Length()!=0)){
		f__toke=f__source.Slice(t_start,f__tokePos);
	}
	return f__toke;
}
String bb_toker_Toker::m_Toke(){
	return f__toke;
}
int bb_toker_Toker::m_TokeType(){
	return f__tokeType;
}
String bb_toker_Toker::m_Path(){
	return f__path;
}
int bb_toker_Toker::m_Line(){
	return f__line;
}
int bb_toker_Toker::m_SkipSpace(){
	while(f__tokeType==1){
		m_NextToke();
	}
	return 0;
}
void bb_toker_Toker::mark(){
	Object::mark();
}
bb_set_Set::bb_set_Set(){
	f_map=0;
}
bb_set_Set* bb_set_Set::g_new(bb_map_Map2* t_map){
	gc_assign(this->f_map,t_map);
	return this;
}
bb_set_Set* bb_set_Set::g_new2(){
	return this;
}
int bb_set_Set::m_Insert(String t_value){
	f_map->m_Insert2(t_value,0);
	return 0;
}
bool bb_set_Set::m_Contains(String t_value){
	return f_map->m_Contains(t_value);
}
void bb_set_Set::mark(){
	Object::mark();
	gc_mark_q(f_map);
}
bb_set_StringSet::bb_set_StringSet(){
}
bb_set_StringSet* bb_set_StringSet::g_new(){
	bb_set_Set::g_new((new bb_map_StringMap2)->g_new());
	return this;
}
void bb_set_StringSet::mark(){
	bb_set_Set::mark();
}
bb_map_Map2::bb_map_Map2(){
	f_root=0;
}
bb_map_Map2* bb_map_Map2::g_new(){
	return this;
}
int bb_map_Map2::m_RotateLeft2(bb_map_Node2* t_node){
	bb_map_Node2* t_child=t_node->f_right;
	gc_assign(t_node->f_right,t_child->f_left);
	if((t_child->f_left)!=0){
		gc_assign(t_child->f_left->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_left){
			gc_assign(t_node->f_parent->f_left,t_child);
		}else{
			gc_assign(t_node->f_parent->f_right,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_left,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map2::m_RotateRight2(bb_map_Node2* t_node){
	bb_map_Node2* t_child=t_node->f_left;
	gc_assign(t_node->f_left,t_child->f_right);
	if((t_child->f_right)!=0){
		gc_assign(t_child->f_right->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_right){
			gc_assign(t_node->f_parent->f_right,t_child);
		}else{
			gc_assign(t_node->f_parent->f_left,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_right,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map2::m_InsertFixup2(bb_map_Node2* t_node){
	while(((t_node->f_parent)!=0) && t_node->f_parent->f_color==-1 && ((t_node->f_parent->f_parent)!=0)){
		if(t_node->f_parent==t_node->f_parent->f_parent->f_left){
			bb_map_Node2* t_uncle=t_node->f_parent->f_parent->f_right;
			if(((t_uncle)!=0) && t_uncle->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle->f_color=1;
				t_uncle->f_parent->f_color=-1;
				t_node=t_uncle->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_right){
					t_node=t_node->f_parent;
					m_RotateLeft2(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateRight2(t_node->f_parent->f_parent);
			}
		}else{
			bb_map_Node2* t_uncle2=t_node->f_parent->f_parent->f_left;
			if(((t_uncle2)!=0) && t_uncle2->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle2->f_color=1;
				t_uncle2->f_parent->f_color=-1;
				t_node=t_uncle2->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_left){
					t_node=t_node->f_parent;
					m_RotateRight2(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateLeft2(t_node->f_parent->f_parent);
			}
		}
	}
	f_root->f_color=1;
	return 0;
}
bool bb_map_Map2::m_Set2(String t_key,Object* t_value){
	bb_map_Node2* t_node=f_root;
	bb_map_Node2* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				gc_assign(t_node->f_value,t_value);
				return false;
			}
		}
	}
	t_node=(new bb_map_Node2)->g_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->f_right,t_node);
		}else{
			gc_assign(t_parent->f_left,t_node);
		}
		m_InsertFixup2(t_node);
	}else{
		gc_assign(f_root,t_node);
	}
	return true;
}
bool bb_map_Map2::m_Insert2(String t_key,Object* t_value){
	return m_Set2(t_key,t_value);
}
bb_map_Node2* bb_map_Map2::m_FindNode(String t_key){
	bb_map_Node2* t_node=f_root;
	while((t_node)!=0){
		int t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool bb_map_Map2::m_Contains(String t_key){
	return m_FindNode(t_key)!=0;
}
Object* bb_map_Map2::m_Get(String t_key){
	bb_map_Node2* t_node=m_FindNode(t_key);
	if((t_node)!=0){
		return t_node->f_value;
	}
	return 0;
}
void bb_map_Map2::mark(){
	Object::mark();
	gc_mark_q(f_root);
}
bb_map_StringMap2::bb_map_StringMap2(){
}
bb_map_StringMap2* bb_map_StringMap2::g_new(){
	bb_map_Map2::g_new();
	return this;
}
int bb_map_StringMap2::m_Compare(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void bb_map_StringMap2::mark(){
	bb_map_Map2::mark();
}
bb_map_Node2::bb_map_Node2(){
	f_key=String();
	f_right=0;
	f_left=0;
	f_value=0;
	f_color=0;
	f_parent=0;
}
bb_map_Node2* bb_map_Node2::g_new(String t_key,Object* t_value,int t_color,bb_map_Node2* t_parent){
	this->f_key=t_key;
	gc_assign(this->f_value,t_value);
	this->f_color=t_color;
	gc_assign(this->f_parent,t_parent);
	return this;
}
bb_map_Node2* bb_map_Node2::g_new2(){
	return this;
}
void bb_map_Node2::mark(){
	Object::mark();
	gc_mark_q(f_right);
	gc_mark_q(f_left);
	gc_mark_q(f_value);
	gc_mark_q(f_parent);
}
int bb_config_IsSpace(int t_ch){
	return ((t_ch<=32)?1:0);
}
int bb_config_IsAlpha(int t_ch){
	return ((t_ch>=65 && t_ch<=90 || t_ch>=97 && t_ch<=122)?1:0);
}
int bb_config_IsDigit(int t_ch){
	return ((t_ch>=48 && t_ch<=57)?1:0);
}
int bb_config_IsBinDigit(int t_ch){
	return ((t_ch==48 || t_ch==49)?1:0);
}
int bb_config_IsHexDigit(int t_ch){
	return ((t_ch>=48 && t_ch<=57 || t_ch>=65 && t_ch<=70 || t_ch>=97 && t_ch<=102)?1:0);
}
bb_type_Type::bb_type_Type(){
	f_arrayOf=0;
}
bb_type_Type* bb_type_Type::g_new(){
	return this;
}
bb_type_BoolType* bb_type_Type::g_boolType;
bb_type_StringType* bb_type_Type::g_stringType;
bb_type_VoidType* bb_type_Type::g_voidType;
bb_type_ArrayType* bb_type_Type::g_emptyArrayType;
bb_type_IntType* bb_type_Type::g_intType;
bb_type_FloatType* bb_type_Type::g_floatType;
bb_type_IdentType* bb_type_Type::g_objectType;
bb_type_IdentType* bb_type_Type::g_throwableType;
bb_type_ArrayType* bb_type_Type::m_ArrayOf(){
	if(!((f_arrayOf)!=0)){
		gc_assign(f_arrayOf,(new bb_type_ArrayType)->g_new(this));
	}
	return f_arrayOf;
}
bb_type_IdentType* bb_type_Type::g_nullObjectType;
int bb_type_Type::m_EqualsType(bb_type_Type* t_ty){
	return 0;
}
bb_type_Type* bb_type_Type::m_Semant(){
	return this;
}
int bb_type_Type::m_ExtendsType(bb_type_Type* t_ty){
	return m_EqualsType(t_ty);
}
String bb_type_Type::m_ToString(){
	return String(L"??Type??",8);
}
bb_decl_ClassDecl* bb_type_Type::m_GetClass(){
	return 0;
}
void bb_type_Type::mark(){
	Object::mark();
	gc_mark_q(f_arrayOf);
}
bb_type_BoolType::bb_type_BoolType(){
}
bb_type_BoolType* bb_type_BoolType::g_new(){
	bb_type_Type::g_new();
	return this;
}
int bb_type_BoolType::m_EqualsType(bb_type_Type* t_ty){
	return ((dynamic_cast<bb_type_BoolType*>(t_ty)!=0)?1:0);
}
int bb_type_BoolType::m_ExtendsType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		bb_expr_Expr* t_expr=((new bb_expr_ConstExpr)->g_new((this),String()))->m_Semant();
		bb_expr_Expr* t_[]={t_expr};
		bb_decl_FuncDecl* t_ctor=t_ty->m_GetClass()->m_FindFuncDecl(String(L"new",3),Array<bb_expr_Expr* >(t_,1),1);
		return ((((t_ctor)!=0) && t_ctor->m_IsCtor())?1:0);
	}
	return ((dynamic_cast<bb_type_IntType*>(t_ty)!=0 || dynamic_cast<bb_type_BoolType*>(t_ty)!=0)?1:0);
}
bb_decl_ClassDecl* bb_type_BoolType::m_GetClass(){
	return dynamic_cast<bb_decl_ClassDecl*>(bb_decl__env->m_FindDecl(String(L"bool",4)));
}
String bb_type_BoolType::m_ToString(){
	return String(L"Bool",4);
}
void bb_type_BoolType::mark(){
	bb_type_Type::mark();
}
bb_map_NodeEnumerator::bb_map_NodeEnumerator(){
	f_node=0;
}
bb_map_NodeEnumerator* bb_map_NodeEnumerator::g_new(bb_map_Node* t_node){
	gc_assign(this->f_node,t_node);
	return this;
}
bb_map_NodeEnumerator* bb_map_NodeEnumerator::g_new2(){
	return this;
}
bool bb_map_NodeEnumerator::m_HasNext(){
	return f_node!=0;
}
bb_map_Node* bb_map_NodeEnumerator::m_NextObject(){
	bb_map_Node* t_t=f_node;
	gc_assign(f_node,f_node->m_NextNode());
	return t_t;
}
void bb_map_NodeEnumerator::mark(){
	Object::mark();
	gc_mark_q(f_node);
}
bb_decl_ValDecl::bb_decl_ValDecl(){
	f_type=0;
	f_init=0;
}
bb_decl_ValDecl* bb_decl_ValDecl::g_new(){
	bb_decl_Decl::g_new();
	return this;
}
String bb_decl_ValDecl::m_ToString(){
	String t_t=bb_decl_Decl::m_ToString();
	return t_t+String(L":",1)+f_type->m_ToString();
}
int bb_decl_ValDecl::m_OnSemant(){
	if((f_type)!=0){
		gc_assign(f_type,f_type->m_Semant());
		if((f_init)!=0){
			gc_assign(f_init,f_init->m_Semant2(f_type,0));
		}
	}else{
		if((f_init)!=0){
			gc_assign(f_init,f_init->m_Semant());
			gc_assign(f_type,f_init->f_exprType);
		}else{
			bb_config_InternalErr(String(L"Internal error",14));
		}
	}
	return 0;
}
bb_expr_Expr* bb_decl_ValDecl::m_CopyInit(){
	if((f_init)!=0){
		return f_init->m_Copy();
	}
	return 0;
}
void bb_decl_ValDecl::mark(){
	bb_decl_Decl::mark();
	gc_mark_q(f_type);
	gc_mark_q(f_init);
}
bb_decl_ConstDecl::bb_decl_ConstDecl(){
	f_value=String();
}
bb_decl_ConstDecl* bb_decl_ConstDecl::g_new(String t_ident,int t_attrs,bb_type_Type* t_type,bb_expr_Expr* t_init){
	bb_decl_ValDecl::g_new();
	this->f_ident=t_ident;
	this->f_munged=t_ident;
	this->f_attrs=t_attrs;
	gc_assign(this->f_type,t_type);
	gc_assign(this->f_init,t_init);
	return this;
}
bb_decl_ConstDecl* bb_decl_ConstDecl::g_new2(){
	bb_decl_ValDecl::g_new();
	return this;
}
bb_decl_Decl* bb_decl_ConstDecl::m_OnCopy(){
	return ((new bb_decl_ConstDecl)->g_new(f_ident,f_attrs,f_type,m_CopyInit()));
}
int bb_decl_ConstDecl::m_OnSemant(){
	bb_decl_ValDecl::m_OnSemant();
	if(!((m_IsExtern())!=0)){
		f_value=f_init->m_Eval();
	}
	return 0;
}
void bb_decl_ConstDecl::mark(){
	bb_decl_ValDecl::mark();
}
bb_type_StringType::bb_type_StringType(){
}
bb_type_StringType* bb_type_StringType::g_new(){
	bb_type_Type::g_new();
	return this;
}
int bb_type_StringType::m_EqualsType(bb_type_Type* t_ty){
	return ((dynamic_cast<bb_type_StringType*>(t_ty)!=0)?1:0);
}
int bb_type_StringType::m_ExtendsType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		bb_expr_Expr* t_expr=((new bb_expr_ConstExpr)->g_new((this),String()))->m_Semant();
		bb_expr_Expr* t_[]={t_expr};
		bb_decl_FuncDecl* t_ctor=t_ty->m_GetClass()->m_FindFuncDecl(String(L"new",3),Array<bb_expr_Expr* >(t_,1),1);
		return ((((t_ctor)!=0) && t_ctor->m_IsCtor())?1:0);
	}
	return m_EqualsType(t_ty);
}
bb_decl_ClassDecl* bb_type_StringType::m_GetClass(){
	return dynamic_cast<bb_decl_ClassDecl*>(bb_decl__env->m_FindDecl(String(L"string",6)));
}
String bb_type_StringType::m_ToString(){
	return String(L"String",6);
}
void bb_type_StringType::mark(){
	bb_type_Type::mark();
}
bb_expr_Expr::bb_expr_Expr(){
	f_exprType=0;
}
bb_expr_Expr* bb_expr_Expr::g_new(){
	return this;
}
bb_expr_Expr* bb_expr_Expr::m_Semant(){
	bb_config_InternalErr(String(L"Internal error",14));
	return 0;
}
Array<bb_expr_Expr* > bb_expr_Expr::m_SemantArgs(Array<bb_expr_Expr* > t_args){
	t_args=t_args.Slice(0);
	for(int t_i=0;t_i<t_args.Length();t_i=t_i+1){
		if((t_args[t_i])!=0){
			gc_assign(t_args[t_i],t_args[t_i]->m_Semant());
		}
	}
	return t_args;
}
bb_expr_Expr* bb_expr_Expr::m_Cast(bb_type_Type* t_ty,int t_castFlags){
	if((f_exprType->m_EqualsType(t_ty))!=0){
		return this;
	}
	return ((new bb_expr_CastExpr)->g_new(t_ty,this,t_castFlags))->m_Semant();
}
Array<bb_expr_Expr* > bb_expr_Expr::m_CastArgs(Array<bb_expr_Expr* > t_args,bb_decl_FuncDecl* t_funcDecl){
	if(t_args.Length()>t_funcDecl->f_argDecls.Length()){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	t_args=t_args.Resize(t_funcDecl->f_argDecls.Length());
	for(int t_i=0;t_i<t_args.Length();t_i=t_i+1){
		if((t_args[t_i])!=0){
			gc_assign(t_args[t_i],t_args[t_i]->m_Cast(t_funcDecl->f_argDecls[t_i]->f_type,0));
		}else{
			if((t_funcDecl->f_argDecls[t_i]->f_init)!=0){
				gc_assign(t_args[t_i],t_funcDecl->f_argDecls[t_i]->f_init);
			}else{
				bb_config_Err(String(L"Missing function argument '",27)+t_funcDecl->f_argDecls[t_i]->f_ident+String(L"'.",2));
			}
		}
	}
	return t_args;
}
String bb_expr_Expr::m_ToString(){
	return String(L"<Expr>",6);
}
String bb_expr_Expr::m_Eval(){
	bb_config_Err(m_ToString()+String(L" cannot be statically evaluated.",32));
	return String();
}
bb_expr_Expr* bb_expr_Expr::m_EvalConst(){
	return ((new bb_expr_ConstExpr)->g_new(f_exprType,m_Eval()))->m_Semant();
}
bb_expr_Expr* bb_expr_Expr::m_Semant2(bb_type_Type* t_ty,int t_castFlags){
	bb_expr_Expr* t_expr=m_Semant();
	if((t_expr->f_exprType->m_EqualsType(t_ty))!=0){
		return t_expr;
	}
	return ((new bb_expr_CastExpr)->g_new(t_ty,t_expr,t_castFlags))->m_Semant();
}
bb_expr_Expr* bb_expr_Expr::m_Copy(){
	bb_config_InternalErr(String(L"Internal error",14));
	return 0;
}
bb_expr_Expr* bb_expr_Expr::m_CopyExpr(bb_expr_Expr* t_expr){
	if(!((t_expr)!=0)){
		return 0;
	}
	return t_expr->m_Copy();
}
Array<bb_expr_Expr* > bb_expr_Expr::m_CopyArgs(Array<bb_expr_Expr* > t_exprs){
	t_exprs=t_exprs.Slice(0);
	for(int t_i=0;t_i<t_exprs.Length();t_i=t_i+1){
		gc_assign(t_exprs[t_i],m_CopyExpr(t_exprs[t_i]));
	}
	return t_exprs;
}
bb_type_Type* bb_expr_Expr::m_BalanceTypes(bb_type_Type* t_lhs,bb_type_Type* t_rhs){
	if(((dynamic_cast<bb_type_StringType*>(t_lhs))!=0) || ((dynamic_cast<bb_type_StringType*>(t_rhs))!=0)){
		return (bb_type_Type::g_stringType);
	}
	if(((dynamic_cast<bb_type_FloatType*>(t_lhs))!=0) || ((dynamic_cast<bb_type_FloatType*>(t_rhs))!=0)){
		return (bb_type_Type::g_floatType);
	}
	if(((dynamic_cast<bb_type_IntType*>(t_lhs))!=0) || ((dynamic_cast<bb_type_IntType*>(t_rhs))!=0)){
		return (bb_type_Type::g_intType);
	}
	if((t_lhs->m_ExtendsType(t_rhs))!=0){
		return t_rhs;
	}
	if((t_rhs->m_ExtendsType(t_lhs))!=0){
		return t_lhs;
	}
	bb_config_Err(String(L"Can't balance types ",20)+t_lhs->m_ToString()+String(L" and ",5)+t_rhs->m_ToString()+String(L".",1));
	return 0;
}
bb_expr_Expr* bb_expr_Expr::m_SemantSet(String t_op,bb_expr_Expr* t_rhs){
	bb_config_Err(m_ToString()+String(L" cannot be assigned to.",23));
	return 0;
}
bb_decl_ScopeDecl* bb_expr_Expr::m_SemantScope(){
	return 0;
}
bb_expr_Expr* bb_expr_Expr::m_SemantFunc(Array<bb_expr_Expr* > t_args){
	bb_config_Err(m_ToString()+String(L" cannot be invoked.",19));
	return 0;
}
bool bb_expr_Expr::m_SideEffects(){
	return true;
}
String bb_expr_Expr::m_Trans(){
	bb_config_Err(String(L"TODO!",5));
	return String();
}
String bb_expr_Expr::m_TransStmt(){
	return m_Trans();
}
String bb_expr_Expr::m_TransVar(){
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
void bb_expr_Expr::mark(){
	Object::mark();
	gc_mark_q(f_exprType);
}
bb_expr_ConstExpr::bb_expr_ConstExpr(){
	f_ty=0;
	f_value=String();
}
bb_expr_ConstExpr* bb_expr_ConstExpr::g_new(bb_type_Type* t_ty,String t_value){
	bb_expr_Expr::g_new();
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		if(t_value.StartsWith(String(L"%",1))){
			t_value=String(bb_config_StringToInt(t_value.Slice(1),2));
		}else{
			if(t_value.StartsWith(String(L"$",1))){
				t_value=String(bb_config_StringToInt(t_value.Slice(1),16));
			}
		}
	}else{
		if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
			if(!(t_value.Contains(String(L"e",1)) || t_value.Contains(String(L"E",1)) || t_value.Contains(String(L".",1)))){
				t_value=t_value+String(L".0",2);
			}
		}
	}
	gc_assign(this->f_ty,t_ty);
	this->f_value=t_value;
	return this;
}
bb_expr_ConstExpr* bb_expr_ConstExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_ConstExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_exprType,f_ty->m_Semant());
	return (this);
}
bb_expr_Expr* bb_expr_ConstExpr::m_Copy(){
	return ((new bb_expr_ConstExpr)->g_new(f_ty,f_value));
}
String bb_expr_ConstExpr::m_ToString(){
	return String(L"ConstExpr(\"",11)+f_value+String(L"\")",2);
}
String bb_expr_ConstExpr::m_Eval(){
	return f_value;
}
bb_expr_Expr* bb_expr_ConstExpr::m_EvalConst(){
	return (this);
}
bool bb_expr_ConstExpr::m_SideEffects(){
	return false;
}
String bb_expr_ConstExpr::m_Trans(){
	return bb_translator__trans->m_TransConstExpr(this);
}
void bb_expr_ConstExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_ty);
}
bb_type_NumericType::bb_type_NumericType(){
}
bb_type_NumericType* bb_type_NumericType::g_new(){
	bb_type_Type::g_new();
	return this;
}
void bb_type_NumericType::mark(){
	bb_type_Type::mark();
}
bb_type_IntType::bb_type_IntType(){
}
bb_type_IntType* bb_type_IntType::g_new(){
	bb_type_NumericType::g_new();
	return this;
}
int bb_type_IntType::m_EqualsType(bb_type_Type* t_ty){
	return ((dynamic_cast<bb_type_IntType*>(t_ty)!=0)?1:0);
}
int bb_type_IntType::m_ExtendsType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		bb_expr_Expr* t_expr=((new bb_expr_ConstExpr)->g_new((this),String()))->m_Semant();
		bb_expr_Expr* t_[]={t_expr};
		bb_decl_FuncDecl* t_ctor=t_ty->m_GetClass()->m_FindFuncDecl(String(L"new",3),Array<bb_expr_Expr* >(t_,1),1);
		return ((((t_ctor)!=0) && t_ctor->m_IsCtor())?1:0);
	}
	return ((dynamic_cast<bb_type_NumericType*>(t_ty)!=0 || dynamic_cast<bb_type_StringType*>(t_ty)!=0)?1:0);
}
bb_decl_ClassDecl* bb_type_IntType::m_GetClass(){
	return dynamic_cast<bb_decl_ClassDecl*>(bb_decl__env->m_FindDecl(String(L"int",3)));
}
String bb_type_IntType::m_ToString(){
	return String(L"Int",3);
}
void bb_type_IntType::mark(){
	bb_type_NumericType::mark();
}
int bb_config_StringToInt(String t_str,int t_base){
	int t_i=0;
	int t_l=t_str.Length();
	while(t_i<t_l && (int)t_str[t_i]<=32){
		t_i+=1;
	}
	bool t_neg=false;
	if(t_i<t_l && ((int)t_str[t_i]==43 || (int)t_str[t_i]==45)){
		t_neg=(int)t_str[t_i]==45;
		t_i+=1;
	}
	int t_n=0;
	while(t_i<t_l){
		int t_c=(int)t_str[t_i];
		int t_t=0;
		if(t_c>=48 && t_c<58){
			t_t=t_c-48;
		}else{
			if(t_c>=65 && t_c<=90){
				t_t=t_c-55;
			}else{
				if(t_c>=97 && t_c<=122){
					t_t=t_c-87;
				}else{
					break;
				}
			}
		}
		if(t_t>=t_base){
			break;
		}
		t_n=t_n*t_base+t_t;
		t_i+=1;
	}
	if(t_neg){
		t_n=-t_n;
	}
	return t_n;
}
bb_type_FloatType::bb_type_FloatType(){
}
bb_type_FloatType* bb_type_FloatType::g_new(){
	bb_type_NumericType::g_new();
	return this;
}
int bb_type_FloatType::m_EqualsType(bb_type_Type* t_ty){
	return ((dynamic_cast<bb_type_FloatType*>(t_ty)!=0)?1:0);
}
int bb_type_FloatType::m_ExtendsType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		bb_expr_Expr* t_expr=((new bb_expr_ConstExpr)->g_new((this),String()))->m_Semant();
		bb_expr_Expr* t_[]={t_expr};
		bb_decl_FuncDecl* t_ctor=t_ty->m_GetClass()->m_FindFuncDecl(String(L"new",3),Array<bb_expr_Expr* >(t_,1),1);
		return ((((t_ctor)!=0) && t_ctor->m_IsCtor())?1:0);
	}
	return ((dynamic_cast<bb_type_NumericType*>(t_ty)!=0 || dynamic_cast<bb_type_StringType*>(t_ty)!=0)?1:0);
}
bb_decl_ClassDecl* bb_type_FloatType::m_GetClass(){
	return dynamic_cast<bb_decl_ClassDecl*>(bb_decl__env->m_FindDecl(String(L"float",5)));
}
String bb_type_FloatType::m_ToString(){
	return String(L"Float",5);
}
void bb_type_FloatType::mark(){
	bb_type_NumericType::mark();
}
int bb_config_InternalErr(String t_err){
	Print(bb_config__errInfo+String(L" : ",3)+t_err);
	Error(bb_config__errInfo+String(L" : ",3)+t_err);
	return 0;
}
bb_list_List::bb_list_List(){
	f__head=((new bb_list_HeadNode)->g_new());
}
bb_list_List* bb_list_List::g_new(){
	return this;
}
bb_list_Node* bb_list_List::m_AddLast(bb_decl_Decl* t_data){
	return (new bb_list_Node)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List* bb_list_List::g_new2(Array<bb_decl_Decl* > t_data){
	Array<bb_decl_Decl* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_Decl* t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast(t_t);
	}
	return this;
}
bb_list_Enumerator2* bb_list_List::m_ObjectEnumerator(){
	return (new bb_list_Enumerator2)->g_new(this);
}
int bb_list_List::m_Count(){
	int t_n=0;
	bb_list_Node* t_node=f__head->f__succ;
	while(t_node!=f__head){
		t_node=t_node->f__succ;
		t_n+=1;
	}
	return t_n;
}
void bb_list_List::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_list_Node::bb_list_Node(){
	f__succ=0;
	f__pred=0;
	f__data=0;
}
bb_list_Node* bb_list_Node::g_new(bb_list_Node* t_succ,bb_list_Node* t_pred,bb_decl_Decl* t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	gc_assign(f__data,t_data);
	return this;
}
bb_list_Node* bb_list_Node::g_new2(){
	return this;
}
void bb_list_Node::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
	gc_mark_q(f__data);
}
bb_list_HeadNode::bb_list_HeadNode(){
}
bb_list_HeadNode* bb_list_HeadNode::g_new(){
	bb_list_Node::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
void bb_list_HeadNode::mark(){
	bb_list_Node::mark();
}
bb_decl_BlockDecl::bb_decl_BlockDecl(){
	f_stmts=(new bb_list_List4)->g_new();
}
bb_decl_BlockDecl* bb_decl_BlockDecl::g_new(bb_decl_ScopeDecl* t_scope){
	bb_decl_ScopeDecl::g_new();
	gc_assign(this->f_scope,t_scope);
	return this;
}
bb_decl_BlockDecl* bb_decl_BlockDecl::g_new2(){
	bb_decl_ScopeDecl::g_new();
	return this;
}
int bb_decl_BlockDecl::m_AddStmt(bb_stmt_Stmt* t_stmt){
	f_stmts->m_AddLast4(t_stmt);
	return 0;
}
bb_decl_Decl* bb_decl_BlockDecl::m_OnCopy(){
	bb_decl_BlockDecl* t_t=(new bb_decl_BlockDecl)->g_new2();
	bb_list_Enumerator5* t_=f_stmts->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_stmt_Stmt* t_stmt=t_->m_NextObject();
		t_t->m_AddStmt(t_stmt->m_Copy2(t_t));
	}
	return (t_t);
}
int bb_decl_BlockDecl::m_OnSemant(){
	bb_decl_PushEnv(this);
	bb_list_Enumerator5* t_=f_stmts->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_stmt_Stmt* t_stmt=t_->m_NextObject();
		t_stmt->m_Semant();
	}
	bb_decl_PopEnv();
	return 0;
}
bb_decl_BlockDecl* bb_decl_BlockDecl::m_CopyBlock(bb_decl_ScopeDecl* t_scope){
	bb_decl_BlockDecl* t_t=dynamic_cast<bb_decl_BlockDecl*>(m_Copy());
	gc_assign(t_t->f_scope,t_scope);
	return t_t;
}
void bb_decl_BlockDecl::mark(){
	bb_decl_ScopeDecl::mark();
	gc_mark_q(f_stmts);
}
bb_decl_FuncDecl::bb_decl_FuncDecl(){
	f_argDecls=Array<bb_decl_ArgDecl* >();
	f_retType=0;
	f_overrides=0;
}
bool bb_decl_FuncDecl::m_IsCtor(){
	return (f_attrs&2)!=0;
}
bool bb_decl_FuncDecl::m_IsMethod(){
	return (f_attrs&1)!=0;
}
String bb_decl_FuncDecl::m_ToString(){
	String t_t=String();
	Array<bb_decl_ArgDecl* > t_=f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_decl=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_decl->m_ToString();
	}
	String t_q=String();
	if(m_IsCtor()){
		t_q=String(L"Method ",7)+bb_decl_Decl::m_ToString();
	}else{
		if(m_IsMethod()){
			t_q=String(L"Method ",7);
		}else{
			t_q=String(L"Function ",9);
		}
		t_q=t_q+(bb_decl_Decl::m_ToString()+String(L":",1));
		t_q=t_q+f_retType->m_ToString();
	}
	return t_q+String(L"(",1)+t_t+String(L")",1);
}
bool bb_decl_FuncDecl::m_EqualsArgs(bb_decl_FuncDecl* t_decl){
	if(f_argDecls.Length()!=t_decl->f_argDecls.Length()){
		return false;
	}
	for(int t_i=0;t_i<f_argDecls.Length();t_i=t_i+1){
		if(!((f_argDecls[t_i]->f_type->m_EqualsType(t_decl->f_argDecls[t_i]->f_type))!=0)){
			return false;
		}
	}
	return true;
}
bool bb_decl_FuncDecl::m_EqualsFunc(bb_decl_FuncDecl* t_decl){
	return ((f_retType->m_EqualsType(t_decl->f_retType))!=0) && m_EqualsArgs(t_decl);
}
bb_decl_FuncDecl* bb_decl_FuncDecl::g_new(String t_ident,int t_attrs,bb_type_Type* t_retType,Array<bb_decl_ArgDecl* > t_argDecls){
	bb_decl_BlockDecl::g_new2();
	this->f_ident=t_ident;
	this->f_attrs=t_attrs;
	gc_assign(this->f_retType,t_retType);
	gc_assign(this->f_argDecls,t_argDecls);
	return this;
}
bb_decl_FuncDecl* bb_decl_FuncDecl::g_new2(){
	bb_decl_BlockDecl::g_new2();
	return this;
}
bb_decl_Decl* bb_decl_FuncDecl::m_OnCopy(){
	Array<bb_decl_ArgDecl* > t_args=f_argDecls.Slice(0);
	for(int t_i=0;t_i<t_args.Length();t_i=t_i+1){
		gc_assign(t_args[t_i],dynamic_cast<bb_decl_ArgDecl*>(t_args[t_i]->m_Copy()));
	}
	bb_decl_FuncDecl* t_t=(new bb_decl_FuncDecl)->g_new(f_ident,f_attrs,f_retType,t_args);
	bb_list_Enumerator5* t_=f_stmts->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_stmt_Stmt* t_stmt=t_->m_NextObject();
		t_t->m_AddStmt(t_stmt->m_Copy2(t_t));
	}
	return (t_t);
}
int bb_decl_FuncDecl::m_OnSemant(){
	bb_decl_ClassDecl* t_cdecl=m_ClassScope();
	bb_decl_ClassDecl* t_sclass=0;
	if((t_cdecl)!=0){
		t_sclass=t_cdecl->f_superClass;
	}
	if(m_IsCtor()){
		gc_assign(f_retType,(t_cdecl->f_objectType));
	}else{
		gc_assign(f_retType,f_retType->m_Semant());
	}
	Array<bb_decl_ArgDecl* > t_=f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_arg=t_[t_2];
		t_2=t_2+1;
		m_InsertDecl(t_arg);
		t_arg->m_Semant();
	}
	bb_list_Enumerator* t_3=f_scope->m_SemantedFuncs(f_ident)->m_ObjectEnumerator();
	while(t_3->m_HasNext()){
		bb_decl_FuncDecl* t_decl=t_3->m_NextObject();
		if(t_decl!=this && m_EqualsArgs(t_decl)){
			bb_config_Err(String(L"Duplicate declaration ",22)+m_ToString());
		}
	}
	if(m_IsCtor() && !((f_attrs&8)!=0)){
		if((t_sclass->m_FindFuncDecl(String(L"new",3),Array<bb_expr_Expr* >(),0))!=0){
			bb_expr_InvokeSuperExpr* t_expr=(new bb_expr_InvokeSuperExpr)->g_new(String(L"new",3),Array<bb_expr_Expr* >());
			f_stmts->m_AddFirst((new bb_stmt_ExprStmt)->g_new(t_expr));
		}
	}
	if(((t_sclass)!=0) && m_IsMethod()){
		while((t_sclass)!=0){
			int t_found=0;
			bb_list_Enumerator* t_4=t_sclass->m_MethodDecls(f_ident)->m_ObjectEnumerator();
			while(t_4->m_HasNext()){
				bb_decl_FuncDecl* t_decl2=t_4->m_NextObject();
				t_found=1;
				t_decl2->m_Semant();
				if(m_EqualsFunc(t_decl2)){
					gc_assign(f_overrides,t_decl2);
					break;
				}
			}
			if((t_found)!=0){
				if(!((f_overrides)!=0)){
					bb_config_Err(String(L"Overriding method does not match any overridden method.",55));
				}
				if((f_overrides->m_IsFinal())!=0){
					bb_config_Err(String(L"Cannot override final method.",29));
				}
				if((f_overrides->f_munged).Length()!=0){
					if(((f_munged).Length()!=0) && f_munged!=f_overrides->f_munged){
						bb_config_InternalErr(String(L"Internal error",14));
					}
					f_munged=f_overrides->f_munged;
				}
				break;
			}
			t_sclass=t_sclass->f_superClass;
		}
	}
	f_attrs|=1048576;
	bb_decl_BlockDecl::m_OnSemant();
	return 0;
}
bool bb_decl_FuncDecl::m_IsStatic(){
	return (f_attrs&3)==0;
}
bool bb_decl_FuncDecl::m_IsProperty(){
	return (f_attrs&4)!=0;
}
void bb_decl_FuncDecl::mark(){
	bb_decl_BlockDecl::mark();
	gc_mark_q(f_argDecls);
	gc_mark_q(f_retType);
	gc_mark_q(f_overrides);
}
bb_list_List2::bb_list_List2(){
	f__head=((new bb_list_HeadNode2)->g_new());
}
bb_list_List2* bb_list_List2::g_new(){
	return this;
}
bb_list_Node2* bb_list_List2::m_AddLast2(bb_decl_FuncDecl* t_data){
	return (new bb_list_Node2)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List2* bb_list_List2::g_new2(Array<bb_decl_FuncDecl* > t_data){
	Array<bb_decl_FuncDecl* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_FuncDecl* t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast2(t_t);
	}
	return this;
}
bb_list_Enumerator* bb_list_List2::m_ObjectEnumerator(){
	return (new bb_list_Enumerator)->g_new(this);
}
bool bb_list_List2::m_IsEmpty(){
	return f__head->f__succ==f__head;
}
int bb_list_List2::m_Count(){
	int t_n=0;
	bb_list_Node2* t_node=f__head->f__succ;
	while(t_node!=f__head){
		t_node=t_node->f__succ;
		t_n+=1;
	}
	return t_n;
}
void bb_list_List2::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_decl_FuncDeclList::bb_decl_FuncDeclList(){
}
bb_decl_FuncDeclList* bb_decl_FuncDeclList::g_new(){
	bb_list_List2::g_new();
	return this;
}
void bb_decl_FuncDeclList::mark(){
	bb_list_List2::mark();
}
bb_list_Node2::bb_list_Node2(){
	f__succ=0;
	f__pred=0;
	f__data=0;
}
bb_list_Node2* bb_list_Node2::g_new(bb_list_Node2* t_succ,bb_list_Node2* t_pred,bb_decl_FuncDecl* t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	gc_assign(f__data,t_data);
	return this;
}
bb_list_Node2* bb_list_Node2::g_new2(){
	return this;
}
void bb_list_Node2::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
	gc_mark_q(f__data);
}
bb_list_HeadNode2::bb_list_HeadNode2(){
}
bb_list_HeadNode2* bb_list_HeadNode2::g_new(){
	bb_list_Node2::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
void bb_list_HeadNode2::mark(){
	bb_list_Node2::mark();
}
int bb_config_Err(String t_err){
	Print(bb_config__errInfo+String(L" : Error : ",11)+t_err);
	ExitApp(-1);
	return 0;
}
bb_decl_ScopeDecl* bb_decl__env;
bb_list_List3::bb_list_List3(){
	f__head=((new bb_list_HeadNode3)->g_new());
}
bb_list_List3* bb_list_List3::g_new(){
	return this;
}
bb_list_Node3* bb_list_List3::m_AddLast3(bb_decl_ScopeDecl* t_data){
	return (new bb_list_Node3)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List3* bb_list_List3::g_new2(Array<bb_decl_ScopeDecl* > t_data){
	Array<bb_decl_ScopeDecl* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ScopeDecl* t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast3(t_t);
	}
	return this;
}
bool bb_list_List3::m_IsEmpty(){
	return f__head->f__succ==f__head;
}
bb_decl_ScopeDecl* bb_list_List3::m_RemoveLast(){
	bb_decl_ScopeDecl* t_data=f__head->m_PrevNode()->f__data;
	f__head->f__pred->m_Remove();
	return t_data;
}
void bb_list_List3::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_list_Node3::bb_list_Node3(){
	f__succ=0;
	f__pred=0;
	f__data=0;
}
bb_list_Node3* bb_list_Node3::g_new(bb_list_Node3* t_succ,bb_list_Node3* t_pred,bb_decl_ScopeDecl* t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	gc_assign(f__data,t_data);
	return this;
}
bb_list_Node3* bb_list_Node3::g_new2(){
	return this;
}
bb_list_Node3* bb_list_Node3::m_GetNode(){
	return this;
}
bb_list_Node3* bb_list_Node3::m_PrevNode(){
	return f__pred->m_GetNode();
}
int bb_list_Node3::m_Remove(){
	gc_assign(f__succ->f__pred,f__pred);
	gc_assign(f__pred->f__succ,f__succ);
	return 0;
}
void bb_list_Node3::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
	gc_mark_q(f__data);
}
bb_list_HeadNode3::bb_list_HeadNode3(){
}
bb_list_HeadNode3* bb_list_HeadNode3::g_new(){
	bb_list_Node3::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
bb_list_Node3* bb_list_HeadNode3::m_GetNode(){
	return 0;
}
void bb_list_HeadNode3::mark(){
	bb_list_Node3::mark();
}
bb_list_List3* bb_decl__envStack;
int bb_decl_PushEnv(bb_decl_ScopeDecl* t_env){
	bb_decl__envStack->m_AddLast3(bb_decl__env);
	gc_assign(bb_decl__env,t_env);
	return 0;
}
bb_parser_Parser::bb_parser_Parser(){
	f__toke=String();
	f__toker=0;
	f__app=0;
	f__module=0;
	f__defattrs=0;
	f__tokeType=0;
	f__block=0;
	f__blockStack=(new bb_list_List8)->g_new();
	f__errStack=(new bb_list_StringList)->g_new();
}
int bb_parser_Parser::m_SetErr(){
	if((f__toker->m_Path()).Length()!=0){
		bb_config__errInfo=f__toker->m_Path()+String(L"<",1)+String(f__toker->m_Line())+String(L">",1);
	}
	return 0;
}
int bb_parser_Parser::m_CParse(String t_toke){
	if(f__toke!=t_toke){
		return 0;
	}
	m_NextToke();
	return 1;
}
int bb_parser_Parser::m_SkipEols(){
	while((m_CParse(String(L"\n",1)))!=0){
	}
	m_SetErr();
	return 0;
}
String bb_parser_Parser::m_NextToke(){
	String t_toke=f__toke;
	do{
		f__toke=f__toker->m_NextToke();
		f__tokeType=f__toker->m_TokeType();
	}while(!(f__tokeType!=1));
	int t_=f__tokeType;
	if(t_==3){
		f__toke=f__toke.ToLower();
	}else{
		if(t_==8){
			if((int)f__toke[0]==91 && (int)f__toke[f__toke.Length()-1]==93){
				f__toke=String(L"[]",2);
			}
		}
	}
	if(t_toke==String(L",",1)){
		m_SkipEols();
	}
	return f__toke;
}
bb_parser_Parser* bb_parser_Parser::g_new(bb_toker_Toker* t_toker,bb_decl_AppDecl* t_app,bb_decl_ModuleDecl* t_mdecl,int t_defattrs){
	f__toke=String(L"\n",1);
	gc_assign(f__toker,t_toker);
	gc_assign(f__app,t_app);
	gc_assign(f__module,t_mdecl);
	f__defattrs=t_defattrs;
	m_SetErr();
	m_NextToke();
	return this;
}
bb_parser_Parser* bb_parser_Parser::g_new2(){
	return this;
}
int bb_parser_Parser::m_Parse(String t_toke){
	if(!((m_CParse(t_toke))!=0)){
		bb_config_Err(String(L"Syntax error - expecting '",26)+t_toke+String(L"'.",2));
	}
	return 0;
}
bb_expr_ArrayExpr* bb_parser_Parser::m_ParseArrayExpr(){
	m_Parse(String(L"[",1));
	bb_stack_Stack2* t_args=(new bb_stack_Stack2)->g_new();
	do{
		t_args->m_Push4(m_ParseExpr());
	}while(!(!((m_CParse(String(L",",1)))!=0)));
	m_Parse(String(L"]",1));
	return (new bb_expr_ArrayExpr)->g_new(t_args->m_ToArray());
}
bb_type_Type* bb_parser_Parser::m_CParsePrimitiveType(){
	if((m_CParse(String(L"void",4)))!=0){
		return (bb_type_Type::g_voidType);
	}
	if((m_CParse(String(L"bool",4)))!=0){
		return (bb_type_Type::g_boolType);
	}
	if((m_CParse(String(L"int",3)))!=0){
		return (bb_type_Type::g_intType);
	}
	if((m_CParse(String(L"float",5)))!=0){
		return (bb_type_Type::g_floatType);
	}
	if((m_CParse(String(L"string",6)))!=0){
		return (bb_type_Type::g_stringType);
	}
	if((m_CParse(String(L"object",6)))!=0){
		return (bb_type_Type::g_objectType);
	}
	if((m_CParse(String(L"throwable",9)))!=0){
		return (bb_type_Type::g_throwableType);
	}
	return 0;
}
String bb_parser_Parser::m_ParseIdent(){
	String t_=f__toke;
	if(t_==String(L"@",1)){
		m_NextToke();
	}else{
		if(t_==String(L"object",6) || t_==String(L"throwable",9)){
		}else{
			if(f__tokeType!=2){
				bb_config_Err(String(L"Syntax error - expecting identifier.",36));
			}
		}
	}
	String t_id=f__toke;
	m_NextToke();
	return t_id;
}
bb_type_IdentType* bb_parser_Parser::m_ParseIdentType(){
	String t_id=m_ParseIdent();
	if((m_CParse(String(L".",1)))!=0){
		t_id=t_id+(String(L".",1)+m_ParseIdent());
	}
	bb_stack_Stack3* t_args=(new bb_stack_Stack3)->g_new();
	if((m_CParse(String(L"<",1)))!=0){
		do{
			bb_type_Type* t_arg=m_ParseType();
			while((m_CParse(String(L"[]",2)))!=0){
				t_arg=(t_arg->m_ArrayOf());
			}
			t_args->m_Push7(t_arg);
		}while(!(!((m_CParse(String(L",",1)))!=0)));
		m_Parse(String(L">",1));
	}
	return (new bb_type_IdentType)->g_new(t_id,t_args->m_ToArray());
}
bb_type_Type* bb_parser_Parser::m_ParseType(){
	bb_type_Type* t_ty=m_CParsePrimitiveType();
	if((t_ty)!=0){
		return t_ty;
	}
	return (m_ParseIdentType());
}
int bb_parser_Parser::m_AtEos(){
	return ((f__toke==String() || f__toke==String(L";",1) || f__toke==String(L"\n",1) || f__toke==String(L"else",4))?1:0);
}
Array<bb_expr_Expr* > bb_parser_Parser::m_ParseArgs(int t_stmt){
	Array<bb_expr_Expr* > t_args=Array<bb_expr_Expr* >();
	if((t_stmt)!=0){
		if((m_AtEos())!=0){
			return t_args;
		}
	}else{
		if(f__toke!=String(L"(",1)){
			return t_args;
		}
	}
	int t_nargs=0;
	int t_eat=0;
	if(f__toke==String(L"(",1)){
		if((t_stmt)!=0){
			bb_toker_Toker* t_toker=(new bb_toker_Toker)->g_new2(f__toker);
			int t_bra=1;
			do{
				t_toker->m_NextToke();
				t_toker->m_SkipSpace();
				String t_=t_toker->m_Toke().ToLower();
				if(t_==String() || t_==String(L"else",4)){
					bb_config_Err(String(L"Parenthesis mismatch error.",27));
				}else{
					if(t_==String(L"(",1) || t_==String(L"[",1)){
						t_bra+=1;
					}else{
						if(t_==String(L"]",1) || t_==String(L")",1)){
							t_bra-=1;
							if((t_bra)!=0){
								continue;
							}
							t_toker->m_NextToke();
							t_toker->m_SkipSpace();
							String t_2=t_toker->m_Toke().ToLower();
							if(t_2==String(L".",1) || t_2==String(L"(",1) || t_2==String(L"[",1) || t_2==String() || t_2==String(L";",1) || t_2==String(L"\n",1) || t_2==String(L"else",4)){
								t_eat=1;
							}
							break;
						}else{
							if(t_==String(L",",1)){
								if(t_bra!=1){
									continue;
								}
								t_eat=1;
								break;
							}
						}
					}
				}
			}while(!(false));
		}else{
			t_eat=1;
		}
		if(((t_eat)!=0) && m_NextToke()==String(L")",1)){
			m_NextToke();
			return t_args;
		}
	}
	do{
		bb_expr_Expr* t_arg=0;
		if(((f__toke).Length()!=0) && f__toke!=String(L",",1)){
			t_arg=m_ParseExpr();
		}
		if(t_args.Length()==t_nargs){
			t_args=t_args.Resize(t_nargs+10);
		}
		gc_assign(t_args[t_nargs],t_arg);
		t_nargs+=1;
	}while(!(!((m_CParse(String(L",",1)))!=0)));
	t_args=t_args.Slice(0,t_nargs);
	if((t_eat)!=0){
		m_Parse(String(L")",1));
	}
	return t_args;
}
bb_type_IdentType* bb_parser_Parser::m_CParseIdentType(bool t_inner){
	if(f__tokeType!=2){
		return 0;
	}
	String t_id=m_ParseIdent();
	if((m_CParse(String(L".",1)))!=0){
		if(f__tokeType!=2){
			return 0;
		}
		t_id=t_id+(String(L".",1)+m_ParseIdent());
	}
	if(!((m_CParse(String(L"<",1)))!=0)){
		if(t_inner){
			return (new bb_type_IdentType)->g_new(t_id,Array<bb_type_Type* >());
		}
		return 0;
	}
	bb_stack_Stack3* t_args=(new bb_stack_Stack3)->g_new();
	do{
		bb_type_Type* t_arg=m_CParsePrimitiveType();
		if(!((t_arg)!=0)){
			t_arg=(m_CParseIdentType(true));
			if(!((t_arg)!=0)){
				return 0;
			}
		}
		while((m_CParse(String(L"[]",2)))!=0){
			t_arg=(t_arg->m_ArrayOf());
		}
		t_args->m_Push7(t_arg);
	}while(!(!((m_CParse(String(L",",1)))!=0)));
	if(!((m_CParse(String(L">",1)))!=0)){
		return 0;
	}
	return (new bb_type_IdentType)->g_new(t_id,t_args->m_ToArray());
}
bb_expr_Expr* bb_parser_Parser::m_ParsePrimaryExpr(int t_stmt){
	bb_expr_Expr* t_expr=0;
	String t_=f__toke;
	if(t_==String(L"(",1)){
		m_NextToke();
		t_expr=m_ParseExpr();
		m_Parse(String(L")",1));
	}else{
		if(t_==String(L"[",1)){
			t_expr=(m_ParseArrayExpr());
		}else{
			if(t_==String(L"[]",2)){
				m_NextToke();
				t_expr=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_emptyArrayType),String()));
			}else{
				if(t_==String(L".",1)){
					t_expr=((new bb_parser_ScopeExpr)->g_new(f__module));
				}else{
					if(t_==String(L"new",3)){
						m_NextToke();
						bb_type_Type* t_ty=m_ParseType();
						if((m_CParse(String(L"[",1)))!=0){
							bb_expr_Expr* t_len=m_ParseExpr();
							m_Parse(String(L"]",1));
							while((m_CParse(String(L"[]",2)))!=0){
								t_ty=(t_ty->m_ArrayOf());
							}
							t_expr=((new bb_expr_NewArrayExpr)->g_new(t_ty,t_len));
						}else{
							t_expr=((new bb_expr_NewObjectExpr)->g_new(t_ty,m_ParseArgs(t_stmt)));
						}
					}else{
						if(t_==String(L"null",4)){
							m_NextToke();
							t_expr=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_nullObjectType),String()));
						}else{
							if(t_==String(L"true",4)){
								m_NextToke();
								t_expr=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_boolType),String(L"1",1)));
							}else{
								if(t_==String(L"false",5)){
									m_NextToke();
									t_expr=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_boolType),String()));
								}else{
									if(t_==String(L"bool",4) || t_==String(L"int",3) || t_==String(L"float",5) || t_==String(L"string",6) || t_==String(L"object",6) || t_==String(L"throwable",9)){
										String t_id=f__toke;
										bb_type_Type* t_ty2=m_ParseType();
										if(((m_CParse(String(L"(",1)))!=0)){
											t_expr=m_ParseExpr();
											m_Parse(String(L")",1));
											t_expr=((new bb_expr_CastExpr)->g_new(t_ty2,t_expr,1));
										}else{
											t_expr=((new bb_parser_IdentExpr)->g_new(t_id,0));
										}
									}else{
										if(t_==String(L"self",4)){
											m_NextToke();
											t_expr=((new bb_expr_SelfExpr)->g_new());
										}else{
											if(t_==String(L"super",5)){
												m_NextToke();
												m_Parse(String(L".",1));
												if(f__toke==String(L"new",3)){
													m_NextToke();
													bb_decl_FuncDecl* t_func=dynamic_cast<bb_decl_FuncDecl*>(f__block);
													if(!((t_func)!=0) || !((t_stmt)!=0) || !t_func->m_IsCtor() || !t_func->f_stmts->m_IsEmpty()){
														bb_config_Err(String(L"Call to Super.new must be first statement in a constructor.",59));
													}
													t_expr=((new bb_expr_InvokeSuperExpr)->g_new(String(L"new",3),m_ParseArgs(t_stmt)));
													t_func->f_attrs|=8;
												}else{
													String t_id2=m_ParseIdent();
													t_expr=((new bb_expr_InvokeSuperExpr)->g_new(t_id2,m_ParseArgs(t_stmt)));
												}
											}else{
												int t_2=f__tokeType;
												if(t_2==2){
													bb_toker_Toker* t_toker=(new bb_toker_Toker)->g_new2(f__toker);
													bb_type_IdentType* t_ty3=m_CParseIdentType(false);
													if((t_ty3)!=0){
														t_expr=((new bb_parser_IdentTypeExpr)->g_new(t_ty3));
													}else{
														gc_assign(f__toker,t_toker);
														f__toke=f__toker->m_Toke();
														f__tokeType=f__toker->m_TokeType();
														t_expr=((new bb_parser_IdentExpr)->g_new(m_ParseIdent(),0));
													}
												}else{
													if(t_2==4){
														t_expr=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_intType),f__toke));
														m_NextToke();
													}else{
														if(t_2==5){
															t_expr=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_floatType),f__toke));
															m_NextToke();
														}else{
															if(t_2==6){
																t_expr=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_stringType),bb_config_Dequote(f__toke,String(L"monkey",6))));
																m_NextToke();
															}else{
																bb_config_Err(String(L"Syntax error - unexpected token '",33)+f__toke+String(L"'",1));
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	do{
		String t_3=f__toke;
		if(t_3==String(L".",1)){
			m_NextToke();
			if(true){
				String t_id3=m_ParseIdent();
				t_expr=((new bb_parser_IdentExpr)->g_new(t_id3,t_expr));
			}
		}else{
			if(t_3==String(L"(",1)){
				t_expr=((new bb_parser_FuncCallExpr)->g_new(t_expr,m_ParseArgs(t_stmt)));
			}else{
				if(t_3==String(L"[",1)){
					m_NextToke();
					if((m_CParse(String(L"..",2)))!=0){
						if(f__toke==String(L"]",1)){
							t_expr=((new bb_expr_SliceExpr)->g_new(t_expr,0,0));
						}else{
							t_expr=((new bb_expr_SliceExpr)->g_new(t_expr,0,m_ParseExpr()));
						}
					}else{
						bb_expr_Expr* t_from=m_ParseExpr();
						if((m_CParse(String(L"..",2)))!=0){
							if(f__toke==String(L"]",1)){
								t_expr=((new bb_expr_SliceExpr)->g_new(t_expr,t_from,0));
							}else{
								t_expr=((new bb_expr_SliceExpr)->g_new(t_expr,t_from,m_ParseExpr()));
							}
						}else{
							t_expr=((new bb_expr_IndexExpr)->g_new(t_expr,t_from));
						}
					}
					m_Parse(String(L"]",1));
				}else{
					return t_expr;
				}
			}
		}
	}while(!(false));
}
bb_expr_Expr* bb_parser_Parser::m_ParseUnaryExpr(){
	m_SkipEols();
	String t_op=f__toke;
	String t_=t_op;
	if(t_==String(L"+",1) || t_==String(L"-",1) || t_==String(L"~",1) || t_==String(L"not",3)){
		m_NextToke();
		bb_expr_Expr* t_expr=m_ParseUnaryExpr();
		return ((new bb_expr_UnaryExpr)->g_new(t_op,t_expr));
	}
	return m_ParsePrimaryExpr(0);
}
bb_expr_Expr* bb_parser_Parser::m_ParseMulDivExpr(){
	bb_expr_Expr* t_expr=m_ParseUnaryExpr();
	do{
		String t_op=f__toke;
		String t_=t_op;
		if(t_==String(L"*",1) || t_==String(L"/",1) || t_==String(L"mod",3) || t_==String(L"shl",3) || t_==String(L"shr",3)){
			m_NextToke();
			bb_expr_Expr* t_rhs=m_ParseUnaryExpr();
			t_expr=((new bb_expr_BinaryMathExpr)->g_new(t_op,t_expr,t_rhs));
		}else{
			return t_expr;
		}
	}while(!(false));
}
bb_expr_Expr* bb_parser_Parser::m_ParseAddSubExpr(){
	bb_expr_Expr* t_expr=m_ParseMulDivExpr();
	do{
		String t_op=f__toke;
		String t_=t_op;
		if(t_==String(L"+",1) || t_==String(L"-",1)){
			m_NextToke();
			bb_expr_Expr* t_rhs=m_ParseMulDivExpr();
			t_expr=((new bb_expr_BinaryMathExpr)->g_new(t_op,t_expr,t_rhs));
		}else{
			return t_expr;
		}
	}while(!(false));
}
bb_expr_Expr* bb_parser_Parser::m_ParseBitandExpr(){
	bb_expr_Expr* t_expr=m_ParseAddSubExpr();
	do{
		String t_op=f__toke;
		String t_=t_op;
		if(t_==String(L"&",1) || t_==String(L"~",1)){
			m_NextToke();
			bb_expr_Expr* t_rhs=m_ParseAddSubExpr();
			t_expr=((new bb_expr_BinaryMathExpr)->g_new(t_op,t_expr,t_rhs));
		}else{
			return t_expr;
		}
	}while(!(false));
}
bb_expr_Expr* bb_parser_Parser::m_ParseBitorExpr(){
	bb_expr_Expr* t_expr=m_ParseBitandExpr();
	do{
		String t_op=f__toke;
		String t_=t_op;
		if(t_==String(L"|",1)){
			m_NextToke();
			bb_expr_Expr* t_rhs=m_ParseBitandExpr();
			t_expr=((new bb_expr_BinaryMathExpr)->g_new(t_op,t_expr,t_rhs));
		}else{
			return t_expr;
		}
	}while(!(false));
}
bb_expr_Expr* bb_parser_Parser::m_ParseCompareExpr(){
	bb_expr_Expr* t_expr=m_ParseBitorExpr();
	do{
		String t_op=f__toke;
		String t_=t_op;
		if(t_==String(L"=",1) || t_==String(L"<",1) || t_==String(L">",1) || t_==String(L"<=",2) || t_==String(L">=",2) || t_==String(L"<>",2)){
			m_NextToke();
			if(t_op==String(L">",1) && f__toke==String(L"=",1)){
				t_op=t_op+f__toke;
				m_NextToke();
			}else{
				if(t_op==String(L"<",1) && (f__toke==String(L"=",1) || f__toke==String(L">",1))){
					t_op=t_op+f__toke;
					m_NextToke();
				}
			}
			bb_expr_Expr* t_rhs=m_ParseBitorExpr();
			t_expr=((new bb_expr_BinaryCompareExpr)->g_new(t_op,t_expr,t_rhs));
		}else{
			return t_expr;
		}
	}while(!(false));
}
bb_expr_Expr* bb_parser_Parser::m_ParseAndExpr(){
	bb_expr_Expr* t_expr=m_ParseCompareExpr();
	do{
		String t_op=f__toke;
		if(t_op==String(L"and",3)){
			m_NextToke();
			bb_expr_Expr* t_rhs=m_ParseCompareExpr();
			t_expr=((new bb_expr_BinaryLogicExpr)->g_new(t_op,t_expr,t_rhs));
		}else{
			return t_expr;
		}
	}while(!(false));
}
bb_expr_Expr* bb_parser_Parser::m_ParseOrExpr(){
	bb_expr_Expr* t_expr=m_ParseAndExpr();
	do{
		String t_op=f__toke;
		if(t_op==String(L"or",2)){
			m_NextToke();
			bb_expr_Expr* t_rhs=m_ParseAndExpr();
			t_expr=((new bb_expr_BinaryLogicExpr)->g_new(t_op,t_expr,t_rhs));
		}else{
			return t_expr;
		}
	}while(!(false));
}
bb_expr_Expr* bb_parser_Parser::m_ParseExpr(){
	return m_ParseOrExpr();
}
int bb_parser_Parser::m_ValidateModIdent(String t_id){
	if((t_id.Length())!=0){
		if(((bb_config_IsAlpha((int)t_id[0]))!=0) || (int)t_id[0]==95){
			int t_err=0;
			for(int t_i=1;t_i<t_id.Length();t_i=t_i+1){
				if(((bb_config_IsAlpha((int)t_id[t_i]))!=0) || ((bb_config_IsDigit((int)t_id[t_i]))!=0) || (int)t_id[t_i]==95){
					continue;
				}
				t_err=1;
				break;
			}
			if(!((t_err)!=0)){
				return 0;
			}
		}
	}
	bb_config_Err(String(L"Invalid module identifier '",27)+t_id+String(L"'.",2));
	return 0;
}
String bb_parser_Parser::m_RealPath(String t_path){
	String t_popDir=CurrentDir();
	ChangeDir(bb_os_ExtractDir(f__toker->m_Path()));
	t_path=RealPath(t_path);
	ChangeDir(t_popDir);
	return t_path;
}
int bb_parser_Parser::m_ImportModule(String t_modpath,int t_attrs){
	String t_filepath=String();
	String t_cd=CurrentDir();
	ChangeDir(bb_os_ExtractDir(f__toker->m_Path()));
	Array<String > t_=bb_config_GetCfgVar(String(L"MODPATH",7)).Split(String(L";",1));
	int t_2=0;
	while(t_2<t_.Length()){
		String t_dir=t_[t_2];
		t_2=t_2+1;
		if(!((t_dir).Length()!=0)){
			continue;
		}
		t_filepath=m_RealPath(t_dir)+String(L"/",1)+t_modpath.Replace(String(L".",1),String(L"/",1))+String(L".",1)+bb_parser_FILE_EXT;
		String t_filepath2=bb_os_StripExt(t_filepath)+String(L"/",1)+bb_os_StripDir(t_filepath);
		if(FileType(t_filepath)==1){
			if(FileType(t_filepath2)!=1){
				break;
			}
			bb_config_Err(String(L"Duplicate module file: '",24)+t_filepath+String(L"' and '",7)+t_filepath2+String(L"'.",2));
		}
		t_filepath=t_filepath2;
		if(FileType(t_filepath)==1){
			break;
		}
		t_filepath=String();
	}
	ChangeDir(t_cd);
	if(!((t_filepath).Length()!=0)){
		bb_config_Err(String(L"Module '",8)+t_modpath+String(L"' not found.",12));
	}
	if(f__module->f_imported->m_Contains(t_filepath)){
		return 0;
	}
	bb_decl_ModuleDecl* t_mdecl=f__app->f_imported->m_ValueForKey(t_filepath);
	if(!((t_mdecl)!=0)){
		t_mdecl=bb_parser_ParseModule(t_filepath,f__app);
	}
	f__module->f_imported->m_Insert3(t_mdecl->f_filepath,t_mdecl);
	if(!((t_attrs&512)!=0)){
		f__module->f_pubImported->m_Insert3(t_mdecl->f_filepath,t_mdecl);
	}
	f__module->m_InsertDecl((new bb_decl_AliasDecl)->g_new(t_mdecl->f_ident,t_attrs,(t_mdecl)));
	return 0;
}
String bb_parser_Parser::m_ParseStringLit(){
	if(f__tokeType!=6){
		bb_config_Err(String(L"Expecting string literal.",25));
	}
	String t_str=bb_config_Dequote(f__toke,String(L"monkey",6));
	m_NextToke();
	return t_str;
}
int bb_parser_Parser::m_ImportFile(String t_filepath){
	if((bb_config_ENV_SAFEMODE)!=0){
		if(f__app->f_mainModule==f__module){
			bb_config_Err(String(L"Import of external files not permitted in safe mode.",52));
		}
	}
	t_filepath=m_RealPath(t_filepath);
	if(FileType(t_filepath)!=1){
		bb_config_Err(String(L"File '",6)+t_filepath+String(L"' not found.",12));
	}
	f__app->f_fileImports->m_AddLast5(t_filepath);
	return 0;
}
String bb_parser_Parser::m_ParseModPath(){
	String t_path=m_ParseIdent();
	while((m_CParse(String(L".",1)))!=0){
		t_path=t_path+(String(L".",1)+m_ParseIdent());
	}
	return t_path;
}
bb_type_Type* bb_parser_Parser::m_ParseDeclType(){
	bb_type_Type* t_ty=0;
	String t_=f__toke;
	if(t_==String(L"?",1)){
		m_NextToke();
		t_ty=(bb_type_Type::g_boolType);
	}else{
		if(t_==String(L"%",1)){
			m_NextToke();
			t_ty=(bb_type_Type::g_intType);
		}else{
			if(t_==String(L"#",1)){
				m_NextToke();
				t_ty=(bb_type_Type::g_floatType);
			}else{
				if(t_==String(L"$",1)){
					m_NextToke();
					t_ty=(bb_type_Type::g_stringType);
				}else{
					if(t_==String(L":",1)){
						m_NextToke();
						t_ty=m_ParseType();
					}else{
						if((f__module->m_IsStrict())!=0){
							bb_config_Err(String(L"Illegal type expression.",24));
						}
						t_ty=(bb_type_Type::g_intType);
					}
				}
			}
		}
	}
	while((m_CParse(String(L"[]",2)))!=0){
		t_ty=(t_ty->m_ArrayOf());
	}
	return t_ty;
}
bb_decl_Decl* bb_parser_Parser::m_ParseDecl(String t_toke,int t_attrs){
	m_SetErr();
	String t_id=m_ParseIdent();
	bb_type_Type* t_ty=0;
	bb_expr_Expr* t_init=0;
	if((t_attrs&256)!=0){
		t_ty=m_ParseDeclType();
	}else{
		if((m_CParse(String(L":=",2)))!=0){
			t_init=m_ParseExpr();
		}else{
			t_ty=m_ParseDeclType();
			if((m_CParse(String(L"=",1)))!=0){
				t_init=m_ParseExpr();
			}else{
				if((m_CParse(String(L"[",1)))!=0){
					bb_expr_Expr* t_len=m_ParseExpr();
					m_Parse(String(L"]",1));
					while((m_CParse(String(L"[]",2)))!=0){
						t_ty=(t_ty->m_ArrayOf());
					}
					t_init=((new bb_expr_NewArrayExpr)->g_new(t_ty,t_len));
					t_ty=(t_ty->m_ArrayOf());
				}else{
					if(t_toke!=String(L"const",5)){
						t_init=((new bb_expr_ConstExpr)->g_new(t_ty,String()));
					}else{
						bb_config_Err(String(L"Constants must be initialized.",30));
					}
				}
			}
		}
	}
	bb_decl_ValDecl* t_decl=0;
	String t_=t_toke;
	if(t_==String(L"global",6)){
		t_decl=((new bb_decl_GlobalDecl)->g_new(t_id,t_attrs,t_ty,t_init));
	}else{
		if(t_==String(L"field",5)){
			t_decl=((new bb_decl_FieldDecl)->g_new(t_id,t_attrs,t_ty,t_init));
		}else{
			if(t_==String(L"const",5)){
				t_decl=((new bb_decl_ConstDecl)->g_new(t_id,t_attrs,t_ty,t_init));
			}else{
				if(t_==String(L"local",5)){
					t_decl=((new bb_decl_LocalDecl)->g_new(t_id,t_attrs,t_ty,t_init));
				}
			}
		}
	}
	if(((t_decl->m_IsExtern())!=0) || ((m_CParse(String(L"extern",6)))!=0)){
		t_decl->f_munged=t_decl->f_ident;
		if((m_CParse(String(L"=",1)))!=0){
			t_decl->f_munged=m_ParseStringLit();
		}
	}
	return (t_decl);
}
bb_list_List* bb_parser_Parser::m_ParseDecls(String t_toke,int t_attrs){
	if((t_toke).Length()!=0){
		m_Parse(t_toke);
	}
	bb_list_List* t_decls=(new bb_list_List)->g_new();
	do{
		bb_decl_Decl* t_decl=m_ParseDecl(t_toke,t_attrs);
		t_decls->m_AddLast(t_decl);
		if(!((m_CParse(String(L",",1)))!=0)){
			return t_decls;
		}
	}while(!(false));
}
int bb_parser_Parser::m_PushBlock(bb_decl_BlockDecl* t_block){
	f__blockStack->m_AddLast8(f__block);
	f__errStack->m_AddLast5(bb_config__errInfo);
	gc_assign(f__block,t_block);
	return 0;
}
int bb_parser_Parser::m_ParseDeclStmts(){
	String t_toke=f__toke;
	m_NextToke();
	do{
		bb_decl_Decl* t_decl=m_ParseDecl(t_toke,0);
		f__block->m_AddStmt((new bb_stmt_DeclStmt)->g_new(t_decl));
	}while(!(!((m_CParse(String(L",",1)))!=0)));
	return 0;
}
int bb_parser_Parser::m_ParseReturnStmt(){
	m_Parse(String(L"return",6));
	bb_expr_Expr* t_expr=0;
	if(!((m_AtEos())!=0)){
		t_expr=m_ParseExpr();
	}
	f__block->m_AddStmt((new bb_stmt_ReturnStmt)->g_new(t_expr));
	return 0;
}
int bb_parser_Parser::m_ParseExitStmt(){
	m_Parse(String(L"exit",4));
	f__block->m_AddStmt((new bb_stmt_BreakStmt)->g_new());
	return 0;
}
int bb_parser_Parser::m_ParseContinueStmt(){
	m_Parse(String(L"continue",8));
	f__block->m_AddStmt((new bb_stmt_ContinueStmt)->g_new());
	return 0;
}
int bb_parser_Parser::m_PopBlock(){
	gc_assign(f__block,f__blockStack->m_RemoveLast());
	bb_config__errInfo=f__errStack->m_RemoveLast();
	return 0;
}
int bb_parser_Parser::m_ParseIfStmt(String t_term){
	m_CParse(String(L"if",2));
	bb_expr_Expr* t_expr=m_ParseExpr();
	m_CParse(String(L"then",4));
	bb_decl_BlockDecl* t_thenBlock=(new bb_decl_BlockDecl)->g_new(f__block);
	bb_decl_BlockDecl* t_elseBlock=(new bb_decl_BlockDecl)->g_new(f__block);
	int t_eatTerm=0;
	if(!((t_term).Length()!=0)){
		if(f__toke==String(L"\n",1)){
			t_term=String(L"end",3);
		}else{
			t_term=String(L"\n",1);
		}
		t_eatTerm=1;
	}
	m_PushBlock(t_thenBlock);
	while(f__toke!=t_term){
		String t_=f__toke;
		if(t_==String(L"endif",5)){
			if(t_term==String(L"end",3)){
				break;
			}
			bb_config_Err(String(L"Syntax error - expecting 'End'.",31));
		}else{
			if(t_==String(L"else",4) || t_==String(L"elseif",6)){
				int t_elif=((f__toke==String(L"elseif",6))?1:0);
				m_NextToke();
				if(f__block==t_elseBlock){
					bb_config_Err(String(L"If statement can only have one 'else' block.",44));
				}
				m_PopBlock();
				m_PushBlock(t_elseBlock);
				if(((t_elif)!=0) || f__toke==String(L"if",2)){
					m_ParseIfStmt(t_term);
				}
			}else{
				m_ParseStmt();
			}
		}
	}
	m_PopBlock();
	if((t_eatTerm)!=0){
		m_NextToke();
		if(t_term==String(L"end",3)){
			m_CParse(String(L"if",2));
		}
	}
	bb_stmt_IfStmt* t_stmt=(new bb_stmt_IfStmt)->g_new(t_expr,t_thenBlock,t_elseBlock);
	f__block->m_AddStmt(t_stmt);
	return 0;
}
int bb_parser_Parser::m_ParseWhileStmt(){
	m_Parse(String(L"while",5));
	bb_expr_Expr* t_expr=m_ParseExpr();
	bb_decl_BlockDecl* t_block=(new bb_decl_BlockDecl)->g_new(f__block);
	m_PushBlock(t_block);
	while(!((m_CParse(String(L"wend",4)))!=0)){
		if((m_CParse(String(L"end",3)))!=0){
			m_CParse(String(L"while",5));
			break;
		}
		m_ParseStmt();
	}
	m_PopBlock();
	bb_stmt_WhileStmt* t_stmt=(new bb_stmt_WhileStmt)->g_new(t_expr,t_block);
	f__block->m_AddStmt(t_stmt);
	return 0;
}
int bb_parser_Parser::m_PushErr(){
	f__errStack->m_AddLast5(bb_config__errInfo);
	return 0;
}
int bb_parser_Parser::m_PopErr(){
	bb_config__errInfo=f__errStack->m_RemoveLast();
	return 0;
}
int bb_parser_Parser::m_ParseRepeatStmt(){
	m_Parse(String(L"repeat",6));
	bb_decl_BlockDecl* t_block=(new bb_decl_BlockDecl)->g_new(f__block);
	m_PushBlock(t_block);
	while(f__toke!=String(L"until",5) && f__toke!=String(L"forever",7)){
		m_ParseStmt();
	}
	m_PopBlock();
	bb_expr_Expr* t_expr=0;
	if((m_CParse(String(L"until",5)))!=0){
		m_PushErr();
		t_expr=m_ParseExpr();
		m_PopErr();
	}else{
		m_Parse(String(L"forever",7));
		t_expr=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_boolType),String()));
	}
	bb_stmt_RepeatStmt* t_stmt=(new bb_stmt_RepeatStmt)->g_new(t_block,t_expr);
	f__block->m_AddStmt(t_stmt);
	return 0;
}
int bb_parser_Parser::m_ParseForStmt(){
	m_Parse(String(L"for",3));
	String t_varid=String();
	bb_type_Type* t_varty=0;
	int t_varlocal=0;
	if((m_CParse(String(L"local",5)))!=0){
		t_varlocal=1;
		t_varid=m_ParseIdent();
		if(!((m_CParse(String(L":=",2)))!=0)){
			t_varty=m_ParseDeclType();
			m_Parse(String(L"=",1));
		}
	}else{
		t_varlocal=0;
		t_varid=m_ParseIdent();
		m_Parse(String(L"=",1));
	}
	if((m_CParse(String(L"eachin",6)))!=0){
		bb_expr_Expr* t_expr=m_ParseExpr();
		bb_decl_BlockDecl* t_block=(new bb_decl_BlockDecl)->g_new(f__block);
		m_PushBlock(t_block);
		while(!((m_CParse(String(L"next",4)))!=0)){
			if((m_CParse(String(L"end",3)))!=0){
				m_CParse(String(L"for",3));
				break;
			}
			m_ParseStmt();
		}
		m_PopBlock();
		bb_parser_ForEachinStmt* t_stmt=(new bb_parser_ForEachinStmt)->g_new(t_varid,t_varty,t_varlocal,t_expr,t_block);
		f__block->m_AddStmt(t_stmt);
		return 0;
	}
	bb_expr_Expr* t_from=m_ParseExpr();
	String t_op=String();
	if((m_CParse(String(L"to",2)))!=0){
		t_op=String(L"<=",2);
	}else{
		if((m_CParse(String(L"until",5)))!=0){
			t_op=String(L"<",1);
		}else{
			bb_config_Err(String(L"Expecting 'To' or 'Until'.",26));
		}
	}
	bb_expr_Expr* t_term=m_ParseExpr();
	bb_expr_Expr* t_stp=0;
	if((m_CParse(String(L"step",4)))!=0){
		t_stp=m_ParseExpr();
	}else{
		t_stp=((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_intType),String(L"1",1)));
	}
	bb_stmt_Stmt* t_init=0;
	bb_expr_Expr* t_expr2=0;
	bb_stmt_Stmt* t_incr=0;
	if((t_varlocal)!=0){
		bb_decl_LocalDecl* t_indexVar=(new bb_decl_LocalDecl)->g_new(t_varid,0,t_varty,t_from);
		t_init=((new bb_stmt_DeclStmt)->g_new(t_indexVar));
	}else{
		t_init=((new bb_stmt_AssignStmt)->g_new(String(L"=",1),((new bb_parser_IdentExpr)->g_new(t_varid,0)),t_from));
	}
	t_expr2=((new bb_expr_BinaryCompareExpr)->g_new(t_op,((new bb_parser_IdentExpr)->g_new(t_varid,0)),t_term));
	t_incr=((new bb_stmt_AssignStmt)->g_new(String(L"=",1),((new bb_parser_IdentExpr)->g_new(t_varid,0)),((new bb_expr_BinaryMathExpr)->g_new(String(L"+",1),((new bb_parser_IdentExpr)->g_new(t_varid,0)),t_stp))));
	bb_decl_BlockDecl* t_block2=(new bb_decl_BlockDecl)->g_new(f__block);
	m_PushBlock(t_block2);
	while(!((m_CParse(String(L"next",4)))!=0)){
		if((m_CParse(String(L"end",3)))!=0){
			m_CParse(String(L"for",3));
			break;
		}
		m_ParseStmt();
	}
	m_PopBlock();
	m_NextToke();
	bb_stmt_ForStmt* t_stmt2=(new bb_stmt_ForStmt)->g_new(t_init,t_expr2,t_incr,t_block2);
	f__block->m_AddStmt(t_stmt2);
	return 0;
}
int bb_parser_Parser::m_ParseSelectStmt(){
	m_Parse(String(L"select",6));
	bb_expr_Expr* t_expr=m_ParseExpr();
	bb_decl_BlockDecl* t_block=f__block;
	bb_decl_LocalDecl* t_tmpVar=(new bb_decl_LocalDecl)->g_new(String(),0,0,t_expr);
	bb_expr_VarExpr* t_tmpExpr=(new bb_expr_VarExpr)->g_new(t_tmpVar);
	t_block->m_AddStmt((new bb_stmt_DeclStmt)->g_new(t_tmpVar));
	while(f__toke!=String(L"end",3) && f__toke!=String(L"default",7)){
		m_SetErr();
		String t_=f__toke;
		if(t_==String(L"\n",1)){
			m_NextToke();
		}else{
			if(t_==String(L"case",4)){
				m_NextToke();
				bb_expr_Expr* t_comp=0;
				do{
					t_expr=((new bb_expr_BinaryCompareExpr)->g_new(String(L"=",1),(t_tmpExpr),m_ParseExpr()));
					if((t_comp)!=0){
						t_comp=((new bb_expr_BinaryLogicExpr)->g_new(String(L"or",2),t_comp,t_expr));
					}else{
						t_comp=t_expr;
					}
				}while(!(!((m_CParse(String(L",",1)))!=0)));
				bb_decl_BlockDecl* t_thenBlock=(new bb_decl_BlockDecl)->g_new(f__block);
				bb_decl_BlockDecl* t_elseBlock=(new bb_decl_BlockDecl)->g_new(f__block);
				bb_stmt_IfStmt* t_ifstmt=(new bb_stmt_IfStmt)->g_new(t_comp,t_thenBlock,t_elseBlock);
				t_block->m_AddStmt(t_ifstmt);
				t_block=t_ifstmt->f_thenBlock;
				m_PushBlock(t_block);
				while(f__toke!=String(L"case",4) && f__toke!=String(L"default",7) && f__toke!=String(L"end",3)){
					m_ParseStmt();
				}
				m_PopBlock();
				t_block=t_elseBlock;
			}else{
				bb_config_Err(String(L"Syntax error - expecting 'Case', 'Default' or 'End'.",52));
			}
		}
	}
	if(f__toke==String(L"default",7)){
		m_NextToke();
		m_PushBlock(t_block);
		while(f__toke!=String(L"end",3)){
			m_SetErr();
			String t_2=f__toke;
			if(t_2==String(L"case",4)){
				bb_config_Err(String(L"Case can not appear after default.",34));
			}else{
				if(t_2==String(L"default",7)){
					bb_config_Err(String(L"Select statement can have only one default block.",49));
				}
			}
			m_ParseStmt();
		}
		m_PopBlock();
	}
	m_SetErr();
	m_Parse(String(L"end",3));
	m_CParse(String(L"select",6));
	return 0;
}
int bb_parser_Parser::m_ParseTryStmt(){
	m_Parse(String(L"try",3));
	bb_decl_BlockDecl* t_block=(new bb_decl_BlockDecl)->g_new(f__block);
	bb_stack_Stack6* t_catches=(new bb_stack_Stack6)->g_new();
	m_PushBlock(t_block);
	while(f__toke!=String(L"end",3)){
		if((m_CParse(String(L"catch",5)))!=0){
			String t_id=m_ParseIdent();
			m_Parse(String(L":",1));
			bb_type_Type* t_ty=m_ParseType();
			bb_decl_LocalDecl* t_init=(new bb_decl_LocalDecl)->g_new(t_id,0,t_ty,0);
			bb_decl_BlockDecl* t_block2=(new bb_decl_BlockDecl)->g_new(f__block);
			t_catches->m_Push16((new bb_stmt_CatchStmt)->g_new(t_init,t_block2));
			m_PopBlock();
			m_PushBlock(t_block2);
		}else{
			m_ParseStmt();
		}
	}
	if(!((t_catches->m_Length())!=0)){
		bb_config_Err(String(L"Try block must have at least one catch block",44));
	}
	m_PopBlock();
	m_NextToke();
	m_CParse(String(L"try",3));
	f__block->m_AddStmt((new bb_stmt_TryStmt)->g_new(t_block,t_catches->m_ToArray()));
	return 0;
}
int bb_parser_Parser::m_ParseThrowStmt(){
	m_Parse(String(L"throw",5));
	f__block->m_AddStmt((new bb_stmt_ThrowStmt)->g_new(m_ParseExpr()));
	return 0;
}
int bb_parser_Parser::m_ParseStmt(){
	m_SetErr();
	String t_=f__toke;
	if(t_==String(L";",1) || t_==String(L"\n",1)){
		m_NextToke();
	}else{
		if(t_==String(L"const",5) || t_==String(L"local",5)){
			m_ParseDeclStmts();
		}else{
			if(t_==String(L"return",6)){
				m_ParseReturnStmt();
			}else{
				if(t_==String(L"exit",4)){
					m_ParseExitStmt();
				}else{
					if(t_==String(L"continue",8)){
						m_ParseContinueStmt();
					}else{
						if(t_==String(L"if",2)){
							m_ParseIfStmt(String());
						}else{
							if(t_==String(L"while",5)){
								m_ParseWhileStmt();
							}else{
								if(t_==String(L"repeat",6)){
									m_ParseRepeatStmt();
								}else{
									if(t_==String(L"for",3)){
										m_ParseForStmt();
									}else{
										if(t_==String(L"select",6)){
											m_ParseSelectStmt();
										}else{
											if(t_==String(L"try",3)){
												m_ParseTryStmt();
											}else{
												if(t_==String(L"throw",5)){
													m_ParseThrowStmt();
												}else{
													bb_expr_Expr* t_expr=m_ParsePrimaryExpr(1);
													String t_2=f__toke;
													if(t_2==String(L"=",1) || t_2==String(L"*=",2) || t_2==String(L"/=",2) || t_2==String(L"+=",2) || t_2==String(L"-=",2) || t_2==String(L"&=",2) || t_2==String(L"|=",2) || t_2==String(L"~=",2) || t_2==String(L"mod",3) || t_2==String(L"shl",3) || t_2==String(L"shr",3)){
														if(((dynamic_cast<bb_parser_IdentExpr*>(t_expr))!=0) || ((dynamic_cast<bb_expr_IndexExpr*>(t_expr))!=0)){
															String t_op=f__toke;
															m_NextToke();
															if(!t_op.EndsWith(String(L"=",1))){
																m_Parse(String(L"=",1));
																t_op=t_op+String(L"=",1);
															}
															f__block->m_AddStmt((new bb_stmt_AssignStmt)->g_new(t_op,t_expr,m_ParseExpr()));
														}else{
															bb_config_Err(String(L"Assignment operator '",21)+f__toke+String(L"' cannot be used this way.",26));
														}
														return 0;
													}
													if((dynamic_cast<bb_parser_IdentExpr*>(t_expr))!=0){
														t_expr=((new bb_parser_FuncCallExpr)->g_new(t_expr,m_ParseArgs(1)));
													}else{
														if(((dynamic_cast<bb_parser_FuncCallExpr*>(t_expr))!=0) || ((dynamic_cast<bb_expr_InvokeSuperExpr*>(t_expr))!=0) || ((dynamic_cast<bb_expr_NewObjectExpr*>(t_expr))!=0)){
														}else{
															bb_config_Err(String(L"Expression cannot be used as a statement.",41));
														}
													}
													f__block->m_AddStmt((new bb_stmt_ExprStmt)->g_new(t_expr));
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}
bb_decl_FuncDecl* bb_parser_Parser::m_ParseFuncDecl(int t_attrs){
	m_SetErr();
	if((m_CParse(String(L"method",6)))!=0){
		t_attrs|=1;
	}else{
		if(!((m_CParse(String(L"function",8)))!=0)){
			bb_config_InternalErr(String(L"Internal error",14));
		}
	}
	t_attrs|=f__defattrs;
	String t_id=String();
	bb_type_Type* t_ty=0;
	if((t_attrs&1)!=0){
		if(f__toke==String(L"new",3)){
			if((t_attrs&256)!=0){
				bb_config_Err(String(L"Extern classes cannot have constructors.",40));
			}
			t_id=f__toke;
			m_NextToke();
			t_attrs|=2;
			t_attrs&=-2;
		}else{
			t_id=m_ParseIdent();
			t_ty=m_ParseDeclType();
		}
	}else{
		t_id=m_ParseIdent();
		t_ty=m_ParseDeclType();
	}
	bb_stack_Stack5* t_args=(new bb_stack_Stack5)->g_new();
	m_Parse(String(L"(",1));
	m_SkipEols();
	if(f__toke!=String(L")",1)){
		do{
			String t_id2=m_ParseIdent();
			bb_type_Type* t_ty2=m_ParseDeclType();
			bb_expr_Expr* t_init=0;
			if((m_CParse(String(L"=",1)))!=0){
				t_init=m_ParseExpr();
			}
			t_args->m_Push13((new bb_decl_ArgDecl)->g_new(t_id2,0,t_ty2,t_init));
			if(f__toke==String(L")",1)){
				break;
			}
			m_Parse(String(L",",1));
		}while(!(false));
	}
	m_Parse(String(L")",1));
	do{
		if((m_CParse(String(L"final",5)))!=0){
			if(!((t_attrs&1)!=0)){
				bb_config_Err(String(L"Functions cannot be final.",26));
			}
			if((t_attrs&2048)!=0){
				bb_config_Err(String(L"Duplicate method attribute.",27));
			}
			if((t_attrs&1024)!=0){
				bb_config_Err(String(L"Methods cannot be both final and abstract.",42));
			}
			t_attrs|=2048;
		}else{
			if((m_CParse(String(L"abstract",8)))!=0){
				if(!((t_attrs&1)!=0)){
					bb_config_Err(String(L"Functions cannot be abstract.",29));
				}
				if((t_attrs&1024)!=0){
					bb_config_Err(String(L"Duplicate method attribute.",27));
				}
				if((t_attrs&2048)!=0){
					bb_config_Err(String(L"Methods cannot be both final and abstract.",42));
				}
				t_attrs|=1024;
			}else{
				if((m_CParse(String(L"property",8)))!=0){
					if(!((t_attrs&1)!=0)){
						bb_config_Err(String(L"Functions cannot be properties.",31));
					}
					if((t_attrs&4)!=0){
						bb_config_Err(String(L"Duplicate method attribute.",27));
					}
					t_attrs|=4;
				}else{
					break;
				}
			}
		}
	}while(!(false));
	bb_decl_FuncDecl* t_funcDecl=(new bb_decl_FuncDecl)->g_new(t_id,t_attrs,t_ty,t_args->m_ToArray());
	if(((t_funcDecl->m_IsExtern())!=0) || ((m_CParse(String(L"extern",6)))!=0)){
		t_funcDecl->f_munged=t_funcDecl->f_ident;
		if((m_CParse(String(L"=",1)))!=0){
			t_funcDecl->f_munged=m_ParseStringLit();
			if(t_funcDecl->f_munged==String(L"$resize",7)){
				gc_assign(t_funcDecl->f_retType,(bb_type_Type::g_emptyArrayType));
			}
		}
	}
	if(((t_funcDecl->m_IsExtern())!=0) || ((t_funcDecl->m_IsAbstract())!=0)){
		return t_funcDecl;
	}
	m_PushBlock(t_funcDecl);
	while(f__toke!=String(L"end",3)){
		m_ParseStmt();
	}
	m_PopBlock();
	m_NextToke();
	if((t_attrs&3)!=0){
		m_CParse(String(L"method",6));
	}else{
		m_CParse(String(L"function",8));
	}
	return t_funcDecl;
}
bb_decl_ClassDecl* bb_parser_Parser::m_ParseClassDecl(int t_attrs){
	m_SetErr();
	String t_toke=f__toke;
	if((m_CParse(String(L"interface",9)))!=0){
		if((t_attrs&256)!=0){
			bb_config_Err(String(L"Interfaces cannot be extern.",28));
		}
		t_attrs|=5120;
	}else{
		if(!((m_CParse(String(L"class",5)))!=0)){
			bb_config_InternalErr(String(L"Internal error",14));
		}
	}
	String t_id=m_ParseIdent();
	bb_stack_StringStack* t_args=(new bb_stack_StringStack)->g_new();
	bb_type_IdentType* t_superTy=bb_type_Type::g_objectType;
	bb_stack_Stack4* t_imps=(new bb_stack_Stack4)->g_new();
	if((m_CParse(String(L"<",1)))!=0){
		if((t_attrs&256)!=0){
			bb_config_Err(String(L"Extern classes cannot be generic.",33));
		}
		if((t_attrs&4096)!=0){
			bb_config_Err(String(L"Interfaces cannot be generic.",29));
		}
		do{
			t_args->m_Push(m_ParseIdent());
		}while(!(!((m_CParse(String(L",",1)))!=0)));
		m_Parse(String(L">",1));
	}
	if((m_CParse(String(L"extends",7)))!=0){
		if((m_CParse(String(L"null",4)))!=0){
			if((t_attrs&4096)!=0){
				bb_config_Err(String(L"Interfaces cannot extend null",29));
			}
			if(!((t_attrs&256)!=0)){
				bb_config_Err(String(L"Only extern objects can extend null.",36));
			}
			t_superTy=0;
		}else{
			if((t_attrs&4096)!=0){
				do{
					t_imps->m_Push10(m_ParseIdentType());
				}while(!(!((m_CParse(String(L",",1)))!=0)));
				t_superTy=bb_type_Type::g_objectType;
			}else{
				t_superTy=m_ParseIdentType();
			}
		}
	}
	if((m_CParse(String(L"implements",10)))!=0){
		if((t_attrs&256)!=0){
			bb_config_Err(String(L"Implements cannot be used with external classes.",48));
		}
		if((t_attrs&4096)!=0){
			bb_config_Err(String(L"Implements cannot be used with interfaces.",42));
		}
		do{
			t_imps->m_Push10(m_ParseIdentType());
		}while(!(!((m_CParse(String(L",",1)))!=0)));
	}
	do{
		if((m_CParse(String(L"final",5)))!=0){
			if((t_attrs&4096)!=0){
				bb_config_Err(String(L"Interfaces cannot be final.",27));
			}
			if((t_attrs&2048)!=0){
				bb_config_Err(String(L"Duplicate class attribute.",26));
			}
			if((t_attrs&1024)!=0){
				bb_config_Err(String(L"Classes cannot be both final and abstract.",42));
			}
			t_attrs|=2048;
		}else{
			if((m_CParse(String(L"abstract",8)))!=0){
				if((t_attrs&4096)!=0){
					bb_config_Err(String(L"Interfaces cannot be abstract.",30));
				}
				if((t_attrs&1024)!=0){
					bb_config_Err(String(L"Duplicate class attribute.",26));
				}
				if((t_attrs&2048)!=0){
					bb_config_Err(String(L"Classes cannot be both final and abstract.",42));
				}
				t_attrs|=1024;
			}else{
				break;
			}
		}
	}while(!(false));
	bb_decl_ClassDecl* t_classDecl=(new bb_decl_ClassDecl)->g_new(t_id,t_attrs,t_args->m_ToArray(),t_superTy,t_imps->m_ToArray());
	if(((t_classDecl->m_IsExtern())!=0) || ((m_CParse(String(L"extern",6)))!=0)){
		t_classDecl->f_munged=t_classDecl->f_ident;
		if((m_CParse(String(L"=",1)))!=0){
			t_classDecl->f_munged=m_ParseStringLit();
		}
	}
	int t_decl_attrs=t_attrs&256;
	int t_func_attrs=t_decl_attrs;
	if((t_attrs&4096)!=0){
		t_func_attrs|=1024;
	}
	do{
		m_SkipEols();
		String t_=f__toke;
		if(t_==String(L"end",3)){
			m_NextToke();
			break;
		}else{
			if(t_==String(L"private",7)){
				m_NextToke();
				t_decl_attrs=t_decl_attrs|512;
			}else{
				if(t_==String(L"public",6)){
					m_NextToke();
					t_decl_attrs=t_decl_attrs&-513;
				}else{
					if(t_==String(L"const",5) || t_==String(L"global",6) || t_==String(L"field",5)){
						if(((t_attrs&4096)!=0) && f__toke!=String(L"const",5)){
							bb_config_Err(String(L"Interfaces can only contain constants and methods.",50));
						}
						t_classDecl->m_InsertDecls(m_ParseDecls(f__toke,t_decl_attrs));
					}else{
						if(t_==String(L"method",6)){
							t_classDecl->m_InsertDecl(m_ParseFuncDecl(t_func_attrs));
						}else{
							if(t_==String(L"function",8)){
								if((t_attrs&4096)!=0){
									bb_config_Err(String(L"Interfaces can only contain constants and methods.",50));
								}
								t_classDecl->m_InsertDecl(m_ParseFuncDecl(t_func_attrs));
							}else{
								bb_config_Err(String(L"Syntax error - expecting class member declaration.",50));
							}
						}
					}
				}
			}
		}
	}while(!(false));
	if((t_toke).Length()!=0){
		m_CParse(t_toke);
	}
	return t_classDecl;
}
int bb_parser_Parser::m_ParseMain(){
	m_SkipEols();
	if(!((f__module)!=0)){
		int t_attrs=0;
		if((m_CParse(String(L"strict",6)))!=0){
			t_attrs|=1;
		}
		String t_path=f__toker->m_Path();
		String t_ident=bb_os_StripAll(t_path);
		String t_munged=String();
		m_ValidateModIdent(t_ident);
		gc_assign(f__module,(new bb_decl_ModuleDecl)->g_new(t_ident,t_attrs,t_munged,t_path));
		f__module->f_imported->m_Insert3(t_path,f__module);
		f__app->m_InsertModule(f__module);
		m_ImportModule(String(L"monkey.lang",11),0);
		m_ImportModule(String(L"monkey",6),0);
	}
	int t_attrs2=0;
	while((f__toke).Length()!=0){
		m_SetErr();
		String t_=f__toke;
		if(t_==String(L"\n",1)){
			m_NextToke();
		}else{
			if(t_==String(L"public",6)){
				m_NextToke();
				t_attrs2=0;
			}else{
				if(t_==String(L"private",7)){
					m_NextToke();
					t_attrs2=512;
				}else{
					if(t_==String(L"import",6)){
						m_NextToke();
						if(f__tokeType==6){
							m_ImportFile(bb_config_EvalCfgTags(m_ParseStringLit()));
						}else{
							m_ImportModule(m_ParseModPath(),t_attrs2);
						}
					}else{
						if(t_==String(L"alias",5)){
							m_NextToke();
							do{
								String t_ident2=m_ParseIdent();
								m_Parse(String(L"=",1));
								Object* t_decl=0;
								String t_2=f__toke;
								if(t_2==String(L"int",3)){
									t_decl=(bb_type_Type::g_intType);
								}else{
									if(t_2==String(L"float",5)){
										t_decl=(bb_type_Type::g_floatType);
									}else{
										if(t_2==String(L"string",6)){
											t_decl=(bb_type_Type::g_stringType);
										}
									}
								}
								if((t_decl)!=0){
									f__module->m_InsertDecl((new bb_decl_AliasDecl)->g_new(t_ident2,t_attrs2,t_decl));
									m_NextToke();
									continue;
								}
								bb_decl_ScopeDecl* t_scope=(f__module);
								gc_assign(bb_decl__env,(f__module));
								do{
									String t_id=m_ParseIdent();
									t_decl=t_scope->m_FindDecl(t_id);
									if(!((t_decl)!=0)){
										bb_config_Err(String(L"Identifier '",12)+t_id+String(L"' not found.",12));
									}
									if(!((m_CParse(String(L".",1)))!=0)){
										break;
									}
									t_scope=dynamic_cast<bb_decl_ScopeDecl*>(t_decl);
									if(!((t_scope)!=0) || ((dynamic_cast<bb_decl_FuncDecl*>(t_scope))!=0)){
										bb_config_Err(String(L"Invalid scope '",15)+t_id+String(L"'.",2));
									}
								}while(!(false));
								bb_decl__env=0;
								f__module->m_InsertDecl((new bb_decl_AliasDecl)->g_new(t_ident2,t_attrs2,t_decl));
							}while(!(!((m_CParse(String(L",",1)))!=0)));
						}else{
							break;
						}
					}
				}
			}
		}
	}
	while((f__toke).Length()!=0){
		m_SetErr();
		String t_3=f__toke;
		if(t_3==String(L"\n",1)){
			m_NextToke();
		}else{
			if(t_3==String(L"public",6)){
				m_NextToke();
				t_attrs2=0;
			}else{
				if(t_3==String(L"private",7)){
					m_NextToke();
					t_attrs2=512;
				}else{
					if(t_3==String(L"extern",6)){
						if((bb_config_ENV_SAFEMODE)!=0){
							if(f__app->f_mainModule==f__module){
								bb_config_Err(String(L"Extern not permitted in safe mode.",34));
							}
						}
						m_NextToke();
						t_attrs2=256;
						if((m_CParse(String(L"private",7)))!=0){
							t_attrs2|=512;
						}
					}else{
						if(t_3==String(L"const",5) || t_3==String(L"global",6)){
							f__module->m_InsertDecls(m_ParseDecls(f__toke,t_attrs2));
						}else{
							if(t_3==String(L"class",5)){
								f__module->m_InsertDecl(m_ParseClassDecl(t_attrs2));
							}else{
								if(t_3==String(L"interface",9)){
									f__module->m_InsertDecl(m_ParseClassDecl(t_attrs2));
								}else{
									if(t_3==String(L"function",8)){
										f__module->m_InsertDecl(m_ParseFuncDecl(t_attrs2));
									}else{
										bb_config_Err(String(L"Syntax error - expecting declaration.",37));
									}
								}
							}
						}
					}
				}
			}
		}
	}
	bb_config__errInfo=String();
	return 0;
}
void bb_parser_Parser::mark(){
	Object::mark();
	gc_mark_q(f__toker);
	gc_mark_q(f__app);
	gc_mark_q(f__module);
	gc_mark_q(f__block);
	gc_mark_q(f__blockStack);
	gc_mark_q(f__errStack);
}
bb_decl_ModuleDecl::bb_decl_ModuleDecl(){
	f_filepath=String();
	f_imported=(new bb_map_StringMap3)->g_new();
	f_pubImported=(new bb_map_StringMap3)->g_new();
}
bb_decl_ModuleDecl* bb_decl_ModuleDecl::g_new(String t_ident,int t_attrs,String t_munged,String t_filepath){
	bb_decl_ScopeDecl::g_new();
	this->f_ident=t_ident;
	this->f_attrs=t_attrs;
	this->f_munged=t_munged;
	this->f_filepath=t_filepath;
	return this;
}
bb_decl_ModuleDecl* bb_decl_ModuleDecl::g_new2(){
	bb_decl_ScopeDecl::g_new();
	return this;
}
int bb_decl_ModuleDecl::m_IsStrict(){
	return (((f_attrs&1)!=0)?1:0);
}
int bb_decl_ModuleDecl::m_SemantAll(){
	bb_list_Enumerator2* t_=m_Decls()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		if((dynamic_cast<bb_decl_AliasDecl*>(t_decl))!=0){
			continue;
		}
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl);
		if((t_cdecl)!=0){
			if((t_cdecl->f_args).Length()!=0){
				bb_list_Enumerator3* t_2=t_cdecl->f_instances->m_ObjectEnumerator();
				while(t_2->m_HasNext()){
					bb_decl_ClassDecl* t_inst=t_2->m_NextObject();
					bb_list_Enumerator2* t_3=t_inst->m_Decls()->m_ObjectEnumerator();
					while(t_3->m_HasNext()){
						bb_decl_Decl* t_decl2=t_3->m_NextObject();
						if((dynamic_cast<bb_decl_AliasDecl*>(t_decl2))!=0){
							continue;
						}
						t_decl2->m_Semant();
					}
				}
			}else{
				t_decl->m_Semant();
				bb_list_Enumerator2* t_4=t_cdecl->m_Decls()->m_ObjectEnumerator();
				while(t_4->m_HasNext()){
					bb_decl_Decl* t_decl3=t_4->m_NextObject();
					if((dynamic_cast<bb_decl_AliasDecl*>(t_decl3))!=0){
						continue;
					}
					t_decl3->m_Semant();
				}
			}
		}else{
			t_decl->m_Semant();
		}
	}
	f_attrs|=2;
	return 0;
}
String bb_decl_ModuleDecl::m_ToString(){
	return String(L"Module ",7)+f_munged;
}
Object* bb_decl_ModuleDecl::m_GetDecl2(String t_ident){
	return bb_decl_ScopeDecl::m_GetDecl(t_ident);
}
Object* bb_decl_ModuleDecl::m_GetDecl(String t_ident){
	bb_list_List9* t_todo=(new bb_list_List9)->g_new();
	bb_map_StringMap3* t_done=(new bb_map_StringMap3)->g_new();
	t_todo->m_AddLast9(this);
	t_done->m_Insert3(f_filepath,this);
	Object* t_decl=0;
	String t_declmod=String();
	while(!t_todo->m_IsEmpty()){
		bb_decl_ModuleDecl* t_mdecl=t_todo->m_RemoveLast();
		Object* t_tdecl=t_mdecl->m_GetDecl2(t_ident);
		if(((t_tdecl)!=0) && t_tdecl!=t_decl){
			if(t_mdecl==this){
				return t_tdecl;
			}
			if((t_decl)!=0){
				bb_config_Err(String(L"Duplicate identifier '",22)+t_ident+String(L"' found in module '",19)+t_declmod+String(L"' and module '",14)+t_mdecl->f_ident+String(L"'.",2));
			}
			t_decl=t_tdecl;
			t_declmod=t_mdecl->f_ident;
		}
		if(!((bb_decl__env)!=0)){
			break;
		}
		bb_map_StringMap3* t_imps=t_mdecl->f_imported;
		if(t_mdecl!=bb_decl__env->m_ModuleScope()){
			t_imps=t_mdecl->f_pubImported;
		}
		bb_map_ValueEnumerator* t_=t_imps->m_Values()->m_ObjectEnumerator();
		while(t_->m_HasNext()){
			bb_decl_ModuleDecl* t_mdecl2=t_->m_NextObject();
			if(!t_done->m_Contains(t_mdecl2->f_filepath)){
				t_todo->m_AddLast9(t_mdecl2);
				t_done->m_Insert3(t_mdecl2->f_filepath,t_mdecl2);
			}
		}
	}
	return t_decl;
}
int bb_decl_ModuleDecl::m_OnSemant(){
	return 0;
}
void bb_decl_ModuleDecl::mark(){
	bb_decl_ScopeDecl::mark();
	gc_mark_q(f_imported);
	gc_mark_q(f_pubImported);
}
bb_expr_UnaryExpr::bb_expr_UnaryExpr(){
	f_op=String();
	f_expr=0;
}
bb_expr_UnaryExpr* bb_expr_UnaryExpr::g_new(String t_op,bb_expr_Expr* t_expr){
	bb_expr_Expr::g_new();
	this->f_op=t_op;
	gc_assign(this->f_expr,t_expr);
	return this;
}
bb_expr_UnaryExpr* bb_expr_UnaryExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_UnaryExpr::m_Copy(){
	return ((new bb_expr_UnaryExpr)->g_new(f_op,m_CopyExpr(f_expr)));
}
bb_expr_Expr* bb_expr_UnaryExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	String t_=f_op;
	if(t_==String(L"+",1) || t_==String(L"-",1)){
		gc_assign(f_expr,f_expr->m_Semant());
		if(!((dynamic_cast<bb_type_NumericType*>(f_expr->f_exprType))!=0)){
			bb_config_Err(f_expr->m_ToString()+String(L" must be numeric for use with unary operator '",46)+f_op+String(L"'",1));
		}
		gc_assign(f_exprType,f_expr->f_exprType);
	}else{
		if(t_==String(L"~",1)){
			gc_assign(f_expr,f_expr->m_Semant2((bb_type_Type::g_intType),0));
			gc_assign(f_exprType,(bb_type_Type::g_intType));
		}else{
			if(t_==String(L"not",3)){
				gc_assign(f_expr,f_expr->m_Semant2((bb_type_Type::g_boolType),1));
				gc_assign(f_exprType,(bb_type_Type::g_boolType));
			}else{
				bb_config_InternalErr(String(L"Internal error",14));
			}
		}
	}
	if((dynamic_cast<bb_expr_ConstExpr*>(f_expr))!=0){
		return m_EvalConst();
	}
	return (this);
}
String bb_expr_UnaryExpr::m_Eval(){
	String t_val=f_expr->m_Eval();
	String t_=f_op;
	if(t_==String(L"~",1)){
		return String(~(t_val).ToInt());
	}else{
		if(t_==String(L"+",1)){
			return t_val;
		}else{
			if(t_==String(L"-",1)){
				if(t_val.StartsWith(String(L"-",1))){
					return t_val.Slice(1);
				}
				return String(L"-",1)+t_val;
			}else{
				if(t_==String(L"not",3)){
					if((t_val).Length()!=0){
						return String();
					}
					return String(L"1",1);
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_expr_UnaryExpr::m_Trans(){
	return bb_translator__trans->m_TransUnaryExpr(this);
}
void bb_expr_UnaryExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_expr);
}
bb_expr_ArrayExpr::bb_expr_ArrayExpr(){
	f_exprs=Array<bb_expr_Expr* >();
}
bb_expr_ArrayExpr* bb_expr_ArrayExpr::g_new(Array<bb_expr_Expr* > t_exprs){
	bb_expr_Expr::g_new();
	gc_assign(this->f_exprs,t_exprs);
	return this;
}
bb_expr_ArrayExpr* bb_expr_ArrayExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_ArrayExpr::m_Copy(){
	return ((new bb_expr_ArrayExpr)->g_new(m_CopyArgs(f_exprs)));
}
bb_expr_Expr* bb_expr_ArrayExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_exprs[0],f_exprs[0]->m_Semant());
	bb_type_Type* t_ty=f_exprs[0]->f_exprType;
	for(int t_i=1;t_i<f_exprs.Length();t_i=t_i+1){
		gc_assign(f_exprs[t_i],f_exprs[t_i]->m_Semant());
		t_ty=m_BalanceTypes(t_ty,f_exprs[t_i]->f_exprType);
	}
	for(int t_i2=0;t_i2<f_exprs.Length();t_i2=t_i2+1){
		gc_assign(f_exprs[t_i2],f_exprs[t_i2]->m_Cast(t_ty,0));
	}
	gc_assign(f_exprType,(t_ty->m_ArrayOf()));
	return (this);
}
String bb_expr_ArrayExpr::m_Trans(){
	return bb_translator__trans->m_TransArrayExpr(this);
}
void bb_expr_ArrayExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_exprs);
}
bb_stack_Stack2::bb_stack_Stack2(){
	f_data=Array<bb_expr_Expr* >();
	f_length=0;
}
bb_stack_Stack2* bb_stack_Stack2::g_new(){
	return this;
}
bb_stack_Stack2* bb_stack_Stack2::g_new2(Array<bb_expr_Expr* > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack2::m_Push4(bb_expr_Expr* t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	gc_assign(f_data[f_length],t_value);
	f_length+=1;
	return 0;
}
int bb_stack_Stack2::m_Push5(Array<bb_expr_Expr* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push4(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack2::m_Push6(Array<bb_expr_Expr* > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push4(t_values[t_i]);
	}
	return 0;
}
Array<bb_expr_Expr* > bb_stack_Stack2::m_ToArray(){
	Array<bb_expr_Expr* > t_t=Array<bb_expr_Expr* >(f_length);
	for(int t_i=0;t_i<f_length;t_i=t_i+1){
		gc_assign(t_t[t_i],f_data[t_i]);
	}
	return t_t;
}
void bb_stack_Stack2::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
bb_type_ArrayType::bb_type_ArrayType(){
	f_elemType=0;
}
bb_type_ArrayType* bb_type_ArrayType::g_new(bb_type_Type* t_elemType){
	bb_type_Type::g_new();
	gc_assign(this->f_elemType,t_elemType);
	return this;
}
bb_type_ArrayType* bb_type_ArrayType::g_new2(){
	bb_type_Type::g_new();
	return this;
}
int bb_type_ArrayType::m_EqualsType(bb_type_Type* t_ty){
	bb_type_ArrayType* t_arrayType=dynamic_cast<bb_type_ArrayType*>(t_ty);
	return ((((t_arrayType)!=0) && ((f_elemType->m_EqualsType(t_arrayType->f_elemType))!=0))?1:0);
}
int bb_type_ArrayType::m_ExtendsType(bb_type_Type* t_ty){
	bb_type_ArrayType* t_arrayType=dynamic_cast<bb_type_ArrayType*>(t_ty);
	return ((((t_arrayType)!=0) && (((dynamic_cast<bb_type_VoidType*>(f_elemType))!=0) || ((f_elemType->m_EqualsType(t_arrayType->f_elemType))!=0)))?1:0);
}
bb_type_Type* bb_type_ArrayType::m_Semant(){
	bb_type_Type* t_ty=f_elemType->m_Semant();
	if(t_ty!=f_elemType){
		return ((new bb_type_ArrayType)->g_new(t_ty));
	}
	return (this);
}
bb_decl_ClassDecl* bb_type_ArrayType::m_GetClass(){
	return dynamic_cast<bb_decl_ClassDecl*>(bb_decl__env->m_FindDecl(String(L"array",5)));
}
String bb_type_ArrayType::m_ToString(){
	return f_elemType->m_ToString()+String(L"[]",2);
}
void bb_type_ArrayType::mark(){
	bb_type_Type::mark();
	gc_mark_q(f_elemType);
}
bb_type_VoidType::bb_type_VoidType(){
}
bb_type_VoidType* bb_type_VoidType::g_new(){
	bb_type_Type::g_new();
	return this;
}
int bb_type_VoidType::m_EqualsType(bb_type_Type* t_ty){
	return ((dynamic_cast<bb_type_VoidType*>(t_ty)!=0)?1:0);
}
String bb_type_VoidType::m_ToString(){
	return String(L"Void",4);
}
void bb_type_VoidType::mark(){
	bb_type_Type::mark();
}
bb_parser_ScopeExpr::bb_parser_ScopeExpr(){
	f_scope=0;
}
bb_parser_ScopeExpr* bb_parser_ScopeExpr::g_new(bb_decl_ScopeDecl* t_scope){
	bb_expr_Expr::g_new();
	gc_assign(this->f_scope,t_scope);
	return this;
}
bb_parser_ScopeExpr* bb_parser_ScopeExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_parser_ScopeExpr::m_Copy(){
	return (this);
}
String bb_parser_ScopeExpr::m_ToString(){
	Print(String(L"ScopeExpr(",10)+f_scope->m_ToString()+String(L")",1));
	return String();
}
bb_expr_Expr* bb_parser_ScopeExpr::m_Semant(){
	bb_config_InternalErr(String(L"Internal error",14));
	return 0;
}
bb_decl_ScopeDecl* bb_parser_ScopeExpr::m_SemantScope(){
	return f_scope;
}
void bb_parser_ScopeExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_scope);
}
bb_type_IdentType::bb_type_IdentType(){
	f_ident=String();
	f_args=Array<bb_type_Type* >();
}
bb_type_IdentType* bb_type_IdentType::g_new(String t_ident,Array<bb_type_Type* > t_args){
	bb_type_Type::g_new();
	this->f_ident=t_ident;
	gc_assign(this->f_args,t_args);
	return this;
}
bb_type_IdentType* bb_type_IdentType::g_new2(){
	bb_type_Type::g_new();
	return this;
}
int bb_type_IdentType::m_EqualsType(bb_type_Type* t_ty){
	bb_config_InternalErr(String(L"Internal error",14));
	return 0;
}
int bb_type_IdentType::m_ExtendsType(bb_type_Type* t_ty){
	bb_config_InternalErr(String(L"Internal error",14));
	return 0;
}
bb_type_Type* bb_type_IdentType::m_Semant(){
	if(!((f_ident).Length()!=0)){
		return (bb_decl_ClassDecl::g_nullObjectClass->f_objectType);
	}
	Array<bb_type_Type* > t_targs=Array<bb_type_Type* >(f_args.Length());
	for(int t_i=0;t_i<f_args.Length();t_i=t_i+1){
		gc_assign(t_targs[t_i],f_args[t_i]->m_Semant());
	}
	String t_tyid=String();
	bb_type_Type* t_type=0;
	int t_i2=f_ident.Find(String(L".",1),0);
	if(t_i2==-1){
		t_tyid=f_ident;
		t_type=bb_decl__env->m_FindType(t_tyid,t_targs);
	}else{
		String t_modid=f_ident.Slice(0,t_i2);
		bb_decl_ModuleDecl* t_mdecl=bb_decl__env->m_FindModuleDecl(t_modid);
		if(!((t_mdecl)!=0)){
			bb_config_Err(String(L"Module '",8)+t_modid+String(L"' not found",11));
		}
		t_tyid=f_ident.Slice(t_i2+1);
		t_type=t_mdecl->m_FindType(t_tyid,t_targs);
	}
	if(!((t_type)!=0)){
		bb_config_Err(String(L"Type '",6)+t_tyid+String(L"' not found",11));
	}
	return t_type;
}
String bb_type_IdentType::m_ToString(){
	String t_t=String();
	Array<bb_type_Type* > t_=f_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_type_Type* t_arg=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_arg->m_ToString();
	}
	if((t_t).Length()!=0){
		return String(L"$",1)+f_ident+String(L"<",1)+t_t.Replace(String(L"$",1),String())+String(L">",1);
	}
	return String(L"$",1)+f_ident;
}
bb_decl_ClassDecl* bb_type_IdentType::m_SemantClass(){
	bb_type_ObjectType* t_type=dynamic_cast<bb_type_ObjectType*>(m_Semant());
	if(!((t_type)!=0)){
		bb_config_Err(String(L"Type is not a class",19));
	}
	return t_type->f_classDecl;
}
void bb_type_IdentType::mark(){
	bb_type_Type::mark();
	gc_mark_q(f_args);
}
bb_stack_Stack3::bb_stack_Stack3(){
	f_data=Array<bb_type_Type* >();
	f_length=0;
}
bb_stack_Stack3* bb_stack_Stack3::g_new(){
	return this;
}
bb_stack_Stack3* bb_stack_Stack3::g_new2(Array<bb_type_Type* > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack3::m_Push7(bb_type_Type* t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	gc_assign(f_data[f_length],t_value);
	f_length+=1;
	return 0;
}
int bb_stack_Stack3::m_Push8(Array<bb_type_Type* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push7(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack3::m_Push9(Array<bb_type_Type* > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push7(t_values[t_i]);
	}
	return 0;
}
Array<bb_type_Type* > bb_stack_Stack3::m_ToArray(){
	Array<bb_type_Type* > t_t=Array<bb_type_Type* >(f_length);
	for(int t_i=0;t_i<f_length;t_i=t_i+1){
		gc_assign(t_t[t_i],f_data[t_i]);
	}
	return t_t;
}
void bb_stack_Stack3::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
bb_expr_NewArrayExpr::bb_expr_NewArrayExpr(){
	f_ty=0;
	f_expr=0;
}
bb_expr_NewArrayExpr* bb_expr_NewArrayExpr::g_new(bb_type_Type* t_ty,bb_expr_Expr* t_expr){
	bb_expr_Expr::g_new();
	gc_assign(this->f_ty,t_ty);
	gc_assign(this->f_expr,t_expr);
	return this;
}
bb_expr_NewArrayExpr* bb_expr_NewArrayExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_NewArrayExpr::m_Copy(){
	if((f_exprType)!=0){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	return ((new bb_expr_NewArrayExpr)->g_new(f_ty,m_CopyExpr(f_expr)));
}
bb_expr_Expr* bb_expr_NewArrayExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_ty,f_ty->m_Semant());
	gc_assign(f_exprType,(f_ty->m_ArrayOf()));
	gc_assign(f_expr,f_expr->m_Semant2((bb_type_Type::g_intType),0));
	return (this);
}
String bb_expr_NewArrayExpr::m_Trans(){
	return bb_translator__trans->m_TransNewArrayExpr(this);
}
void bb_expr_NewArrayExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_ty);
	gc_mark_q(f_expr);
}
bb_expr_NewObjectExpr::bb_expr_NewObjectExpr(){
	f_ty=0;
	f_args=Array<bb_expr_Expr* >();
	f_classDecl=0;
	f_ctor=0;
}
bb_expr_NewObjectExpr* bb_expr_NewObjectExpr::g_new(bb_type_Type* t_ty,Array<bb_expr_Expr* > t_args){
	bb_expr_Expr::g_new();
	gc_assign(this->f_ty,t_ty);
	gc_assign(this->f_args,t_args);
	return this;
}
bb_expr_NewObjectExpr* bb_expr_NewObjectExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_NewObjectExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_ty,f_ty->m_Semant());
	gc_assign(f_args,m_SemantArgs(f_args));
	bb_type_ObjectType* t_objTy=dynamic_cast<bb_type_ObjectType*>(f_ty);
	if(!((t_objTy)!=0)){
		bb_config_Err(String(L"Expression is not a class.",26));
	}
	gc_assign(f_classDecl,t_objTy->f_classDecl);
	if((f_classDecl->m_IsInterface())!=0){
		bb_config_Err(String(L"Cannot create instance of an interface.",39));
	}
	if((f_classDecl->m_IsAbstract())!=0){
		bb_config_Err(String(L"Cannot create instance of an abstract class.",44));
	}
	if(((f_classDecl->f_args).Length()!=0) && !((f_classDecl->f_instanceof)!=0)){
		bb_config_Err(String(L"Cannot create instance of a generic class.",42));
	}
	if((f_classDecl->m_IsExtern())!=0){
		if((f_args).Length()!=0){
			bb_config_Err(String(L"No suitable constructor found for class ",40)+f_classDecl->m_ToString()+String(L".",1));
		}
	}else{
		gc_assign(f_ctor,f_classDecl->m_FindFuncDecl(String(L"new",3),f_args,0));
		if(!((f_ctor)!=0)){
			bb_config_Err(String(L"No suitable constructor found for class ",40)+f_classDecl->m_ToString()+String(L".",1));
		}
		gc_assign(f_args,m_CastArgs(f_args,f_ctor));
	}
	f_classDecl->f_attrs|=1;
	gc_assign(f_exprType,f_ty);
	return (this);
}
bb_expr_Expr* bb_expr_NewObjectExpr::m_Copy(){
	return ((new bb_expr_NewObjectExpr)->g_new(f_ty,m_CopyArgs(f_args)));
}
String bb_expr_NewObjectExpr::m_Trans(){
	return bb_translator__trans->m_TransNewObjectExpr(this);
}
void bb_expr_NewObjectExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_ty);
	gc_mark_q(f_args);
	gc_mark_q(f_classDecl);
	gc_mark_q(f_ctor);
}
bb_expr_CastExpr::bb_expr_CastExpr(){
	f_ty=0;
	f_expr=0;
	f_flags=0;
}
bb_expr_CastExpr* bb_expr_CastExpr::g_new(bb_type_Type* t_ty,bb_expr_Expr* t_expr,int t_flags){
	bb_expr_Expr::g_new();
	gc_assign(this->f_ty,t_ty);
	gc_assign(this->f_expr,t_expr);
	this->f_flags=t_flags;
	return this;
}
bb_expr_CastExpr* bb_expr_CastExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_CastExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_ty,f_ty->m_Semant());
	gc_assign(f_expr,f_expr->m_Semant());
	bb_type_Type* t_src=f_expr->f_exprType;
	if((t_src->m_EqualsType(f_ty))!=0){
		return f_expr;
	}
	if((t_src->m_ExtendsType(f_ty))!=0){
		if(((dynamic_cast<bb_type_ArrayType*>(t_src))!=0) && ((dynamic_cast<bb_type_VoidType*>(dynamic_cast<bb_type_ArrayType*>(t_src)->f_elemType))!=0)){
			return ((new bb_expr_ConstExpr)->g_new(f_ty,String()))->m_Semant();
		}
		if(((dynamic_cast<bb_type_ObjectType*>(f_ty))!=0) && !((dynamic_cast<bb_type_ObjectType*>(t_src))!=0)){
			bb_expr_Expr* t_[]={f_expr};
			gc_assign(f_expr,((new bb_expr_NewObjectExpr)->g_new(f_ty,Array<bb_expr_Expr* >(t_,1)))->m_Semant());
		}else{
			if(((dynamic_cast<bb_type_ObjectType*>(t_src))!=0) && !((dynamic_cast<bb_type_ObjectType*>(f_ty))!=0)){
				String t_op=String();
				if((dynamic_cast<bb_type_BoolType*>(f_ty))!=0){
					t_op=String(L"ToBool",6);
				}else{
					if((dynamic_cast<bb_type_IntType*>(f_ty))!=0){
						t_op=String(L"ToInt",5);
					}else{
						if((dynamic_cast<bb_type_FloatType*>(f_ty))!=0){
							t_op=String(L"ToFloat",7);
						}else{
							if((dynamic_cast<bb_type_StringType*>(f_ty))!=0){
								t_op=String(L"ToString",8);
							}else{
								bb_config_InternalErr(String(L"Internal error",14));
							}
						}
					}
				}
				bb_decl_FuncDecl* t_fdecl=t_src->m_GetClass()->m_FindFuncDecl(t_op,Array<bb_expr_Expr* >(),0);
				gc_assign(f_expr,((new bb_expr_InvokeMemberExpr)->g_new(f_expr,t_fdecl,Array<bb_expr_Expr* >()))->m_Semant());
			}
		}
		gc_assign(f_exprType,f_ty);
	}else{
		if((dynamic_cast<bb_type_BoolType*>(f_ty))!=0){
			if((dynamic_cast<bb_type_VoidType*>(t_src))!=0){
				bb_config_Err(String(L"Cannot convert from Void to Bool.",33));
			}
			if((f_flags&1)!=0){
				gc_assign(f_exprType,f_ty);
			}
		}else{
			if((f_ty->m_ExtendsType(t_src))!=0){
				if((f_flags&1)!=0){
					if(dynamic_cast<bb_type_ObjectType*>(f_ty)!=0==(dynamic_cast<bb_type_ObjectType*>(t_src)!=0)){
						gc_assign(f_exprType,f_ty);
					}
				}
			}else{
				if(((dynamic_cast<bb_type_ObjectType*>(f_ty))!=0) && ((dynamic_cast<bb_type_ObjectType*>(t_src))!=0)){
					if((f_flags&1)!=0){
						if(((t_src->m_GetClass()->m_IsInterface())!=0) || ((f_ty->m_GetClass()->m_IsInterface())!=0)){
							gc_assign(f_exprType,f_ty);
						}
					}
				}
			}
		}
	}
	if(!((f_exprType)!=0)){
		bb_config_Err(String(L"Cannot convert from ",20)+t_src->m_ToString()+String(L" to ",4)+f_ty->m_ToString()+String(L".",1));
	}
	if((dynamic_cast<bb_expr_ConstExpr*>(f_expr))!=0){
		return m_EvalConst();
	}
	return (this);
}
bb_expr_Expr* bb_expr_CastExpr::m_Copy(){
	return ((new bb_expr_CastExpr)->g_new(f_ty,m_CopyExpr(f_expr),f_flags));
}
String bb_expr_CastExpr::m_Eval(){
	String t_val=f_expr->m_Eval();
	if(!((t_val).Length()!=0)){
		return t_val;
	}
	if((dynamic_cast<bb_type_BoolType*>(f_exprType))!=0){
		if((dynamic_cast<bb_type_IntType*>(f_expr->f_exprType))!=0){
			if(((t_val).ToInt())!=0){
				return String(L"1",1);
			}
			return String();
		}else{
			if((dynamic_cast<bb_type_FloatType*>(f_expr->f_exprType))!=0){
				if(((t_val).ToFloat())!=0){
					return String(L"1",1);
				}
				return String();
			}else{
				if((dynamic_cast<bb_type_StringType*>(f_expr->f_exprType))!=0){
					if((t_val.Length())!=0){
						return String(L"1",1);
					}
					return String();
				}
			}
		}
	}else{
		if((dynamic_cast<bb_type_IntType*>(f_exprType))!=0){
			if((dynamic_cast<bb_type_BoolType*>(f_expr->f_exprType))!=0){
				if((t_val).Length()!=0){
					return String(L"1",1);
				}
				return String(L"0",1);
			}
			return String((t_val).ToInt());
		}else{
			if((dynamic_cast<bb_type_FloatType*>(f_exprType))!=0){
				return String((t_val).ToFloat());
			}else{
				if((dynamic_cast<bb_type_StringType*>(f_exprType))!=0){
					return t_val;
				}
			}
		}
	}
	return bb_expr_Expr::m_Eval();
}
String bb_expr_CastExpr::m_Trans(){
	return bb_translator__trans->m_TransCastExpr(this);
}
void bb_expr_CastExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_ty);
	gc_mark_q(f_expr);
}
bb_parser_IdentExpr::bb_parser_IdentExpr(){
	f_ident=String();
	f_expr=0;
	f_scope=0;
	f_static=false;
}
bb_parser_IdentExpr* bb_parser_IdentExpr::g_new(String t_ident,bb_expr_Expr* t_expr){
	bb_expr_Expr::g_new();
	this->f_ident=t_ident;
	gc_assign(this->f_expr,t_expr);
	return this;
}
bb_parser_IdentExpr* bb_parser_IdentExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_parser_IdentExpr::m_Copy(){
	return ((new bb_parser_IdentExpr)->g_new(f_ident,m_CopyExpr(f_expr)));
}
String bb_parser_IdentExpr::m_ToString(){
	String t_t=String(L"IdentExpr(\"",11)+f_ident+String(L"\"",1);
	if((f_expr)!=0){
		t_t=t_t+(String(L",",1)+f_expr->m_ToString());
	}
	return t_t+String(L")",1);
}
int bb_parser_IdentExpr::m__Semant(){
	if((f_scope)!=0){
		return 0;
	}
	if((f_expr)!=0){
		gc_assign(f_scope,f_expr->m_SemantScope());
		if((f_scope)!=0){
			f_static=true;
		}else{
			gc_assign(f_expr,f_expr->m_Semant());
			gc_assign(f_scope,(f_expr->f_exprType->m_GetClass()));
			if(!((f_scope)!=0)){
				bb_config_Err(String(L"Expression has no scope",23));
			}
		}
	}else{
		gc_assign(f_scope,bb_decl__env);
		f_static=bb_decl__env->m_FuncScope()==0 || bb_decl__env->m_FuncScope()->m_IsStatic();
	}
	return 0;
}
int bb_parser_IdentExpr::m_IdentErr(){
	String t_close=String();
	bb_list_Enumerator2* t_=f_scope->m_Decls()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		if(f_ident.ToLower()==t_decl->f_ident.ToLower()){
			t_close=t_decl->f_ident;
		}
	}
	if((t_close).Length()!=0){
		bb_config_Err(String(L"Identifier '",12)+f_ident+String(L"' not found - perhaps you meant '",33)+t_close+String(L"'?",2));
	}
	bb_config_Err(String(L"Identifier '",12)+f_ident+String(L"' not found.",12));
	return 0;
}
bb_expr_Expr* bb_parser_IdentExpr::m_SemantSet(String t_op,bb_expr_Expr* t_rhs){
	m__Semant();
	bb_decl_ValDecl* t_vdecl=f_scope->m_FindValDecl(f_ident);
	if((t_vdecl)!=0){
		if((dynamic_cast<bb_decl_ConstDecl*>(t_vdecl))!=0){
			if((t_rhs)!=0){
				bb_config_Err(String(L"Constant '",10)+f_ident+String(L"' cannot be modified.",21));
			}
			return ((new bb_expr_ConstExpr)->g_new(t_vdecl->f_type,dynamic_cast<bb_decl_ConstDecl*>(t_vdecl)->f_value))->m_Semant();
		}else{
			if((dynamic_cast<bb_decl_FieldDecl*>(t_vdecl))!=0){
				if(f_static){
					bb_config_Err(String(L"Field '",7)+f_ident+String(L"' cannot be accessed from here.",31));
				}
				if((f_expr)!=0){
					return ((new bb_expr_MemberVarExpr)->g_new(f_expr,dynamic_cast<bb_decl_VarDecl*>(t_vdecl)))->m_Semant();
				}
			}
		}
		return ((new bb_expr_VarExpr)->g_new(dynamic_cast<bb_decl_VarDecl*>(t_vdecl)))->m_Semant();
	}
	if(((t_op).Length()!=0) && t_op!=String(L"=",1)){
		bb_decl_FuncDecl* t_fdecl=f_scope->m_FindFuncDecl(f_ident,Array<bb_expr_Expr* >(),0);
		if(!((t_fdecl)!=0)){
			m_IdentErr();
		}
		if(((bb_decl__env->m_ModuleScope()->m_IsStrict())!=0) && !t_fdecl->m_IsProperty()){
			bb_config_Err(String(L"Identifier '",12)+f_ident+String(L"' cannot be used in this way.",29));
		}
		bb_expr_Expr* t_lhs=0;
		if(t_fdecl->m_IsStatic() || f_scope==bb_decl__env && !bb_decl__env->m_FuncScope()->m_IsStatic()){
			t_lhs=((new bb_expr_InvokeExpr)->g_new(t_fdecl,Array<bb_expr_Expr* >()));
		}else{
			if((f_expr)!=0){
				bb_decl_LocalDecl* t_tmp=(new bb_decl_LocalDecl)->g_new(String(),0,0,f_expr);
				t_lhs=((new bb_expr_InvokeMemberExpr)->g_new(((new bb_expr_VarExpr)->g_new(t_tmp)),t_fdecl,Array<bb_expr_Expr* >()));
				t_lhs=((new bb_expr_StmtExpr)->g_new(((new bb_stmt_DeclStmt)->g_new(t_tmp)),t_lhs));
			}else{
				return 0;
			}
		}
		String t_bop=t_op.Slice(0,1);
		String t_=t_bop;
		if(t_==String(L"*",1) || t_==String(L"/",1) || t_==String(L"shl",3) || t_==String(L"shr",3) || t_==String(L"+",1) || t_==String(L"-",1) || t_==String(L"&",1) || t_==String(L"|",1) || t_==String(L"~",1)){
			t_rhs=((new bb_expr_BinaryMathExpr)->g_new(t_bop,t_lhs,t_rhs));
		}else{
			bb_config_InternalErr(String(L"Internal error",14));
		}
		t_rhs=t_rhs->m_Semant();
	}
	Array<bb_expr_Expr* > t_args=Array<bb_expr_Expr* >();
	if((t_rhs)!=0){
		bb_expr_Expr* t_2[]={t_rhs};
		t_args=Array<bb_expr_Expr* >(t_2,1);
	}
	bb_decl_FuncDecl* t_fdecl2=f_scope->m_FindFuncDecl(f_ident,t_args,0);
	if((t_fdecl2)!=0){
		if(((bb_decl__env->m_ModuleScope()->m_IsStrict())!=0) && !t_fdecl2->m_IsProperty()){
			bb_config_Err(String(L"Identifier '",12)+f_ident+String(L"' cannot be used in this way.",29));
		}
		if(!t_fdecl2->m_IsStatic()){
			if(f_static){
				bb_config_Err(String(L"Method '",8)+f_ident+String(L"' cannot be accessed from here.",31));
			}
			if((f_expr)!=0){
				return ((new bb_expr_InvokeMemberExpr)->g_new(f_expr,t_fdecl2,t_args))->m_Semant();
			}
		}
		return ((new bb_expr_InvokeExpr)->g_new(t_fdecl2,t_args))->m_Semant();
	}
	m_IdentErr();
	return 0;
}
bb_expr_Expr* bb_parser_IdentExpr::m_Semant(){
	return m_SemantSet(String(),0);
}
bb_decl_ScopeDecl* bb_parser_IdentExpr::m_SemantScope(){
	m__Semant();
	return f_scope->m_FindScopeDecl(f_ident);
}
bb_expr_Expr* bb_parser_IdentExpr::m_SemantFunc(Array<bb_expr_Expr* > t_args){
	m__Semant();
	bb_decl_FuncDecl* t_fdecl=f_scope->m_FindFuncDecl(f_ident,t_args,0);
	if((t_fdecl)!=0){
		if(!t_fdecl->m_IsStatic()){
			if(f_static){
				bb_config_Err(String(L"Method '",8)+f_ident+String(L"' cannot be accessed from here.",31));
			}
			if((f_expr)!=0){
				return ((new bb_expr_InvokeMemberExpr)->g_new(f_expr,t_fdecl,t_args))->m_Semant();
			}
		}
		return ((new bb_expr_InvokeExpr)->g_new(t_fdecl,t_args))->m_Semant();
	}
	bb_type_Type* t_type=f_scope->m_FindType(f_ident,Array<bb_type_Type* >());
	if((t_type)!=0){
		if(t_args.Length()==1 && ((t_args[0])!=0)){
			return t_args[0]->m_Cast(t_type,1);
		}
		bb_config_Err(String(L"Illegal number of arguments for type conversion",47));
	}
	m_IdentErr();
	return 0;
}
void bb_parser_IdentExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_expr);
	gc_mark_q(f_scope);
}
bb_expr_SelfExpr::bb_expr_SelfExpr(){
}
bb_expr_SelfExpr* bb_expr_SelfExpr::g_new(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_SelfExpr::m_Copy(){
	return ((new bb_expr_SelfExpr)->g_new());
}
bb_expr_Expr* bb_expr_SelfExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	if((bb_decl__env->m_FuncScope())!=0){
		if(bb_decl__env->m_FuncScope()->m_IsStatic()){
			bb_config_Err(String(L"Illegal use of Self within static scope.",40));
		}
	}else{
		bb_config_Err(String(L"Self cannot be used here.",25));
	}
	gc_assign(f_exprType,(bb_decl__env->m_ClassScope()->f_objectType));
	return (this);
}
bool bb_expr_SelfExpr::m_SideEffects(){
	return false;
}
String bb_expr_SelfExpr::m_Trans(){
	return bb_translator__trans->m_TransSelfExpr(this);
}
void bb_expr_SelfExpr::mark(){
	bb_expr_Expr::mark();
}
bb_stmt_Stmt::bb_stmt_Stmt(){
	f_errInfo=String();
}
bb_stmt_Stmt* bb_stmt_Stmt::g_new(){
	f_errInfo=bb_config__errInfo;
	return this;
}
bb_stmt_Stmt* bb_stmt_Stmt::m_Copy2(bb_decl_ScopeDecl* t_scope){
	bb_stmt_Stmt* t_t=m_OnCopy2(t_scope);
	t_t->f_errInfo=f_errInfo;
	return t_t;
}
int bb_stmt_Stmt::m_Semant(){
	bb_config_PushErr(f_errInfo);
	m_OnSemant();
	bb_config_PopErr();
	return 0;
}
void bb_stmt_Stmt::mark(){
	Object::mark();
}
bb_list_List4::bb_list_List4(){
	f__head=((new bb_list_HeadNode4)->g_new());
}
bb_list_List4* bb_list_List4::g_new(){
	return this;
}
bb_list_Node4* bb_list_List4::m_AddLast4(bb_stmt_Stmt* t_data){
	return (new bb_list_Node4)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List4* bb_list_List4::g_new2(Array<bb_stmt_Stmt* > t_data){
	Array<bb_stmt_Stmt* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_stmt_Stmt* t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast4(t_t);
	}
	return this;
}
bool bb_list_List4::m_IsEmpty(){
	return f__head->f__succ==f__head;
}
bb_list_Enumerator5* bb_list_List4::m_ObjectEnumerator(){
	return (new bb_list_Enumerator5)->g_new(this);
}
bb_list_Node4* bb_list_List4::m_AddFirst(bb_stmt_Stmt* t_data){
	return (new bb_list_Node4)->g_new(f__head->f__succ,f__head,t_data);
}
bb_stmt_Stmt* bb_list_List4::m_Last(){
	return f__head->m_PrevNode()->f__data;
}
void bb_list_List4::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_list_Node4::bb_list_Node4(){
	f__succ=0;
	f__pred=0;
	f__data=0;
}
bb_list_Node4* bb_list_Node4::g_new(bb_list_Node4* t_succ,bb_list_Node4* t_pred,bb_stmt_Stmt* t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	gc_assign(f__data,t_data);
	return this;
}
bb_list_Node4* bb_list_Node4::g_new2(){
	return this;
}
bb_list_Node4* bb_list_Node4::m_GetNode(){
	return this;
}
bb_list_Node4* bb_list_Node4::m_PrevNode(){
	return f__pred->m_GetNode();
}
void bb_list_Node4::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
	gc_mark_q(f__data);
}
bb_list_HeadNode4::bb_list_HeadNode4(){
}
bb_list_HeadNode4* bb_list_HeadNode4::g_new(){
	bb_list_Node4::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
bb_list_Node4* bb_list_HeadNode4::m_GetNode(){
	return 0;
}
void bb_list_HeadNode4::mark(){
	bb_list_Node4::mark();
}
bb_expr_InvokeSuperExpr::bb_expr_InvokeSuperExpr(){
	f_ident=String();
	f_args=Array<bb_expr_Expr* >();
	f_funcDecl=0;
}
bb_expr_InvokeSuperExpr* bb_expr_InvokeSuperExpr::g_new(String t_ident,Array<bb_expr_Expr* > t_args){
	bb_expr_Expr::g_new();
	this->f_ident=t_ident;
	gc_assign(this->f_args,t_args);
	return this;
}
bb_expr_InvokeSuperExpr* bb_expr_InvokeSuperExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_InvokeSuperExpr::m_Copy(){
	return ((new bb_expr_InvokeSuperExpr)->g_new(f_ident,m_CopyArgs(f_args)));
}
bb_expr_Expr* bb_expr_InvokeSuperExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	if(bb_decl__env->m_FuncScope()->m_IsStatic()){
		bb_config_Err(String(L"Illegal use of Super.",21));
	}
	bb_decl_ClassDecl* t_classScope=bb_decl__env->m_ClassScope();
	bb_decl_ClassDecl* t_superClass=t_classScope->f_superClass;
	if(!((t_superClass)!=0)){
		bb_config_Err(String(L"Class has no super class.",25));
	}
	gc_assign(f_args,m_SemantArgs(f_args));
	gc_assign(f_funcDecl,t_superClass->m_FindFuncDecl(f_ident,f_args,0));
	if(!((f_funcDecl)!=0)){
		bb_config_Err(String(L"Can't find superclass method '",30)+f_ident+String(L"'.",2));
	}
	if((f_funcDecl->m_IsAbstract())!=0){
		bb_config_Err(String(L"Can't invoke abstract superclass method '",41)+f_ident+String(L"'.",2));
	}
	gc_assign(f_args,m_CastArgs(f_args,f_funcDecl));
	gc_assign(f_exprType,f_funcDecl->f_retType);
	return (this);
}
String bb_expr_InvokeSuperExpr::m_Trans(){
	return bb_translator__trans->m_TransInvokeSuperExpr(this);
}
void bb_expr_InvokeSuperExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_args);
	gc_mark_q(f_funcDecl);
}
bb_parser_IdentTypeExpr::bb_parser_IdentTypeExpr(){
	f_cdecl=0;
}
bb_parser_IdentTypeExpr* bb_parser_IdentTypeExpr::g_new(bb_type_Type* t_type){
	bb_expr_Expr::g_new();
	gc_assign(this->f_exprType,t_type);
	return this;
}
bb_parser_IdentTypeExpr* bb_parser_IdentTypeExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_parser_IdentTypeExpr::m_Copy(){
	return ((new bb_parser_IdentTypeExpr)->g_new(f_exprType));
}
int bb_parser_IdentTypeExpr::m__Semant(){
	if((f_cdecl)!=0){
		return 0;
	}
	gc_assign(f_exprType,f_exprType->m_Semant());
	gc_assign(f_cdecl,f_exprType->m_GetClass());
	if(!((f_cdecl)!=0)){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	return 0;
}
bb_expr_Expr* bb_parser_IdentTypeExpr::m_Semant(){
	m__Semant();
	bb_config_Err(String(L"Expression can't be used in this way",36));
	return 0;
}
bb_decl_ScopeDecl* bb_parser_IdentTypeExpr::m_SemantScope(){
	m__Semant();
	return (f_cdecl);
}
bb_expr_Expr* bb_parser_IdentTypeExpr::m_SemantFunc(Array<bb_expr_Expr* > t_args){
	m__Semant();
	if(t_args.Length()==1 && ((t_args[0])!=0)){
		return t_args[0]->m_Cast((f_cdecl->f_objectType),1);
	}
	bb_config_Err(String(L"Illegal number of arguments for type conversion",47));
	return 0;
}
void bb_parser_IdentTypeExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_cdecl);
}
String bb_config_Dequote(String t_str,String t_lang){
	String t_=t_lang;
	if(t_==String(L"monkey",6)){
		if(t_str.Length()<2 || !t_str.StartsWith(String(L"\"",1)) || !t_str.EndsWith(String(L"\"",1))){
			bb_config_InternalErr(String(L"Internal error",14));
		}
		t_str=t_str.Slice(1,-1);
		int t_i=0;
		do{
			t_i=t_str.Find(String(L"~",1),t_i);
			if(t_i==-1){
				break;
			}
			if(t_i+1>=t_str.Length()){
				bb_config_Err(String(L"Invalid escape sequence in string",33));
			}
			String t_ch=t_str.Slice(t_i+1,t_i+2);
			String t_2=t_ch;
			if(t_2==String(L"~",1)){
				t_ch=String(L"~",1);
			}else{
				if(t_2==String(L"q",1)){
					t_ch=String(L"\"",1);
				}else{
					if(t_2==String(L"n",1)){
						t_ch=String(L"\n",1);
					}else{
						if(t_2==String(L"r",1)){
							t_ch=String(L"\r",1);
						}else{
							if(t_2==String(L"t",1)){
								t_ch=String(L"\t",1);
							}else{
								if(t_2==String(L"u",1)){
									String t_t=t_str.Slice(t_i+2,t_i+6);
									if(t_t.Length()!=4){
										bb_config_Err(String(L"Invalid unicode hex value in string",35));
									}
									for(int t_j=0;t_j<4;t_j=t_j+1){
										if(!((bb_config_IsHexDigit((int)t_t[t_j]))!=0)){
											bb_config_Err(String(L"Invalid unicode hex digit in string",35));
										}
									}
									t_str=t_str.Slice(0,t_i)+String((Char)(bb_config_StringToInt(t_t,16)),1)+t_str.Slice(t_i+6);
									t_i+=1;
									continue;
								}else{
									if(t_2==String(L"0",1)){
										t_ch=String((Char)(0),1);
									}else{
										bb_config_Err(String(L"Invalid escape character in string: '",37)+t_ch+String(L"'",1));
									}
								}
							}
						}
					}
				}
			}
			t_str=t_str.Slice(0,t_i)+t_ch+t_str.Slice(t_i+2);
			t_i+=t_ch.Length();
		}while(!(false));
		return t_str;
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
bb_parser_FuncCallExpr::bb_parser_FuncCallExpr(){
	f_expr=0;
	f_args=Array<bb_expr_Expr* >();
}
bb_parser_FuncCallExpr* bb_parser_FuncCallExpr::g_new(bb_expr_Expr* t_expr,Array<bb_expr_Expr* > t_args){
	bb_expr_Expr::g_new();
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_args,t_args);
	return this;
}
bb_parser_FuncCallExpr* bb_parser_FuncCallExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_parser_FuncCallExpr::m_Copy(){
	return ((new bb_parser_FuncCallExpr)->g_new(m_CopyExpr(f_expr),m_CopyArgs(f_args)));
}
String bb_parser_FuncCallExpr::m_ToString(){
	String t_t=String(L"FuncCallExpr(",13)+f_expr->m_ToString();
	Array<bb_expr_Expr* > t_=f_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_arg=t_[t_2];
		t_2=t_2+1;
		t_t=t_t+(String(L",",1)+t_arg->m_ToString());
	}
	return t_t+String(L")",1);
}
bb_expr_Expr* bb_parser_FuncCallExpr::m_Semant(){
	gc_assign(f_args,m_SemantArgs(f_args));
	return f_expr->m_SemantFunc(f_args);
}
void bb_parser_FuncCallExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_expr);
	gc_mark_q(f_args);
}
bb_expr_SliceExpr::bb_expr_SliceExpr(){
	f_expr=0;
	f_from=0;
	f_term=0;
}
bb_expr_SliceExpr* bb_expr_SliceExpr::g_new(bb_expr_Expr* t_expr,bb_expr_Expr* t_from,bb_expr_Expr* t_term){
	bb_expr_Expr::g_new();
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_from,t_from);
	gc_assign(this->f_term,t_term);
	return this;
}
bb_expr_SliceExpr* bb_expr_SliceExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_SliceExpr::m_Copy(){
	return ((new bb_expr_SliceExpr)->g_new(m_CopyExpr(f_expr),m_CopyExpr(f_from),m_CopyExpr(f_term)));
}
bb_expr_Expr* bb_expr_SliceExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_expr,f_expr->m_Semant());
	if(((dynamic_cast<bb_type_ArrayType*>(f_expr->f_exprType))!=0) || ((dynamic_cast<bb_type_StringType*>(f_expr->f_exprType))!=0)){
		if((f_from)!=0){
			gc_assign(f_from,f_from->m_Semant2((bb_type_Type::g_intType),0));
		}
		if((f_term)!=0){
			gc_assign(f_term,f_term->m_Semant2((bb_type_Type::g_intType),0));
		}
		gc_assign(f_exprType,f_expr->f_exprType);
	}else{
		bb_config_Err(String(L"Slices can only be used on strings or arrays.",45));
	}
	return (this);
}
String bb_expr_SliceExpr::m_Eval(){
	int t_from=(this->f_from->m_Eval()).ToInt();
	int t_term=(this->f_term->m_Eval()).ToInt();
	if((dynamic_cast<bb_type_StringType*>(f_expr->f_exprType))!=0){
		return f_expr->m_Eval().Slice(t_from,t_term);
	}else{
		if((dynamic_cast<bb_type_ArrayType*>(f_expr->f_exprType))!=0){
			bb_config_Err(String(L"TODO!",5));
		}
	}
	return String();
}
String bb_expr_SliceExpr::m_Trans(){
	return bb_translator__trans->m_TransSliceExpr(this);
}
void bb_expr_SliceExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_expr);
	gc_mark_q(f_from);
	gc_mark_q(f_term);
}
bb_expr_IndexExpr::bb_expr_IndexExpr(){
	f_expr=0;
	f_index=0;
}
bb_expr_IndexExpr* bb_expr_IndexExpr::g_new(bb_expr_Expr* t_expr,bb_expr_Expr* t_index){
	bb_expr_Expr::g_new();
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_index,t_index);
	return this;
}
bb_expr_IndexExpr* bb_expr_IndexExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_IndexExpr::m_Copy(){
	return ((new bb_expr_IndexExpr)->g_new(m_CopyExpr(f_expr),m_CopyExpr(f_index)));
}
bb_expr_Expr* bb_expr_IndexExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_expr,f_expr->m_Semant());
	gc_assign(f_index,f_index->m_Semant2((bb_type_Type::g_intType),0));
	if((dynamic_cast<bb_type_StringType*>(f_expr->f_exprType))!=0){
		gc_assign(f_exprType,(bb_type_Type::g_intType));
	}else{
		if((dynamic_cast<bb_type_ArrayType*>(f_expr->f_exprType))!=0){
			gc_assign(f_exprType,dynamic_cast<bb_type_ArrayType*>(f_expr->f_exprType)->f_elemType);
		}else{
			bb_config_Err(String(L"Only strings and arrays may be indexed.",39));
		}
	}
	if(((dynamic_cast<bb_type_StringType*>(f_expr->f_exprType))!=0) && ((dynamic_cast<bb_expr_ConstExpr*>(f_expr))!=0) && ((dynamic_cast<bb_expr_ConstExpr*>(f_index))!=0)){
		return m_EvalConst();
	}
	return (this);
}
String bb_expr_IndexExpr::m_Eval(){
	if((dynamic_cast<bb_type_StringType*>(f_expr->f_exprType))!=0){
		String t_str=f_expr->m_Eval();
		int t_idx=(f_index->m_Eval()).ToInt();
		if(t_idx<0 || t_idx>=t_str.Length()){
			bb_config_Err(String(L"String index out of range.",26));
		}
		return String((int)t_str[t_idx]);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
bb_expr_Expr* bb_expr_IndexExpr::m_SemantSet(String t_op,bb_expr_Expr* t_rhs){
	m_Semant();
	if((dynamic_cast<bb_type_StringType*>(f_expr->f_exprType))!=0){
		bb_config_Err(String(L"Strings are read only.",22));
	}
	return (this);
}
bool bb_expr_IndexExpr::m_SideEffects(){
	return bb_config_ENV_CONFIG==String(L"debug",5);
}
String bb_expr_IndexExpr::m_Trans(){
	return bb_translator__trans->m_TransIndexExpr(this);
}
String bb_expr_IndexExpr::m_TransVar(){
	return bb_translator__trans->m_TransIndexExpr(this);
}
void bb_expr_IndexExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_expr);
	gc_mark_q(f_index);
}
bb_expr_BinaryExpr::bb_expr_BinaryExpr(){
	f_op=String();
	f_lhs=0;
	f_rhs=0;
}
bb_expr_BinaryExpr* bb_expr_BinaryExpr::g_new(String t_op,bb_expr_Expr* t_lhs,bb_expr_Expr* t_rhs){
	bb_expr_Expr::g_new();
	this->f_op=t_op;
	gc_assign(this->f_lhs,t_lhs);
	gc_assign(this->f_rhs,t_rhs);
	return this;
}
bb_expr_BinaryExpr* bb_expr_BinaryExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
String bb_expr_BinaryExpr::m_Trans(){
	return bb_translator__trans->m_TransBinaryExpr(this);
}
void bb_expr_BinaryExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_lhs);
	gc_mark_q(f_rhs);
}
bb_expr_BinaryMathExpr::bb_expr_BinaryMathExpr(){
}
bb_expr_BinaryMathExpr* bb_expr_BinaryMathExpr::g_new(String t_op,bb_expr_Expr* t_lhs,bb_expr_Expr* t_rhs){
	bb_expr_BinaryExpr::g_new2();
	this->f_op=t_op;
	gc_assign(this->f_lhs,t_lhs);
	gc_assign(this->f_rhs,t_rhs);
	return this;
}
bb_expr_BinaryMathExpr* bb_expr_BinaryMathExpr::g_new2(){
	bb_expr_BinaryExpr::g_new2();
	return this;
}
bb_expr_Expr* bb_expr_BinaryMathExpr::m_Copy(){
	return ((new bb_expr_BinaryMathExpr)->g_new(f_op,m_CopyExpr(f_lhs),m_CopyExpr(f_rhs)));
}
bb_expr_Expr* bb_expr_BinaryMathExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_lhs,f_lhs->m_Semant());
	gc_assign(f_rhs,f_rhs->m_Semant());
	String t_=f_op;
	if(t_==String(L"&",1) || t_==String(L"~",1) || t_==String(L"|",1) || t_==String(L"shl",3) || t_==String(L"shr",3)){
		gc_assign(f_exprType,(bb_type_Type::g_intType));
	}else{
		gc_assign(f_exprType,m_BalanceTypes(f_lhs->f_exprType,f_rhs->f_exprType));
		if((dynamic_cast<bb_type_StringType*>(f_exprType))!=0){
			if(f_op!=String(L"+",1)){
				bb_config_Err(String(L"Illegal string operator.",24));
			}
		}else{
			if(!((dynamic_cast<bb_type_NumericType*>(f_exprType))!=0)){
				bb_config_Err(String(L"Illegal expression type.",24));
			}
		}
	}
	gc_assign(f_lhs,f_lhs->m_Cast(f_exprType,0));
	gc_assign(f_rhs,f_rhs->m_Cast(f_exprType,0));
	if(((dynamic_cast<bb_expr_ConstExpr*>(f_lhs))!=0) && ((dynamic_cast<bb_expr_ConstExpr*>(f_rhs))!=0)){
		return m_EvalConst();
	}
	return (this);
}
String bb_expr_BinaryMathExpr::m_Eval(){
	String t_lhs=this->f_lhs->m_Eval();
	String t_rhs=this->f_rhs->m_Eval();
	if((dynamic_cast<bb_type_IntType*>(f_exprType))!=0){
		int t_x=(t_lhs).ToInt();
		int t_y=(t_rhs).ToInt();
		String t_=f_op;
		if(t_==String(L"/",1)){
			if(!((t_y)!=0)){
				bb_config_Err(String(L"Divide by zero error.",21));
			}
			return String(t_x/t_y);
		}else{
			if(t_==String(L"*",1)){
				return String(t_x*t_y);
			}else{
				if(t_==String(L"mod",3)){
					return String(t_x % t_y);
				}else{
					if(t_==String(L"shl",3)){
						return String(t_x<<t_y);
					}else{
						if(t_==String(L"shr",3)){
							return String(t_x>>t_y);
						}else{
							if(t_==String(L"+",1)){
								return String(t_x+t_y);
							}else{
								if(t_==String(L"-",1)){
									return String(t_x-t_y);
								}else{
									if(t_==String(L"&",1)){
										return String(t_x&t_y);
									}else{
										if(t_==String(L"~",1)){
											return String(t_x^t_y);
										}else{
											if(t_==String(L"|",1)){
												return String(t_x|t_y);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}else{
		if((dynamic_cast<bb_type_FloatType*>(f_exprType))!=0){
			Float t_x2=(t_lhs).ToFloat();
			Float t_y2=(t_rhs).ToFloat();
			String t_2=f_op;
			if(t_2==String(L"/",1)){
				if(!((t_y2)!=0)){
					bb_config_Err(String(L"Divide by zero error.",21));
				}
				return String(t_x2/t_y2);
			}else{
				if(t_2==String(L"*",1)){
					return String(t_x2*t_y2);
				}else{
					if(t_2==String(L"+",1)){
						return String(t_x2+t_y2);
					}else{
						if(t_2==String(L"-",1)){
							return String(t_x2-t_y2);
						}
					}
				}
			}
		}else{
			if((dynamic_cast<bb_type_StringType*>(f_exprType))!=0){
				String t_3=f_op;
				if(t_3==String(L"+",1)){
					return t_lhs+t_rhs;
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
void bb_expr_BinaryMathExpr::mark(){
	bb_expr_BinaryExpr::mark();
}
bb_expr_BinaryCompareExpr::bb_expr_BinaryCompareExpr(){
	f_ty=0;
}
bb_expr_BinaryCompareExpr* bb_expr_BinaryCompareExpr::g_new(String t_op,bb_expr_Expr* t_lhs,bb_expr_Expr* t_rhs){
	bb_expr_BinaryExpr::g_new2();
	this->f_op=t_op;
	gc_assign(this->f_lhs,t_lhs);
	gc_assign(this->f_rhs,t_rhs);
	return this;
}
bb_expr_BinaryCompareExpr* bb_expr_BinaryCompareExpr::g_new2(){
	bb_expr_BinaryExpr::g_new2();
	return this;
}
bb_expr_Expr* bb_expr_BinaryCompareExpr::m_Copy(){
	return ((new bb_expr_BinaryCompareExpr)->g_new(f_op,m_CopyExpr(f_lhs),m_CopyExpr(f_rhs)));
}
bb_expr_Expr* bb_expr_BinaryCompareExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_lhs,f_lhs->m_Semant());
	gc_assign(f_rhs,f_rhs->m_Semant());
	gc_assign(f_ty,m_BalanceTypes(f_lhs->f_exprType,f_rhs->f_exprType));
	if((dynamic_cast<bb_type_ArrayType*>(f_ty))!=0){
		bb_config_Err(String(L"Arrays cannot be compared.",26));
	}
	if(((dynamic_cast<bb_type_ObjectType*>(f_ty))!=0) && f_op!=String(L"=",1) && f_op!=String(L"<>",2)){
		bb_config_Err(String(L"Objects can only be compared for equality.",42));
	}
	gc_assign(f_lhs,f_lhs->m_Cast(f_ty,0));
	gc_assign(f_rhs,f_rhs->m_Cast(f_ty,0));
	gc_assign(f_exprType,(bb_type_Type::g_boolType));
	if(((dynamic_cast<bb_expr_ConstExpr*>(f_lhs))!=0) && ((dynamic_cast<bb_expr_ConstExpr*>(f_rhs))!=0)){
		return m_EvalConst();
	}
	return (this);
}
String bb_expr_BinaryCompareExpr::m_Eval(){
	int t_r=-1;
	if((dynamic_cast<bb_type_IntType*>(f_ty))!=0){
		int t_lhs=(this->f_lhs->m_Eval()).ToInt();
		int t_rhs=(this->f_rhs->m_Eval()).ToInt();
		String t_=f_op;
		if(t_==String(L"=",1)){
			t_r=((t_lhs==t_rhs)?1:0);
		}else{
			if(t_==String(L"<>",2)){
				t_r=((t_lhs!=t_rhs)?1:0);
			}else{
				if(t_==String(L"<",1)){
					t_r=((t_lhs<t_rhs)?1:0);
				}else{
					if(t_==String(L"<=",2)){
						t_r=((t_lhs<=t_rhs)?1:0);
					}else{
						if(t_==String(L">",1)){
							t_r=((t_lhs>t_rhs)?1:0);
						}else{
							if(t_==String(L">=",2)){
								t_r=((t_lhs>=t_rhs)?1:0);
							}
						}
					}
				}
			}
		}
	}else{
		if((dynamic_cast<bb_type_FloatType*>(f_ty))!=0){
			Float t_lhs2=(this->f_lhs->m_Eval()).ToFloat();
			Float t_rhs2=(this->f_rhs->m_Eval()).ToFloat();
			String t_2=f_op;
			if(t_2==String(L"=",1)){
				t_r=((t_lhs2==t_rhs2)?1:0);
			}else{
				if(t_2==String(L"<>",2)){
					t_r=((t_lhs2!=t_rhs2)?1:0);
				}else{
					if(t_2==String(L"<",1)){
						t_r=((t_lhs2<t_rhs2)?1:0);
					}else{
						if(t_2==String(L"<=",2)){
							t_r=((t_lhs2<=t_rhs2)?1:0);
						}else{
							if(t_2==String(L">",1)){
								t_r=((t_lhs2>t_rhs2)?1:0);
							}else{
								if(t_2==String(L">=",2)){
									t_r=((t_lhs2>=t_rhs2)?1:0);
								}
							}
						}
					}
				}
			}
		}else{
			if((dynamic_cast<bb_type_StringType*>(f_ty))!=0){
				String t_lhs3=this->f_lhs->m_Eval();
				String t_rhs3=this->f_rhs->m_Eval();
				String t_3=f_op;
				if(t_3==String(L"=",1)){
					t_r=((t_lhs3==t_rhs3)?1:0);
				}else{
					if(t_3==String(L"<>",2)){
						t_r=((t_lhs3!=t_rhs3)?1:0);
					}else{
						if(t_3==String(L"<",1)){
							t_r=((t_lhs3<t_rhs3)?1:0);
						}else{
							if(t_3==String(L"<=",2)){
								t_r=((t_lhs3<=t_rhs3)?1:0);
							}else{
								if(t_3==String(L">",1)){
									t_r=((t_lhs3>t_rhs3)?1:0);
								}else{
									if(t_3==String(L">=",2)){
										t_r=((t_lhs3>=t_rhs3)?1:0);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if(t_r==1){
		return String(L"1",1);
	}
	if(t_r==0){
		return String();
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
void bb_expr_BinaryCompareExpr::mark(){
	bb_expr_BinaryExpr::mark();
	gc_mark_q(f_ty);
}
bb_expr_BinaryLogicExpr::bb_expr_BinaryLogicExpr(){
}
bb_expr_BinaryLogicExpr* bb_expr_BinaryLogicExpr::g_new(String t_op,bb_expr_Expr* t_lhs,bb_expr_Expr* t_rhs){
	bb_expr_BinaryExpr::g_new2();
	this->f_op=t_op;
	gc_assign(this->f_lhs,t_lhs);
	gc_assign(this->f_rhs,t_rhs);
	return this;
}
bb_expr_BinaryLogicExpr* bb_expr_BinaryLogicExpr::g_new2(){
	bb_expr_BinaryExpr::g_new2();
	return this;
}
bb_expr_Expr* bb_expr_BinaryLogicExpr::m_Copy(){
	return ((new bb_expr_BinaryLogicExpr)->g_new(f_op,m_CopyExpr(f_lhs),m_CopyExpr(f_rhs)));
}
bb_expr_Expr* bb_expr_BinaryLogicExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_lhs,f_lhs->m_Semant2((bb_type_Type::g_boolType),1));
	gc_assign(f_rhs,f_rhs->m_Semant2((bb_type_Type::g_boolType),1));
	gc_assign(f_exprType,(bb_type_Type::g_boolType));
	if(((dynamic_cast<bb_expr_ConstExpr*>(f_lhs))!=0) && ((dynamic_cast<bb_expr_ConstExpr*>(f_rhs))!=0)){
		return m_EvalConst();
	}
	return (this);
}
String bb_expr_BinaryLogicExpr::m_Eval(){
	String t_=f_op;
	if(t_==String(L"and",3)){
		if(((f_lhs->m_Eval()).Length()!=0) && ((f_rhs->m_Eval()).Length()!=0)){
			return String(L"1",1);
		}else{
			return String();
		}
	}else{
		if(t_==String(L"or",2)){
			if(((f_lhs->m_Eval()).Length()!=0) || ((f_rhs->m_Eval()).Length()!=0)){
				return String(L"1",1);
			}else{
				return String();
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
void bb_expr_BinaryLogicExpr::mark(){
	bb_expr_BinaryExpr::mark();
}
bb_type_ObjectType::bb_type_ObjectType(){
	f_classDecl=0;
}
bb_type_ObjectType* bb_type_ObjectType::g_new(bb_decl_ClassDecl* t_classDecl){
	bb_type_Type::g_new();
	gc_assign(this->f_classDecl,t_classDecl);
	return this;
}
bb_type_ObjectType* bb_type_ObjectType::g_new2(){
	bb_type_Type::g_new();
	return this;
}
int bb_type_ObjectType::m_EqualsType(bb_type_Type* t_ty){
	bb_type_ObjectType* t_objty=dynamic_cast<bb_type_ObjectType*>(t_ty);
	return ((((t_objty)!=0) && f_classDecl==t_objty->f_classDecl)?1:0);
}
bb_decl_ClassDecl* bb_type_ObjectType::m_GetClass(){
	return f_classDecl;
}
int bb_type_ObjectType::m_ExtendsType(bb_type_Type* t_ty){
	bb_type_ObjectType* t_objty=dynamic_cast<bb_type_ObjectType*>(t_ty);
	if((t_objty)!=0){
		return f_classDecl->m_ExtendsClass(t_objty->f_classDecl);
	}
	String t_op=String();
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		t_op=String(L"ToBool",6);
	}else{
		if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
			t_op=String(L"ToInt",5);
		}else{
			if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
				t_op=String(L"ToFloat",7);
			}else{
				if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
					t_op=String(L"ToString",8);
				}else{
					return 0;
				}
			}
		}
	}
	bb_decl_FuncDecl* t_fdecl=m_GetClass()->m_FindFuncDecl(t_op,Array<bb_expr_Expr* >(),1);
	return ((((t_fdecl)!=0) && t_fdecl->m_IsMethod() && ((t_fdecl->f_retType->m_EqualsType(t_ty))!=0))?1:0);
}
String bb_type_ObjectType::m_ToString(){
	return f_classDecl->m_ToString();
}
void bb_type_ObjectType::mark(){
	bb_type_Type::mark();
	gc_mark_q(f_classDecl);
}
bb_decl_ClassDecl::bb_decl_ClassDecl(){
	f_args=Array<String >();
	f_instanceof=0;
	f_instArgs=Array<bb_type_Type* >();
	f_implmentsAll=Array<bb_decl_ClassDecl* >();
	f_superTy=0;
	f_impltys=Array<bb_type_IdentType* >();
	f_objectType=0;
	f_instances=0;
	f_superClass=0;
	f_implments=Array<bb_decl_ClassDecl* >();
}
int bb_decl_ClassDecl::m_IsInterface(){
	return (((f_attrs&4096)!=0)?1:0);
}
String bb_decl_ClassDecl::m_ToString(){
	String t_t=String();
	if((f_args).Length()!=0){
		t_t=String(L",",1).Join(f_args);
	}else{
		if((f_instArgs).Length()!=0){
			Array<bb_type_Type* > t_=f_instArgs;
			int t_2=0;
			while(t_2<t_.Length()){
				bb_type_Type* t_arg=t_[t_2];
				t_2=t_2+1;
				if((t_t).Length()!=0){
					t_t=t_t+String(L",",1);
				}
				t_t=t_t+t_arg->m_ToString();
			}
		}
	}
	if((t_t).Length()!=0){
		t_t=String(L"<",1)+t_t+String(L">",1);
	}
	return f_ident+t_t;
}
bb_decl_FuncDecl* bb_decl_ClassDecl::m_FindFuncDecl2(String t_ident,Array<bb_expr_Expr* > t_args,int t_explicit){
	return bb_decl_ScopeDecl::m_FindFuncDecl(t_ident,t_args,t_explicit);
}
bb_decl_FuncDecl* bb_decl_ClassDecl::m_FindFuncDecl(String t_ident,Array<bb_expr_Expr* > t_args,int t_explicit){
	if(!((m_IsInterface())!=0)){
		return m_FindFuncDecl2(t_ident,t_args,t_explicit);
	}
	bb_decl_FuncDecl* t_fdecl=m_FindFuncDecl2(t_ident,t_args,1);
	Array<bb_decl_ClassDecl* > t_=f_implmentsAll;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ClassDecl* t_iface=t_[t_2];
		t_2=t_2+1;
		bb_decl_FuncDecl* t_decl=t_iface->m_FindFuncDecl2(t_ident,t_args,1);
		if(!((t_decl)!=0)){
			continue;
		}
		if((t_fdecl)!=0){
			if(t_fdecl->m_EqualsFunc(t_decl)){
				continue;
			}
			bb_config_Err(String(L"Unable to determine overload to use: ",37)+t_fdecl->m_ToString()+String(L" or ",4)+t_decl->m_ToString()+String(L".",1));
		}
		t_fdecl=t_decl;
	}
	if(((t_fdecl)!=0) || ((t_explicit)!=0)){
		return t_fdecl;
	}
	t_fdecl=m_FindFuncDecl2(t_ident,t_args,0);
	Array<bb_decl_ClassDecl* > t_3=f_implmentsAll;
	int t_4=0;
	while(t_4<t_3.Length()){
		bb_decl_ClassDecl* t_iface2=t_3[t_4];
		t_4=t_4+1;
		bb_decl_FuncDecl* t_decl2=t_iface2->m_FindFuncDecl2(t_ident,t_args,0);
		if(!((t_decl2)!=0)){
			continue;
		}
		if((t_fdecl)!=0){
			if(t_fdecl->m_EqualsFunc(t_decl2)){
				continue;
			}
			bb_config_Err(String(L"Unable to determine overload to use: ",37)+t_fdecl->m_ToString()+String(L" or ",4)+t_decl2->m_ToString()+String(L".",1));
		}
		t_fdecl=t_decl2;
	}
	return t_fdecl;
}
bb_decl_ClassDecl* bb_decl_ClassDecl::g_new(String t_ident,int t_attrs,Array<String > t_args,bb_type_IdentType* t_superTy,Array<bb_type_IdentType* > t_impls){
	bb_decl_ScopeDecl::g_new();
	this->f_ident=t_ident;
	this->f_attrs=t_attrs;
	gc_assign(this->f_args,t_args);
	gc_assign(this->f_superTy,t_superTy);
	gc_assign(this->f_impltys,t_impls);
	gc_assign(this->f_objectType,(new bb_type_ObjectType)->g_new(this));
	if((t_args).Length()!=0){
		gc_assign(f_instances,(new bb_list_List7)->g_new());
	}
	return this;
}
bb_decl_ClassDecl* bb_decl_ClassDecl::g_new2(){
	bb_decl_ScopeDecl::g_new();
	return this;
}
int bb_decl_ClassDecl::m_ExtendsObject(){
	return (((f_attrs&2)!=0)?1:0);
}
bb_decl_ClassDecl* bb_decl_ClassDecl::m_GenClassInstance(Array<bb_type_Type* > t_instArgs){
	if((f_instanceof)!=0){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	if(!((t_instArgs).Length()!=0)){
		if(!((f_args).Length()!=0)){
			return this;
		}
		bb_list_Enumerator3* t_=f_instances->m_ObjectEnumerator();
		while(t_->m_HasNext()){
			bb_decl_ClassDecl* t_inst=t_->m_NextObject();
			if(bb_decl__env->m_ClassScope()==t_inst){
				return t_inst;
			}
		}
	}
	if(f_args.Length()!=t_instArgs.Length()){
		bb_config_Err(String(L"Wrong number of type arguments for class ",41)+m_ToString());
	}
	bb_list_Enumerator3* t_2=f_instances->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_ClassDecl* t_inst2=t_2->m_NextObject();
		int t_equal=1;
		for(int t_i=0;t_i<f_args.Length();t_i=t_i+1){
			if(!((t_inst2->f_instArgs[t_i]->m_EqualsType(t_instArgs[t_i]))!=0)){
				t_equal=0;
				break;
			}
		}
		if((t_equal)!=0){
			return t_inst2;
		}
	}
	bb_decl_ClassDecl* t_inst3=(new bb_decl_ClassDecl)->g_new(f_ident,f_attrs,Array<String >(),f_superTy,f_impltys);
	t_inst3->f_attrs&=-1048577;
	t_inst3->f_munged=f_munged;
	t_inst3->f_errInfo=f_errInfo;
	gc_assign(t_inst3->f_scope,f_scope);
	gc_assign(t_inst3->f_instanceof,this);
	gc_assign(t_inst3->f_instArgs,t_instArgs);
	f_instances->m_AddLast7(t_inst3);
	for(int t_i2=0;t_i2<f_args.Length();t_i2=t_i2+1){
		t_inst3->m_InsertDecl((new bb_decl_AliasDecl)->g_new(f_args[t_i2],0,(t_instArgs[t_i2])));
	}
	bb_list_Enumerator2* t_3=f_decls->m_ObjectEnumerator();
	while(t_3->m_HasNext()){
		bb_decl_Decl* t_decl=t_3->m_NextObject();
		t_inst3->m_InsertDecl(t_decl->m_Copy());
	}
	return t_inst3;
}
int bb_decl_ClassDecl::m_IsFinalized(){
	return (((f_attrs&4)!=0)?1:0);
}
int bb_decl_ClassDecl::m_UpdateLiveMethods(){
	if((m_IsFinalized())!=0){
		return 0;
	}
	if((m_IsInterface())!=0){
		return 0;
	}
	if(!((f_superClass)!=0)){
		return 0;
	}
	int t_n=0;
	bb_list_Enumerator* t_=m_MethodDecls(String())->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_FuncDecl* t_decl=t_->m_NextObject();
		if((t_decl->m_IsSemanted())!=0){
			continue;
		}
		int t_live=0;
		bb_list_List2* t_unsem=(new bb_list_List2)->g_new();
		t_unsem->m_AddLast2(t_decl);
		bb_decl_ClassDecl* t_sclass=f_superClass;
		while((t_sclass)!=0){
			bb_list_Enumerator* t_2=t_sclass->m_MethodDecls(t_decl->f_ident)->m_ObjectEnumerator();
			while(t_2->m_HasNext()){
				bb_decl_FuncDecl* t_decl2=t_2->m_NextObject();
				if((t_decl2->m_IsSemanted())!=0){
					t_live=1;
				}else{
					t_unsem->m_AddLast2(t_decl2);
					if((t_decl2->m_IsExtern())!=0){
						t_live=1;
					}
					if((t_decl2->m_IsSemanted())!=0){
						t_live=1;
					}
				}
			}
			t_sclass=t_sclass->f_superClass;
		}
		if(!((t_live)!=0)){
			bb_decl_ClassDecl* t_cdecl=this;
			while((t_cdecl)!=0){
				Array<bb_decl_ClassDecl* > t_3=t_cdecl->f_implmentsAll;
				int t_4=0;
				while(t_4<t_3.Length()){
					bb_decl_ClassDecl* t_iface=t_3[t_4];
					t_4=t_4+1;
					bb_list_Enumerator* t_5=t_iface->m_MethodDecls(t_decl->f_ident)->m_ObjectEnumerator();
					while(t_5->m_HasNext()){
						bb_decl_FuncDecl* t_decl22=t_5->m_NextObject();
						if((t_decl22->m_IsSemanted())!=0){
							t_live=1;
						}else{
							t_unsem->m_AddLast2(t_decl22);
							if((t_decl22->m_IsExtern())!=0){
								t_live=1;
							}
							if((t_decl22->m_IsSemanted())!=0){
								t_live=1;
							}
						}
					}
				}
				t_cdecl=t_cdecl->f_superClass;
			}
		}
		if(!((t_live)!=0)){
			continue;
		}
		bb_list_Enumerator* t_6=t_unsem->m_ObjectEnumerator();
		while(t_6->m_HasNext()){
			bb_decl_FuncDecl* t_decl3=t_6->m_NextObject();
			t_decl3->m_Semant();
			t_n+=1;
		}
	}
	return t_n;
}
int bb_decl_ClassDecl::m_IsInstanced(){
	return (((f_attrs&1)!=0)?1:0);
}
int bb_decl_ClassDecl::m_FinalizeClass(){
	if((m_IsFinalized())!=0){
		return 0;
	}
	f_attrs|=4;
	if((m_IsInterface())!=0){
		return 0;
	}
	bb_config_PushErr(f_errInfo);
	bb_list_Enumerator2* t_=m_Semanted()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		bb_decl_FieldDecl* t_fdecl=dynamic_cast<bb_decl_FieldDecl*>(t_decl);
		if(!((t_fdecl)!=0)){
			continue;
		}
		bb_decl_ClassDecl* t_cdecl=f_superClass;
		while((t_cdecl)!=0){
			bb_list_Enumerator2* t_2=t_cdecl->m_Semanted()->m_ObjectEnumerator();
			while(t_2->m_HasNext()){
				bb_decl_Decl* t_decl2=t_2->m_NextObject();
				if(t_decl2->f_ident==t_fdecl->f_ident){
					bb_config__errInfo=t_fdecl->f_errInfo;
					bb_config_Err(String(L"Field '",7)+t_fdecl->f_ident+String(L"' in class ",11)+m_ToString()+String(L" overrides existing declaration in class ",41)+t_cdecl->m_ToString());
				}
			}
			t_cdecl=t_cdecl->f_superClass;
		}
	}
	if((m_IsAbstract())!=0){
		if((m_IsInstanced())!=0){
			bb_config_Err(String(L"Can't create instance of abstract class ",40)+m_ToString()+String(L".",1));
		}
	}else{
		bb_decl_ClassDecl* t_cdecl2=this;
		bb_list_List2* t_impls=(new bb_list_List2)->g_new();
		while(((t_cdecl2)!=0) && !((m_IsAbstract())!=0)){
			bb_list_Enumerator* t_3=t_cdecl2->m_SemantedMethods(String())->m_ObjectEnumerator();
			while(t_3->m_HasNext()){
				bb_decl_FuncDecl* t_decl3=t_3->m_NextObject();
				if((t_decl3->m_IsAbstract())!=0){
					int t_found=0;
					bb_list_Enumerator* t_4=t_impls->m_ObjectEnumerator();
					while(t_4->m_HasNext()){
						bb_decl_FuncDecl* t_decl22=t_4->m_NextObject();
						if(t_decl3->f_ident==t_decl22->f_ident && t_decl3->m_EqualsFunc(t_decl22)){
							t_found=1;
							break;
						}
					}
					if(!((t_found)!=0)){
						if((m_IsInstanced())!=0){
							bb_config_Err(String(L"Can't create instance of class ",31)+m_ToString()+String(L" due to abstract method ",24)+t_decl3->m_ToString()+String(L".",1));
						}
						f_attrs|=1024;
						break;
					}
				}else{
					t_impls->m_AddLast2(t_decl3);
				}
			}
			t_cdecl2=t_cdecl2->f_superClass;
		}
	}
	Array<bb_decl_ClassDecl* > t_5=f_implmentsAll;
	int t_6=0;
	while(t_6<t_5.Length()){
		bb_decl_ClassDecl* t_iface=t_5[t_6];
		t_6=t_6+1;
		bb_list_Enumerator* t_7=t_iface->m_SemantedMethods(String())->m_ObjectEnumerator();
		while(t_7->m_HasNext()){
			bb_decl_FuncDecl* t_decl4=t_7->m_NextObject();
			int t_found2=0;
			bb_list_Enumerator* t_8=m_SemantedMethods(t_decl4->f_ident)->m_ObjectEnumerator();
			while(t_8->m_HasNext()){
				bb_decl_FuncDecl* t_decl23=t_8->m_NextObject();
				if(t_decl4->m_EqualsFunc(t_decl23)){
					if((t_decl23->f_munged).Length()!=0){
						bb_config_Err(String(L"Extern methods cannot be used to implement interface methods.",61));
					}
					t_found2=1;
				}
			}
			if(!((t_found2)!=0)){
				bb_config_Err(t_decl4->m_ToString()+String(L" must be implemented by class ",30)+m_ToString());
			}
		}
	}
	bb_config_PopErr();
	return 0;
}
bb_decl_ClassDecl* bb_decl_ClassDecl::g_nullObjectClass;
int bb_decl_ClassDecl::m_ExtendsClass(bb_decl_ClassDecl* t_cdecl){
	if(this==bb_decl_ClassDecl::g_nullObjectClass){
		return 1;
	}
	bb_decl_ClassDecl* t_tdecl=this;
	while((t_tdecl)!=0){
		if(t_tdecl==t_cdecl){
			return 1;
		}
		if((t_cdecl->m_IsInterface())!=0){
			Array<bb_decl_ClassDecl* > t_=t_tdecl->f_implmentsAll;
			int t_2=0;
			while(t_2<t_.Length()){
				bb_decl_ClassDecl* t_iface=t_[t_2];
				t_2=t_2+1;
				if(t_iface==t_cdecl){
					return 1;
				}
			}
		}
		t_tdecl=t_tdecl->f_superClass;
	}
	return 0;
}
bb_decl_Decl* bb_decl_ClassDecl::m_OnCopy(){
	bb_config_InternalErr(String(L"Internal error",14));
	return 0;
}
Object* bb_decl_ClassDecl::m_GetDecl2(String t_ident){
	return bb_decl_ScopeDecl::m_GetDecl(t_ident);
}
Object* bb_decl_ClassDecl::m_GetDecl(String t_ident){
	bb_decl_ClassDecl* t_cdecl=this;
	while((t_cdecl)!=0){
		Object* t_decl=t_cdecl->m_GetDecl2(t_ident);
		if((t_decl)!=0){
			return t_decl;
		}
		t_cdecl=t_cdecl->f_superClass;
	}
	return 0;
}
int bb_decl_ClassDecl::m_IsThrowable(){
	return (((f_attrs&8192)!=0)?1:0);
}
int bb_decl_ClassDecl::m_OnSemant(){
	if((f_args).Length()!=0){
		return 0;
	}
	bb_decl_PushEnv(this);
	if((f_superTy)!=0){
		gc_assign(f_superClass,f_superTy->m_SemantClass());
		if((f_superClass->m_IsFinal())!=0){
			bb_config_Err(String(L"Cannot extend final class.",26));
		}
		if((f_superClass->m_IsInterface())!=0){
			bb_config_Err(String(L"Cannot extend an interface.",27));
		}
		if(f_munged==String(L"ThrowableObject",15) || ((f_superClass->m_IsThrowable())!=0)){
			f_attrs|=8192;
		}
		if((f_superClass->m_ExtendsObject())!=0){
			f_attrs|=2;
		}
	}else{
		if(f_munged==String(L"Object",6)){
			f_attrs|=2;
		}
	}
	Array<bb_decl_ClassDecl* > t_impls=Array<bb_decl_ClassDecl* >(f_impltys.Length());
	bb_stack_Stack7* t_implsall=(new bb_stack_Stack7)->g_new();
	for(int t_i=0;t_i<f_impltys.Length();t_i=t_i+1){
		bb_decl_ClassDecl* t_cdecl=f_impltys[t_i]->m_SemantClass();
		if(!((t_cdecl->m_IsInterface())!=0)){
			bb_config_Err(t_cdecl->m_ToString()+String(L" is a class, not an interface.",30));
		}
		for(int t_j=0;t_j<t_i;t_j=t_j+1){
			if(t_impls[t_j]==t_cdecl){
				bb_config_Err(String(L"Duplicate interface ",20)+t_cdecl->m_ToString()+String(L".",1));
			}
		}
		gc_assign(t_impls[t_i],t_cdecl);
		t_implsall->m_Push19(t_cdecl);
		Array<bb_decl_ClassDecl* > t_=t_cdecl->f_implmentsAll;
		int t_2=0;
		while(t_2<t_.Length()){
			bb_decl_ClassDecl* t_tdecl=t_[t_2];
			t_2=t_2+1;
			t_implsall->m_Push19(t_tdecl);
		}
	}
	gc_assign(f_implmentsAll,Array<bb_decl_ClassDecl* >(t_implsall->m_Length()));
	for(int t_i2=0;t_i2<t_implsall->m_Length();t_i2=t_i2+1){
		gc_assign(f_implmentsAll[t_i2],t_implsall->m_Get2(t_i2));
	}
	gc_assign(f_implments,t_impls);
	bb_decl_PopEnv();
	if(!((m_IsAbstract())!=0)){
		bb_list_Enumerator2* t_3=f_decls->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl=t_3->m_NextObject();
			bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
			if(((t_fdecl)!=0) && ((t_fdecl->m_IsAbstract())!=0)){
				f_attrs|=1024;
				break;
			}
		}
	}
	if(!((m_IsExtern())!=0) && !((m_IsInterface())!=0)){
		bb_decl_FuncDecl* t_fdecl2=0;
		bb_list_Enumerator* t_4=m_FuncDecls(String())->m_ObjectEnumerator();
		while(t_4->m_HasNext()){
			bb_decl_FuncDecl* t_decl2=t_4->m_NextObject();
			if(!t_decl2->m_IsCtor()){
				continue;
			}
			int t_nargs=0;
			Array<bb_decl_ArgDecl* > t_5=t_decl2->f_argDecls;
			int t_6=0;
			while(t_6<t_5.Length()){
				bb_decl_ArgDecl* t_arg=t_5[t_6];
				t_6=t_6+1;
				if(!((t_arg->f_init)!=0)){
					t_nargs+=1;
				}
			}
			if((t_nargs)!=0){
				continue;
			}
			t_fdecl2=t_decl2;
			break;
		}
		if(!((t_fdecl2)!=0)){
			t_fdecl2=(new bb_decl_FuncDecl)->g_new(String(L"new",3),2,(f_objectType),Array<bb_decl_ArgDecl* >());
			t_fdecl2->m_AddStmt((new bb_stmt_ReturnStmt)->g_new(0));
			m_InsertDecl(t_fdecl2);
		}
	}
	m_AppScope()->f_semantedClasses->m_AddLast7(this);
	return 0;
}
void bb_decl_ClassDecl::mark(){
	bb_decl_ScopeDecl::mark();
	gc_mark_q(f_args);
	gc_mark_q(f_instanceof);
	gc_mark_q(f_instArgs);
	gc_mark_q(f_implmentsAll);
	gc_mark_q(f_superTy);
	gc_mark_q(f_impltys);
	gc_mark_q(f_objectType);
	gc_mark_q(f_instances);
	gc_mark_q(f_superClass);
	gc_mark_q(f_implments);
}
bb_decl_AliasDecl::bb_decl_AliasDecl(){
	f_decl=0;
}
bb_decl_AliasDecl* bb_decl_AliasDecl::g_new(String t_ident,int t_attrs,Object* t_decl){
	bb_decl_Decl::g_new();
	this->f_ident=t_ident;
	this->f_attrs=t_attrs;
	gc_assign(this->f_decl,t_decl);
	return this;
}
bb_decl_AliasDecl* bb_decl_AliasDecl::g_new2(){
	bb_decl_Decl::g_new();
	return this;
}
bb_decl_Decl* bb_decl_AliasDecl::m_OnCopy(){
	return ((new bb_decl_AliasDecl)->g_new(f_ident,f_attrs,f_decl));
}
int bb_decl_AliasDecl::m_OnSemant(){
	return 0;
}
void bb_decl_AliasDecl::mark(){
	bb_decl_Decl::mark();
	gc_mark_q(f_decl);
}
bb_list_Enumerator::bb_list_Enumerator(){
	f__list=0;
	f__curr=0;
}
bb_list_Enumerator* bb_list_Enumerator::g_new(bb_list_List2* t_list){
	gc_assign(f__list,t_list);
	gc_assign(f__curr,t_list->f__head->f__succ);
	return this;
}
bb_list_Enumerator* bb_list_Enumerator::g_new2(){
	return this;
}
bool bb_list_Enumerator::m_HasNext(){
	while(f__curr->f__succ->f__pred!=f__curr){
		gc_assign(f__curr,f__curr->f__succ);
	}
	return f__curr!=f__list->f__head;
}
bb_decl_FuncDecl* bb_list_Enumerator::m_NextObject(){
	bb_decl_FuncDecl* t_data=f__curr->f__data;
	gc_assign(f__curr,f__curr->f__succ);
	return t_data;
}
void bb_list_Enumerator::mark(){
	Object::mark();
	gc_mark_q(f__list);
	gc_mark_q(f__curr);
}
bb_list_List5::bb_list_List5(){
	f__head=((new bb_list_HeadNode5)->g_new());
}
bb_list_List5* bb_list_List5::g_new(){
	return this;
}
bb_list_Node5* bb_list_List5::m_AddLast5(String t_data){
	return (new bb_list_Node5)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List5* bb_list_List5::g_new2(Array<String > t_data){
	Array<String > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		String t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast5(t_t);
	}
	return this;
}
String bb_list_List5::m_RemoveLast(){
	String t_data=f__head->m_PrevNode()->f__data;
	f__head->f__pred->m_Remove();
	return t_data;
}
bb_list_Enumerator4* bb_list_List5::m_ObjectEnumerator(){
	return (new bb_list_Enumerator4)->g_new(this);
}
bool bb_list_List5::m_IsEmpty(){
	return f__head->f__succ==f__head;
}
String bb_list_List5::m_RemoveFirst(){
	String t_data=f__head->m_NextNode()->f__data;
	f__head->f__succ->m_Remove();
	return t_data;
}
int bb_list_List5::m_Count(){
	int t_n=0;
	bb_list_Node5* t_node=f__head->f__succ;
	while(t_node!=f__head){
		t_node=t_node->f__succ;
		t_n+=1;
	}
	return t_n;
}
Array<String > bb_list_List5::m_ToArray(){
	Array<String > t_arr=Array<String >(m_Count());
	int t_i=0;
	bb_list_Enumerator4* t_=this->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		String t_t=t_->m_NextObject();
		t_arr[t_i]=t_t;
		t_i+=1;
	}
	return t_arr;
}
int bb_list_List5::m_Clear(){
	gc_assign(f__head->f__succ,f__head);
	gc_assign(f__head->f__pred,f__head);
	return 0;
}
void bb_list_List5::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_list_StringList::bb_list_StringList(){
}
bb_list_StringList* bb_list_StringList::g_new(){
	bb_list_List5::g_new();
	return this;
}
String bb_list_StringList::m_Join(String t_separator){
	return t_separator.Join(m_ToArray());
}
void bb_list_StringList::mark(){
	bb_list_List5::mark();
}
bb_list_Node5::bb_list_Node5(){
	f__succ=0;
	f__pred=0;
	f__data=String();
}
bb_list_Node5* bb_list_Node5::g_new(bb_list_Node5* t_succ,bb_list_Node5* t_pred,String t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	f__data=t_data;
	return this;
}
bb_list_Node5* bb_list_Node5::g_new2(){
	return this;
}
bb_list_Node5* bb_list_Node5::m_GetNode(){
	return this;
}
bb_list_Node5* bb_list_Node5::m_PrevNode(){
	return f__pred->m_GetNode();
}
int bb_list_Node5::m_Remove(){
	gc_assign(f__succ->f__pred,f__pred);
	gc_assign(f__pred->f__succ,f__succ);
	return 0;
}
bb_list_Node5* bb_list_Node5::m_NextNode(){
	return f__succ->m_GetNode();
}
void bb_list_Node5::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
}
bb_list_HeadNode5::bb_list_HeadNode5(){
}
bb_list_HeadNode5* bb_list_HeadNode5::g_new(){
	bb_list_Node5::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
bb_list_Node5* bb_list_HeadNode5::m_GetNode(){
	return 0;
}
void bb_list_HeadNode5::mark(){
	bb_list_Node5::mark();
}
bb_list_StringList* bb_config__errStack;
int bb_config_PushErr(String t_errInfo){
	bb_config__errStack->m_AddLast5(bb_config__errInfo);
	bb_config__errInfo=t_errInfo;
	return 0;
}
bb_decl_VarDecl::bb_decl_VarDecl(){
}
bb_decl_VarDecl* bb_decl_VarDecl::g_new(){
	bb_decl_ValDecl::g_new();
	return this;
}
void bb_decl_VarDecl::mark(){
	bb_decl_ValDecl::mark();
}
bb_decl_GlobalDecl::bb_decl_GlobalDecl(){
}
bb_decl_GlobalDecl* bb_decl_GlobalDecl::g_new(String t_ident,int t_attrs,bb_type_Type* t_type,bb_expr_Expr* t_init){
	bb_decl_VarDecl::g_new();
	this->f_ident=t_ident;
	this->f_attrs=t_attrs;
	gc_assign(this->f_type,t_type);
	gc_assign(this->f_init,t_init);
	return this;
}
bb_decl_GlobalDecl* bb_decl_GlobalDecl::g_new2(){
	bb_decl_VarDecl::g_new();
	return this;
}
String bb_decl_GlobalDecl::m_ToString(){
	return String(L"Global ",7)+bb_decl_ValDecl::m_ToString();
}
bb_decl_Decl* bb_decl_GlobalDecl::m_OnCopy(){
	return ((new bb_decl_GlobalDecl)->g_new(f_ident,f_attrs,f_type,m_CopyInit()));
}
void bb_decl_GlobalDecl::mark(){
	bb_decl_VarDecl::mark();
}
bb_list_List6::bb_list_List6(){
	f__head=((new bb_list_HeadNode6)->g_new());
}
bb_list_List6* bb_list_List6::g_new(){
	return this;
}
bb_list_Node6* bb_list_List6::m_AddLast6(bb_decl_GlobalDecl* t_data){
	return (new bb_list_Node6)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List6* bb_list_List6::g_new2(Array<bb_decl_GlobalDecl* > t_data){
	Array<bb_decl_GlobalDecl* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_GlobalDecl* t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast6(t_t);
	}
	return this;
}
bb_list_Enumerator6* bb_list_List6::m_ObjectEnumerator(){
	return (new bb_list_Enumerator6)->g_new(this);
}
void bb_list_List6::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_list_Node6::bb_list_Node6(){
	f__succ=0;
	f__pred=0;
	f__data=0;
}
bb_list_Node6* bb_list_Node6::g_new(bb_list_Node6* t_succ,bb_list_Node6* t_pred,bb_decl_GlobalDecl* t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	gc_assign(f__data,t_data);
	return this;
}
bb_list_Node6* bb_list_Node6::g_new2(){
	return this;
}
void bb_list_Node6::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
	gc_mark_q(f__data);
}
bb_list_HeadNode6::bb_list_HeadNode6(){
}
bb_list_HeadNode6* bb_list_HeadNode6::g_new(){
	bb_list_Node6::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
void bb_list_HeadNode6::mark(){
	bb_list_Node6::mark();
}
int bb_decl_PopEnv(){
	if(bb_decl__envStack->m_IsEmpty()){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	gc_assign(bb_decl__env,bb_decl__envStack->m_RemoveLast());
	return 0;
}
int bb_config_PopErr(){
	bb_config__errInfo=bb_config__errStack->m_RemoveLast();
	return 0;
}
bb_decl_LocalDecl::bb_decl_LocalDecl(){
}
String bb_decl_LocalDecl::m_ToString(){
	return String(L"Local ",6)+bb_decl_ValDecl::m_ToString();
}
bb_decl_LocalDecl* bb_decl_LocalDecl::g_new(String t_ident,int t_attrs,bb_type_Type* t_type,bb_expr_Expr* t_init){
	bb_decl_VarDecl::g_new();
	this->f_ident=t_ident;
	this->f_attrs=t_attrs;
	gc_assign(this->f_type,t_type);
	gc_assign(this->f_init,t_init);
	return this;
}
bb_decl_LocalDecl* bb_decl_LocalDecl::g_new2(){
	bb_decl_VarDecl::g_new();
	return this;
}
bb_decl_Decl* bb_decl_LocalDecl::m_OnCopy(){
	return ((new bb_decl_LocalDecl)->g_new(f_ident,f_attrs,f_type,m_CopyInit()));
}
void bb_decl_LocalDecl::mark(){
	bb_decl_VarDecl::mark();
}
bb_decl_ArgDecl::bb_decl_ArgDecl(){
}
String bb_decl_ArgDecl::m_ToString(){
	return bb_decl_LocalDecl::m_ToString();
}
bb_decl_ArgDecl* bb_decl_ArgDecl::g_new(String t_ident,int t_attrs,bb_type_Type* t_type,bb_expr_Expr* t_init){
	bb_decl_LocalDecl::g_new2();
	this->f_ident=t_ident;
	this->f_attrs=t_attrs;
	gc_assign(this->f_type,t_type);
	gc_assign(this->f_init,t_init);
	return this;
}
bb_decl_ArgDecl* bb_decl_ArgDecl::g_new2(){
	bb_decl_LocalDecl::g_new2();
	return this;
}
bb_decl_Decl* bb_decl_ArgDecl::m_OnCopy(){
	return ((new bb_decl_ArgDecl)->g_new(f_ident,f_attrs,f_type,m_CopyInit()));
}
void bb_decl_ArgDecl::mark(){
	bb_decl_LocalDecl::mark();
}
bb_expr_InvokeMemberExpr::bb_expr_InvokeMemberExpr(){
	f_expr=0;
	f_decl=0;
	f_args=Array<bb_expr_Expr* >();
	f_isResize=0;
}
bb_expr_InvokeMemberExpr* bb_expr_InvokeMemberExpr::g_new(bb_expr_Expr* t_expr,bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args){
	bb_expr_Expr::g_new();
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_decl,t_decl);
	gc_assign(this->f_args,t_args);
	return this;
}
bb_expr_InvokeMemberExpr* bb_expr_InvokeMemberExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_InvokeMemberExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_exprType,f_decl->f_retType);
	gc_assign(f_args,m_CastArgs(f_args,f_decl));
	if(((dynamic_cast<bb_type_ArrayType*>(f_exprType))!=0) && ((dynamic_cast<bb_type_VoidType*>(dynamic_cast<bb_type_ArrayType*>(f_exprType)->f_elemType))!=0)){
		f_isResize=1;
		gc_assign(f_exprType,f_expr->f_exprType);
	}
	return (this);
}
String bb_expr_InvokeMemberExpr::m_ToString(){
	String t_t=String(L"InvokeMemberExpr(",17)+f_expr->m_ToString()+String(L",",1)+f_decl->m_ToString();
	Array<bb_expr_Expr* > t_=f_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_arg=t_[t_2];
		t_2=t_2+1;
		t_t=t_t+(String(L",",1)+t_arg->m_ToString());
	}
	return t_t+String(L")",1);
}
String bb_expr_InvokeMemberExpr::m_Trans(){
	if((f_isResize)!=0){
		return bb_translator__trans->m_TransInvokeMemberExpr(this);
	}
	return bb_translator__trans->m_TransInvokeMemberExpr(this);
}
String bb_expr_InvokeMemberExpr::m_TransStmt(){
	return bb_translator__trans->m_TransInvokeMemberExpr(this);
}
void bb_expr_InvokeMemberExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_expr);
	gc_mark_q(f_decl);
	gc_mark_q(f_args);
}
String bb_preprocessor_Eval(String t_source,bb_type_Type* t_ty){
	bb_decl_ScopeDecl* t_env=(new bb_decl_ScopeDecl)->g_new();
	bb_map_NodeEnumerator* t_=bb_config__cfgVars->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_map_Node* t_kv=t_->m_NextObject();
		t_env->m_InsertDecl((new bb_decl_ConstDecl)->g_new(t_kv->m_Key(),0,(bb_type_Type::g_stringType),((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_stringType),t_kv->m_Value()))));
	}
	bb_decl_PushEnv(t_env);
	bb_toker_Toker* t_toker=(new bb_toker_Toker)->g_new(String(),t_source);
	bb_parser_Parser* t_parser=(new bb_parser_Parser)->g_new(t_toker,0,0,0);
	bb_expr_Expr* t_expr=t_parser->m_ParseExpr()->m_Semant();
	String t_val=String();
	if(((dynamic_cast<bb_type_StringType*>(t_ty))!=0) && ((dynamic_cast<bb_type_BoolType*>(t_expr->f_exprType))!=0)){
		t_val=t_expr->m_Eval();
		if((t_val).Length()!=0){
			t_val=String(L"1",1);
		}else{
			t_val=String(L"0",1);
		}
	}else{
		if(((dynamic_cast<bb_type_BoolType*>(t_ty))!=0) && ((dynamic_cast<bb_type_StringType*>(t_expr->f_exprType))!=0)){
			t_val=t_expr->m_Eval();
			if(((t_val).Length()!=0) && t_val!=String(L"0",1)){
				t_val=String(L"1",1);
			}else{
				t_val=String(L"0",1);
			}
		}else{
			if((t_ty)!=0){
				t_expr=t_expr->m_Cast(t_ty,0);
			}
			t_val=t_expr->m_Eval();
		}
	}
	bb_decl_PopEnv();
	return t_val;
}
String bb_preprocessor_Eval2(bb_toker_Toker* t_toker,bb_type_Type* t_type){
	bb_stack_StringStack* t_buf=(new bb_stack_StringStack)->g_new();
	while(((t_toker->m_Toke()).Length()!=0) && t_toker->m_Toke()!=String(L"\n",1) && t_toker->m_TokeType()!=9){
		t_buf->m_Push(t_toker->m_Toke());
		t_toker->m_NextToke();
	}
	return bb_preprocessor_Eval(t_buf->m_Join(String()),t_type);
}
int bb_math_Min(int t_x,int t_y){
	if(t_x<t_y){
		return t_x;
	}
	return t_y;
}
Float bb_math_Min2(Float t_x,Float t_y){
	if(t_x<t_y){
		return t_x;
	}
	return t_y;
}
String bb_config_EvalCfgTags(String t_cfg){
	int t_i=0;
	do{
		t_i=t_cfg.Find(String(L"${",2),0);
		if(t_i==-1){
			return t_cfg;
		}
		int t_e=t_cfg.Find(String(L"}",1),t_i+2);
		if(t_e==-1){
			return t_cfg;
		}
		String t_key=t_cfg.Slice(t_i+2,t_e);
		if(!bb_config__cfgVars->m_Contains(t_key)){
			t_i=t_e+1;
			continue;
		}
		String t_t=bb_config__cfgVars->m_Get(t_key);
		bb_config__cfgVars->m_Set(t_key,String());
		String t_v=bb_config_EvalCfgTags(t_t);
		bb_config__cfgVars->m_Set(t_key,t_t);
		t_cfg=t_cfg.Slice(0,t_i)+t_v+t_cfg.Slice(t_e+1);
		t_i+=t_v.Length();
	}while(!(false));
}
String bb_preprocessor_PreProcess(String t_path){
	int t_cnest=0;
	int t_ifnest=0;
	int t_line=0;
	bb_stack_StringStack* t_source=(new bb_stack_StringStack)->g_new();
	bb_toker_Toker* t_toker=(new bb_toker_Toker)->g_new(t_path,LoadString(t_path));
	t_toker->m_NextToke();
	bb_config_SetCfgVar(String(L"CD",2),bb_os_ExtractDir(RealPath(t_path)));
	do{
		if((t_line)!=0){
			t_source->m_Push(String(L"\n",1));
			while(((t_toker->m_Toke()).Length()!=0) && t_toker->m_Toke()!=String(L"\n",1) && t_toker->m_TokeType()!=9){
				t_toker->m_NextToke();
			}
			if(!((t_toker->m_Toke()).Length()!=0)){
				break;
			}
			t_toker->m_NextToke();
		}
		t_line+=1;
		bb_config__errInfo=t_toker->m_Path()+String(L"<",1)+String(t_toker->m_Line())+String(L">",1);
		if(t_toker->m_TokeType()==1){
			t_toker->m_NextToke();
		}
		if(t_toker->m_Toke()!=String(L"#",1)){
			if(t_cnest==t_ifnest){
				String t_line2=String();
				while(((t_toker->m_Toke()).Length()!=0) && t_toker->m_Toke()!=String(L"\n",1) && t_toker->m_TokeType()!=9){
					String t_toke=t_toker->m_Toke();
					t_line2=t_line2+t_toke;
					t_toker->m_NextToke();
				}
				if((t_line2).Length()!=0){
					t_source->m_Push(t_line2);
				}
			}
			continue;
		}
		String t_toke2=t_toker->m_NextToke();
		if(t_toker->m_TokeType()==1){
			t_toke2=t_toker->m_NextToke();
		}
		String t_stm=t_toke2.ToLower();
		int t_ty=t_toker->m_TokeType();
		t_toker->m_NextToke();
		if(t_stm==String(L"end",3) || t_stm==String(L"else",4)){
			if(t_toker->m_TokeType()==1){
				t_toker->m_NextToke();
			}
			if(t_toker->m_Toke().ToLower()==String(L"if",2)){
				t_toker->m_NextToke();
				t_stm=t_stm+String(L"if",2);
			}
		}
		String t_=t_stm;
		if(t_==String(L"if",2)){
			if(t_cnest==t_ifnest){
				if((bb_preprocessor_Eval2(t_toker,(bb_type_Type::g_boolType))).Length()!=0){
					t_cnest+=1;
				}
			}
			t_ifnest+=1;
		}else{
			if(t_==String(L"rem",3)){
				t_ifnest+=1;
			}else{
				if(t_==String(L"else",4)){
					if(!((t_ifnest)!=0)){
						bb_config_Err(String(L"#Else without #If",17));
					}
					if(t_cnest==t_ifnest){
						t_cnest-=1;
						t_ifnest|=65536;
					}else{
						if(t_cnest==t_ifnest-1){
							t_cnest+=1;
						}
					}
				}else{
					if(t_==String(L"elseif",6)){
						if(!((t_ifnest)!=0)){
							bb_config_Err(String(L"#ElseIf without #If",19));
						}
						if(t_cnest==t_ifnest){
							t_cnest-=1;
							t_ifnest|=65536;
						}else{
							if(t_cnest==t_ifnest-1){
								if((bb_preprocessor_Eval2(t_toker,(bb_type_Type::g_boolType))).Length()!=0){
									t_cnest+=1;
								}
							}
						}
					}else{
						if(t_==String(L"end",3) || t_==String(L"endif",5)){
							if(!((t_ifnest)!=0)){
								bb_config_Err(String(L"#End without #If",16));
							}
							t_ifnest&=65535;
							t_ifnest-=1;
							t_cnest=bb_math_Min(t_cnest,t_ifnest);
						}else{
							if(t_==String(L"print",5)){
								if(t_cnest==t_ifnest){
									Print(bb_config_EvalCfgTags(bb_preprocessor_Eval2(t_toker,(bb_type_Type::g_stringType))));
								}
							}else{
								if(t_==String(L"error",5)){
									if(t_cnest==t_ifnest){
										bb_config_Err(bb_config_EvalCfgTags(bb_preprocessor_Eval2(t_toker,(bb_type_Type::g_stringType))));
									}
								}else{
									if(t_cnest==t_ifnest){
										if(t_ty==2){
											if(t_toker->m_TokeType()==1){
												t_toker->m_NextToke();
											}
											String t_op=t_toker->m_Toke();
											if(t_op==String(L"=",1) || t_op==String(L"+=",2)){
												String t_2=t_toke2;
												if(t_2==String(L"HOST",4) || t_2==String(L"LANG",4) || t_2==String(L"CONFIG",6) || t_2==String(L"TARGET",6) || t_2==String(L"SAFEMODE",8)){
													bb_config_Err(String(L"App config var '",16)+t_toke2+String(L"' cannot be modified",20));
												}
												t_toker->m_NextToke();
												String t_val=bb_config_EvalCfgTags(bb_preprocessor_Eval2(t_toker,(bb_type_Type::g_stringType)));
												if(t_op==String(L"=",1)){
													if(!((bb_config_GetCfgVar(t_toke2)).Length()!=0)){
														bb_config_SetCfgVar(t_toke2,t_val);
													}
												}else{
													if(t_op==String(L"+=",2)){
														String t_var=bb_config_GetCfgVar(t_toke2);
														if(((t_var).Length()!=0) && !t_val.StartsWith(String(L";",1))){
															t_val=String(L";",1)+t_val;
														}
														bb_config_SetCfgVar(t_toke2,t_var+t_val);
													}
												}
											}else{
												bb_config_Err(String(L"Syntax error - expecting assignment",35));
											}
										}else{
											bb_config_Err(String(L"Unrecognized preprocessor directive '",37)+t_toke2+String(L"'",1));
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}while(!(false));
	bb_config_SetCfgVar(String(L"CD",2),String());
	return t_source->m_Join(String());
}
String bb_os_StripExt(String t_path){
	int t_i=t_path.FindLast(String(L".",1));
	if(t_i!=-1 && t_path.Find(String(L"/",1),t_i+1)==-1 && t_path.Find(String(L"\\",1),t_i+1)==-1){
		return t_path.Slice(0,t_i);
	}
	return t_path;
}
String bb_os_StripDir(String t_path){
	int t_i=t_path.FindLast(String(L"/",1));
	if(t_i==-1){
		t_i=t_path.FindLast(String(L"\\",1));
	}
	if(t_i!=-1){
		return t_path.Slice(t_i+1);
	}
	return t_path;
}
String bb_os_StripAll(String t_path){
	return bb_os_StripDir(bb_os_StripExt(t_path));
}
bb_map_Map3::bb_map_Map3(){
	f_root=0;
}
bb_map_Map3* bb_map_Map3::g_new(){
	return this;
}
int bb_map_Map3::m_RotateLeft3(bb_map_Node3* t_node){
	bb_map_Node3* t_child=t_node->f_right;
	gc_assign(t_node->f_right,t_child->f_left);
	if((t_child->f_left)!=0){
		gc_assign(t_child->f_left->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_left){
			gc_assign(t_node->f_parent->f_left,t_child);
		}else{
			gc_assign(t_node->f_parent->f_right,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_left,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map3::m_RotateRight3(bb_map_Node3* t_node){
	bb_map_Node3* t_child=t_node->f_left;
	gc_assign(t_node->f_left,t_child->f_right);
	if((t_child->f_right)!=0){
		gc_assign(t_child->f_right->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_right){
			gc_assign(t_node->f_parent->f_right,t_child);
		}else{
			gc_assign(t_node->f_parent->f_left,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_right,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map3::m_InsertFixup3(bb_map_Node3* t_node){
	while(((t_node->f_parent)!=0) && t_node->f_parent->f_color==-1 && ((t_node->f_parent->f_parent)!=0)){
		if(t_node->f_parent==t_node->f_parent->f_parent->f_left){
			bb_map_Node3* t_uncle=t_node->f_parent->f_parent->f_right;
			if(((t_uncle)!=0) && t_uncle->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle->f_color=1;
				t_uncle->f_parent->f_color=-1;
				t_node=t_uncle->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_right){
					t_node=t_node->f_parent;
					m_RotateLeft3(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateRight3(t_node->f_parent->f_parent);
			}
		}else{
			bb_map_Node3* t_uncle2=t_node->f_parent->f_parent->f_left;
			if(((t_uncle2)!=0) && t_uncle2->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle2->f_color=1;
				t_uncle2->f_parent->f_color=-1;
				t_node=t_uncle2->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_left){
					t_node=t_node->f_parent;
					m_RotateRight3(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateLeft3(t_node->f_parent->f_parent);
			}
		}
	}
	f_root->f_color=1;
	return 0;
}
bool bb_map_Map3::m_Set3(String t_key,bb_decl_ModuleDecl* t_value){
	bb_map_Node3* t_node=f_root;
	bb_map_Node3* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				gc_assign(t_node->f_value,t_value);
				return false;
			}
		}
	}
	t_node=(new bb_map_Node3)->g_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->f_right,t_node);
		}else{
			gc_assign(t_parent->f_left,t_node);
		}
		m_InsertFixup3(t_node);
	}else{
		gc_assign(f_root,t_node);
	}
	return true;
}
bool bb_map_Map3::m_Insert3(String t_key,bb_decl_ModuleDecl* t_value){
	return m_Set3(t_key,t_value);
}
bb_map_Node3* bb_map_Map3::m_FindNode(String t_key){
	bb_map_Node3* t_node=f_root;
	while((t_node)!=0){
		int t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool bb_map_Map3::m_Contains(String t_key){
	return m_FindNode(t_key)!=0;
}
bb_decl_ModuleDecl* bb_map_Map3::m_Get(String t_key){
	bb_map_Node3* t_node=m_FindNode(t_key);
	if((t_node)!=0){
		return t_node->f_value;
	}
	return 0;
}
bb_decl_ModuleDecl* bb_map_Map3::m_ValueForKey(String t_key){
	return m_Get(t_key);
}
bb_map_MapValues* bb_map_Map3::m_Values(){
	return (new bb_map_MapValues)->g_new(this);
}
bb_map_Node3* bb_map_Map3::m_FirstNode(){
	if(!((f_root)!=0)){
		return 0;
	}
	bb_map_Node3* t_node=f_root;
	while((t_node->f_left)!=0){
		t_node=t_node->f_left;
	}
	return t_node;
}
void bb_map_Map3::mark(){
	Object::mark();
	gc_mark_q(f_root);
}
bb_map_StringMap3::bb_map_StringMap3(){
}
bb_map_StringMap3* bb_map_StringMap3::g_new(){
	bb_map_Map3::g_new();
	return this;
}
int bb_map_StringMap3::m_Compare(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void bb_map_StringMap3::mark(){
	bb_map_Map3::mark();
}
bb_map_Node3::bb_map_Node3(){
	f_key=String();
	f_right=0;
	f_left=0;
	f_value=0;
	f_color=0;
	f_parent=0;
}
bb_map_Node3* bb_map_Node3::g_new(String t_key,bb_decl_ModuleDecl* t_value,int t_color,bb_map_Node3* t_parent){
	this->f_key=t_key;
	gc_assign(this->f_value,t_value);
	this->f_color=t_color;
	gc_assign(this->f_parent,t_parent);
	return this;
}
bb_map_Node3* bb_map_Node3::g_new2(){
	return this;
}
bb_map_Node3* bb_map_Node3::m_NextNode(){
	bb_map_Node3* t_node=0;
	if((f_right)!=0){
		t_node=f_right;
		while((t_node->f_left)!=0){
			t_node=t_node->f_left;
		}
		return t_node;
	}
	t_node=this;
	bb_map_Node3* t_parent=this->f_parent;
	while(((t_parent)!=0) && t_node==t_parent->f_right){
		t_node=t_parent;
		t_parent=t_parent->f_parent;
	}
	return t_parent;
}
void bb_map_Node3::mark(){
	Object::mark();
	gc_mark_q(f_right);
	gc_mark_q(f_left);
	gc_mark_q(f_value);
	gc_mark_q(f_parent);
}
String bb_parser_FILE_EXT;
bb_decl_ModuleDecl* bb_parser_ParseModule(String t_path,bb_decl_AppDecl* t_app){
	String t_source=bb_preprocessor_PreProcess(t_path);
	bb_toker_Toker* t_toker=(new bb_toker_Toker)->g_new(t_path,t_source);
	bb_parser_Parser* t_parser=(new bb_parser_Parser)->g_new(t_toker,t_app,0,0);
	t_parser->m_ParseMain();
	return t_parser->f__module;
}
bb_decl_FieldDecl::bb_decl_FieldDecl(){
}
bb_decl_FieldDecl* bb_decl_FieldDecl::g_new(String t_ident,int t_attrs,bb_type_Type* t_type,bb_expr_Expr* t_init){
	bb_decl_VarDecl::g_new();
	this->f_ident=t_ident;
	this->f_attrs=t_attrs;
	gc_assign(this->f_type,t_type);
	gc_assign(this->f_init,t_init);
	return this;
}
bb_decl_FieldDecl* bb_decl_FieldDecl::g_new2(){
	bb_decl_VarDecl::g_new();
	return this;
}
String bb_decl_FieldDecl::m_ToString(){
	return String(L"Field ",6)+bb_decl_ValDecl::m_ToString();
}
bb_decl_Decl* bb_decl_FieldDecl::m_OnCopy(){
	return ((new bb_decl_FieldDecl)->g_new(f_ident,f_attrs,f_type,m_CopyInit()));
}
void bb_decl_FieldDecl::mark(){
	bb_decl_VarDecl::mark();
}
bb_list_Enumerator2::bb_list_Enumerator2(){
	f__list=0;
	f__curr=0;
}
bb_list_Enumerator2* bb_list_Enumerator2::g_new(bb_list_List* t_list){
	gc_assign(f__list,t_list);
	gc_assign(f__curr,t_list->f__head->f__succ);
	return this;
}
bb_list_Enumerator2* bb_list_Enumerator2::g_new2(){
	return this;
}
bool bb_list_Enumerator2::m_HasNext(){
	while(f__curr->f__succ->f__pred!=f__curr){
		gc_assign(f__curr,f__curr->f__succ);
	}
	return f__curr!=f__list->f__head;
}
bb_decl_Decl* bb_list_Enumerator2::m_NextObject(){
	bb_decl_Decl* t_data=f__curr->f__data;
	gc_assign(f__curr,f__curr->f__succ);
	return t_data;
}
void bb_list_Enumerator2::mark(){
	Object::mark();
	gc_mark_q(f__list);
	gc_mark_q(f__curr);
}
bb_stack_Stack4::bb_stack_Stack4(){
	f_data=Array<bb_type_IdentType* >();
	f_length=0;
}
bb_stack_Stack4* bb_stack_Stack4::g_new(){
	return this;
}
bb_stack_Stack4* bb_stack_Stack4::g_new2(Array<bb_type_IdentType* > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack4::m_Push10(bb_type_IdentType* t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	gc_assign(f_data[f_length],t_value);
	f_length+=1;
	return 0;
}
int bb_stack_Stack4::m_Push11(Array<bb_type_IdentType* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push10(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack4::m_Push12(Array<bb_type_IdentType* > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push10(t_values[t_i]);
	}
	return 0;
}
Array<bb_type_IdentType* > bb_stack_Stack4::m_ToArray(){
	Array<bb_type_IdentType* > t_t=Array<bb_type_IdentType* >(f_length);
	for(int t_i=0;t_i<f_length;t_i=t_i+1){
		gc_assign(t_t[t_i],f_data[t_i]);
	}
	return t_t;
}
void bb_stack_Stack4::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
bb_list_List7::bb_list_List7(){
	f__head=((new bb_list_HeadNode7)->g_new());
}
bb_list_List7* bb_list_List7::g_new(){
	return this;
}
bb_list_Node7* bb_list_List7::m_AddLast7(bb_decl_ClassDecl* t_data){
	return (new bb_list_Node7)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List7* bb_list_List7::g_new2(Array<bb_decl_ClassDecl* > t_data){
	Array<bb_decl_ClassDecl* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ClassDecl* t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast7(t_t);
	}
	return this;
}
bb_list_Enumerator3* bb_list_List7::m_ObjectEnumerator(){
	return (new bb_list_Enumerator3)->g_new(this);
}
void bb_list_List7::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_list_Node7::bb_list_Node7(){
	f__succ=0;
	f__pred=0;
	f__data=0;
}
bb_list_Node7* bb_list_Node7::g_new(bb_list_Node7* t_succ,bb_list_Node7* t_pred,bb_decl_ClassDecl* t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	gc_assign(f__data,t_data);
	return this;
}
bb_list_Node7* bb_list_Node7::g_new2(){
	return this;
}
void bb_list_Node7::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
	gc_mark_q(f__data);
}
bb_list_HeadNode7::bb_list_HeadNode7(){
}
bb_list_HeadNode7* bb_list_HeadNode7::g_new(){
	bb_list_Node7::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
void bb_list_HeadNode7::mark(){
	bb_list_Node7::mark();
}
bb_stack_Stack5::bb_stack_Stack5(){
	f_data=Array<bb_decl_ArgDecl* >();
	f_length=0;
}
bb_stack_Stack5* bb_stack_Stack5::g_new(){
	return this;
}
bb_stack_Stack5* bb_stack_Stack5::g_new2(Array<bb_decl_ArgDecl* > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack5::m_Push13(bb_decl_ArgDecl* t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	gc_assign(f_data[f_length],t_value);
	f_length+=1;
	return 0;
}
int bb_stack_Stack5::m_Push14(Array<bb_decl_ArgDecl* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push13(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack5::m_Push15(Array<bb_decl_ArgDecl* > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push13(t_values[t_i]);
	}
	return 0;
}
Array<bb_decl_ArgDecl* > bb_stack_Stack5::m_ToArray(){
	Array<bb_decl_ArgDecl* > t_t=Array<bb_decl_ArgDecl* >(f_length);
	for(int t_i=0;t_i<f_length;t_i=t_i+1){
		gc_assign(t_t[t_i],f_data[t_i]);
	}
	return t_t;
}
void bb_stack_Stack5::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
bb_list_List8::bb_list_List8(){
	f__head=((new bb_list_HeadNode8)->g_new());
}
bb_list_List8* bb_list_List8::g_new(){
	return this;
}
bb_list_Node8* bb_list_List8::m_AddLast8(bb_decl_BlockDecl* t_data){
	return (new bb_list_Node8)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List8* bb_list_List8::g_new2(Array<bb_decl_BlockDecl* > t_data){
	Array<bb_decl_BlockDecl* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_BlockDecl* t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast8(t_t);
	}
	return this;
}
bb_decl_BlockDecl* bb_list_List8::m_RemoveLast(){
	bb_decl_BlockDecl* t_data=f__head->m_PrevNode()->f__data;
	f__head->f__pred->m_Remove();
	return t_data;
}
void bb_list_List8::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_list_Node8::bb_list_Node8(){
	f__succ=0;
	f__pred=0;
	f__data=0;
}
bb_list_Node8* bb_list_Node8::g_new(bb_list_Node8* t_succ,bb_list_Node8* t_pred,bb_decl_BlockDecl* t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	gc_assign(f__data,t_data);
	return this;
}
bb_list_Node8* bb_list_Node8::g_new2(){
	return this;
}
bb_list_Node8* bb_list_Node8::m_GetNode(){
	return this;
}
bb_list_Node8* bb_list_Node8::m_PrevNode(){
	return f__pred->m_GetNode();
}
int bb_list_Node8::m_Remove(){
	gc_assign(f__succ->f__pred,f__pred);
	gc_assign(f__pred->f__succ,f__succ);
	return 0;
}
void bb_list_Node8::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
	gc_mark_q(f__data);
}
bb_list_HeadNode8::bb_list_HeadNode8(){
}
bb_list_HeadNode8* bb_list_HeadNode8::g_new(){
	bb_list_Node8::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
bb_list_Node8* bb_list_HeadNode8::m_GetNode(){
	return 0;
}
void bb_list_HeadNode8::mark(){
	bb_list_Node8::mark();
}
bb_stmt_DeclStmt::bb_stmt_DeclStmt(){
	f_decl=0;
}
bb_stmt_DeclStmt* bb_stmt_DeclStmt::g_new(bb_decl_Decl* t_decl){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_decl,t_decl);
	return this;
}
bb_stmt_DeclStmt* bb_stmt_DeclStmt::g_new2(String t_id,bb_type_Type* t_ty,bb_expr_Expr* t_init){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_decl,((new bb_decl_LocalDecl)->g_new(t_id,0,t_ty,t_init)));
	return this;
}
bb_stmt_DeclStmt* bb_stmt_DeclStmt::g_new3(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_DeclStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_DeclStmt)->g_new(f_decl->m_Copy()));
}
int bb_stmt_DeclStmt::m_OnSemant(){
	f_decl->m_Semant();
	bb_decl__env->m_InsertDecl(f_decl);
	return 0;
}
String bb_stmt_DeclStmt::m_Trans(){
	return bb_translator__trans->m_TransDeclStmt(this);
}
void bb_stmt_DeclStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_decl);
}
bb_stmt_ReturnStmt::bb_stmt_ReturnStmt(){
	f_expr=0;
}
bb_stmt_ReturnStmt* bb_stmt_ReturnStmt::g_new(bb_expr_Expr* t_expr){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_expr,t_expr);
	return this;
}
bb_stmt_ReturnStmt* bb_stmt_ReturnStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_ReturnStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	if((f_expr)!=0){
		return ((new bb_stmt_ReturnStmt)->g_new(f_expr->m_Copy()));
	}
	return ((new bb_stmt_ReturnStmt)->g_new(0));
}
int bb_stmt_ReturnStmt::m_OnSemant(){
	bb_decl_FuncDecl* t_fdecl=bb_decl__env->m_FuncScope();
	if((f_expr)!=0){
		if(t_fdecl->m_IsCtor()){
			bb_config_Err(String(L"Constructors may not return a value.",36));
		}
		if((dynamic_cast<bb_type_VoidType*>(t_fdecl->f_retType))!=0){
			bb_config_Err(String(L"Void functions may not return a value.",38));
		}
		gc_assign(f_expr,f_expr->m_Semant2(t_fdecl->f_retType,0));
	}else{
		if(t_fdecl->m_IsCtor()){
			gc_assign(f_expr,((new bb_expr_SelfExpr)->g_new())->m_Semant());
		}else{
			if(!((dynamic_cast<bb_type_VoidType*>(t_fdecl->f_retType))!=0)){
				if((bb_decl__env->m_ModuleScope()->m_IsStrict())!=0){
					bb_config_Err(String(L"Missing return expression.",26));
				}
				gc_assign(f_expr,((new bb_expr_ConstExpr)->g_new(t_fdecl->f_retType,String()))->m_Semant());
			}
		}
	}
	return 0;
}
String bb_stmt_ReturnStmt::m_Trans(){
	return bb_translator__trans->m_TransReturnStmt(this);
}
void bb_stmt_ReturnStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_expr);
}
bb_stmt_BreakStmt::bb_stmt_BreakStmt(){
}
bb_stmt_BreakStmt* bb_stmt_BreakStmt::g_new(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_BreakStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_BreakStmt)->g_new());
}
int bb_stmt_BreakStmt::m_OnSemant(){
	if(!((bb_decl__loopnest)!=0)){
		bb_config_Err(String(L"Exit statement must appear inside a loop.",41));
	}
	return 0;
}
String bb_stmt_BreakStmt::m_Trans(){
	return bb_translator__trans->m_TransBreakStmt(this);
}
void bb_stmt_BreakStmt::mark(){
	bb_stmt_Stmt::mark();
}
bb_stmt_ContinueStmt::bb_stmt_ContinueStmt(){
}
bb_stmt_ContinueStmt* bb_stmt_ContinueStmt::g_new(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_ContinueStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_ContinueStmt)->g_new());
}
int bb_stmt_ContinueStmt::m_OnSemant(){
	if(!((bb_decl__loopnest)!=0)){
		bb_config_Err(String(L"Continue statement must appear inside a loop.",45));
	}
	return 0;
}
String bb_stmt_ContinueStmt::m_Trans(){
	return bb_translator__trans->m_TransContinueStmt(this);
}
void bb_stmt_ContinueStmt::mark(){
	bb_stmt_Stmt::mark();
}
bb_stmt_IfStmt::bb_stmt_IfStmt(){
	f_expr=0;
	f_thenBlock=0;
	f_elseBlock=0;
}
bb_stmt_IfStmt* bb_stmt_IfStmt::g_new(bb_expr_Expr* t_expr,bb_decl_BlockDecl* t_thenBlock,bb_decl_BlockDecl* t_elseBlock){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_thenBlock,t_thenBlock);
	gc_assign(this->f_elseBlock,t_elseBlock);
	return this;
}
bb_stmt_IfStmt* bb_stmt_IfStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_IfStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_IfStmt)->g_new(f_expr->m_Copy(),f_thenBlock->m_CopyBlock(t_scope),f_elseBlock->m_CopyBlock(t_scope)));
}
int bb_stmt_IfStmt::m_OnSemant(){
	gc_assign(f_expr,f_expr->m_Semant2((bb_type_Type::g_boolType),1));
	f_thenBlock->m_Semant();
	f_elseBlock->m_Semant();
	return 0;
}
String bb_stmt_IfStmt::m_Trans(){
	return bb_translator__trans->m_TransIfStmt(this);
}
void bb_stmt_IfStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_expr);
	gc_mark_q(f_thenBlock);
	gc_mark_q(f_elseBlock);
}
bb_stmt_WhileStmt::bb_stmt_WhileStmt(){
	f_expr=0;
	f_block=0;
}
bb_stmt_WhileStmt* bb_stmt_WhileStmt::g_new(bb_expr_Expr* t_expr,bb_decl_BlockDecl* t_block){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_block,t_block);
	return this;
}
bb_stmt_WhileStmt* bb_stmt_WhileStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_WhileStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_WhileStmt)->g_new(f_expr->m_Copy(),f_block->m_CopyBlock(t_scope)));
}
int bb_stmt_WhileStmt::m_OnSemant(){
	gc_assign(f_expr,f_expr->m_Semant2((bb_type_Type::g_boolType),1));
	bb_decl__loopnest+=1;
	f_block->m_Semant();
	bb_decl__loopnest-=1;
	return 0;
}
String bb_stmt_WhileStmt::m_Trans(){
	return bb_translator__trans->m_TransWhileStmt(this);
}
void bb_stmt_WhileStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_expr);
	gc_mark_q(f_block);
}
bb_stmt_RepeatStmt::bb_stmt_RepeatStmt(){
	f_block=0;
	f_expr=0;
}
bb_stmt_RepeatStmt* bb_stmt_RepeatStmt::g_new(bb_decl_BlockDecl* t_block,bb_expr_Expr* t_expr){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_block,t_block);
	gc_assign(this->f_expr,t_expr);
	return this;
}
bb_stmt_RepeatStmt* bb_stmt_RepeatStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_RepeatStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_RepeatStmt)->g_new(f_block->m_CopyBlock(t_scope),f_expr->m_Copy()));
}
int bb_stmt_RepeatStmt::m_OnSemant(){
	bb_decl__loopnest+=1;
	f_block->m_Semant();
	bb_decl__loopnest-=1;
	gc_assign(f_expr,f_expr->m_Semant2((bb_type_Type::g_boolType),1));
	return 0;
}
String bb_stmt_RepeatStmt::m_Trans(){
	return bb_translator__trans->m_TransRepeatStmt(this);
}
void bb_stmt_RepeatStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_block);
	gc_mark_q(f_expr);
}
bb_parser_ForEachinStmt::bb_parser_ForEachinStmt(){
	f_varid=String();
	f_varty=0;
	f_varlocal=0;
	f_expr=0;
	f_block=0;
}
bb_parser_ForEachinStmt* bb_parser_ForEachinStmt::g_new(String t_varid,bb_type_Type* t_varty,int t_varlocal,bb_expr_Expr* t_expr,bb_decl_BlockDecl* t_block){
	bb_stmt_Stmt::g_new();
	this->f_varid=t_varid;
	gc_assign(this->f_varty,t_varty);
	this->f_varlocal=t_varlocal;
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_block,t_block);
	return this;
}
bb_parser_ForEachinStmt* bb_parser_ForEachinStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_parser_ForEachinStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_parser_ForEachinStmt)->g_new(f_varid,f_varty,f_varlocal,f_expr->m_Copy(),f_block->m_CopyBlock(t_scope)));
}
int bb_parser_ForEachinStmt::m_OnSemant(){
	gc_assign(f_expr,f_expr->m_Semant());
	if(((dynamic_cast<bb_type_ArrayType*>(f_expr->f_exprType))!=0) || ((dynamic_cast<bb_type_StringType*>(f_expr->f_exprType))!=0)){
		bb_decl_LocalDecl* t_exprTmp=(new bb_decl_LocalDecl)->g_new(String(),0,0,f_expr);
		bb_decl_LocalDecl* t_indexTmp=(new bb_decl_LocalDecl)->g_new(String(),0,0,((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_intType),String(L"0",1))));
		bb_expr_Expr* t_lenExpr=((new bb_parser_IdentExpr)->g_new(String(L"Length",6),((new bb_expr_VarExpr)->g_new(t_exprTmp))));
		bb_expr_Expr* t_cmpExpr=((new bb_expr_BinaryCompareExpr)->g_new(String(L"<",1),((new bb_expr_VarExpr)->g_new(t_indexTmp)),t_lenExpr));
		bb_expr_Expr* t_indexExpr=((new bb_expr_IndexExpr)->g_new(((new bb_expr_VarExpr)->g_new(t_exprTmp)),((new bb_expr_VarExpr)->g_new(t_indexTmp))));
		bb_expr_Expr* t_addExpr=((new bb_expr_BinaryMathExpr)->g_new(String(L"+",1),((new bb_expr_VarExpr)->g_new(t_indexTmp)),((new bb_expr_ConstExpr)->g_new((bb_type_Type::g_intType),String(L"1",1)))));
		f_block->f_stmts->m_AddFirst((new bb_stmt_AssignStmt)->g_new(String(L"=",1),((new bb_expr_VarExpr)->g_new(t_indexTmp)),t_addExpr));
		if((f_varlocal)!=0){
			bb_decl_LocalDecl* t_varTmp=(new bb_decl_LocalDecl)->g_new(f_varid,0,f_varty,t_indexExpr);
			f_block->f_stmts->m_AddFirst((new bb_stmt_DeclStmt)->g_new(t_varTmp));
		}else{
			f_block->f_stmts->m_AddFirst((new bb_stmt_AssignStmt)->g_new(String(L"=",1),((new bb_parser_IdentExpr)->g_new(f_varid,0)),t_indexExpr));
		}
		bb_stmt_WhileStmt* t_whileStmt=(new bb_stmt_WhileStmt)->g_new(t_cmpExpr,f_block);
		gc_assign(f_block,(new bb_decl_BlockDecl)->g_new(f_block->f_scope));
		f_block->m_AddStmt((new bb_stmt_DeclStmt)->g_new(t_exprTmp));
		f_block->m_AddStmt((new bb_stmt_DeclStmt)->g_new(t_indexTmp));
		f_block->m_AddStmt(t_whileStmt);
	}else{
		if((dynamic_cast<bb_type_ObjectType*>(f_expr->f_exprType))!=0){
			bb_expr_Expr* t_enumerInit=((new bb_parser_FuncCallExpr)->g_new(((new bb_parser_IdentExpr)->g_new(String(L"ObjectEnumerator",16),f_expr)),Array<bb_expr_Expr* >()));
			bb_decl_LocalDecl* t_enumerTmp=(new bb_decl_LocalDecl)->g_new(String(),0,0,t_enumerInit);
			bb_expr_Expr* t_hasNextExpr=((new bb_parser_FuncCallExpr)->g_new(((new bb_parser_IdentExpr)->g_new(String(L"HasNext",7),((new bb_expr_VarExpr)->g_new(t_enumerTmp)))),Array<bb_expr_Expr* >()));
			bb_expr_Expr* t_nextObjExpr=((new bb_parser_FuncCallExpr)->g_new(((new bb_parser_IdentExpr)->g_new(String(L"NextObject",10),((new bb_expr_VarExpr)->g_new(t_enumerTmp)))),Array<bb_expr_Expr* >()));
			if((f_varlocal)!=0){
				bb_decl_LocalDecl* t_varTmp2=(new bb_decl_LocalDecl)->g_new(f_varid,0,f_varty,t_nextObjExpr);
				f_block->f_stmts->m_AddFirst((new bb_stmt_DeclStmt)->g_new(t_varTmp2));
			}else{
				f_block->f_stmts->m_AddFirst((new bb_stmt_AssignStmt)->g_new(String(L"=",1),((new bb_parser_IdentExpr)->g_new(f_varid,0)),t_nextObjExpr));
			}
			bb_stmt_WhileStmt* t_whileStmt2=(new bb_stmt_WhileStmt)->g_new(t_hasNextExpr,f_block);
			gc_assign(f_block,(new bb_decl_BlockDecl)->g_new(f_block->f_scope));
			f_block->m_AddStmt((new bb_stmt_DeclStmt)->g_new(t_enumerTmp));
			f_block->m_AddStmt(t_whileStmt2);
		}else{
			bb_config_Err(String(L"Expression cannot be used with For Each.",40));
		}
	}
	f_block->m_Semant();
	return 0;
}
String bb_parser_ForEachinStmt::m_Trans(){
	return bb_translator__trans->m_TransBlock(f_block);
}
void bb_parser_ForEachinStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_varty);
	gc_mark_q(f_expr);
	gc_mark_q(f_block);
}
bb_stmt_AssignStmt::bb_stmt_AssignStmt(){
	f_op=String();
	f_lhs=0;
	f_rhs=0;
	f_tmp1=0;
	f_tmp2=0;
}
bb_stmt_AssignStmt* bb_stmt_AssignStmt::g_new(String t_op,bb_expr_Expr* t_lhs,bb_expr_Expr* t_rhs){
	bb_stmt_Stmt::g_new();
	this->f_op=t_op;
	gc_assign(this->f_lhs,t_lhs);
	gc_assign(this->f_rhs,t_rhs);
	return this;
}
bb_stmt_AssignStmt* bb_stmt_AssignStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_AssignStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_AssignStmt)->g_new(f_op,f_lhs->m_Copy(),f_rhs->m_Copy()));
}
int bb_stmt_AssignStmt::m_FixSideEffects(){
	bb_expr_MemberVarExpr* t_e1=dynamic_cast<bb_expr_MemberVarExpr*>(f_lhs);
	if((t_e1)!=0){
		if(t_e1->f_expr->m_SideEffects()){
			gc_assign(f_tmp1,(new bb_decl_LocalDecl)->g_new(String(),0,t_e1->f_expr->f_exprType,t_e1->f_expr));
			gc_assign(f_lhs,((new bb_expr_MemberVarExpr)->g_new(((new bb_expr_VarExpr)->g_new(f_tmp1)),t_e1->f_decl))->m_Semant());
		}
	}
	bb_expr_IndexExpr* t_e2=dynamic_cast<bb_expr_IndexExpr*>(f_lhs);
	if((t_e2)!=0){
		bb_expr_Expr* t_expr=t_e2->f_expr;
		bb_expr_Expr* t_index=t_e2->f_index;
		if(t_expr->m_SideEffects() || t_index->m_SideEffects()){
			if(t_expr->m_SideEffects()){
				gc_assign(f_tmp1,(new bb_decl_LocalDecl)->g_new(String(),0,t_expr->f_exprType,t_expr));
				t_expr=((new bb_expr_VarExpr)->g_new(f_tmp1));
			}
			if(t_index->m_SideEffects()){
				gc_assign(f_tmp2,(new bb_decl_LocalDecl)->g_new(String(),0,t_index->f_exprType,t_index));
				t_index=((new bb_expr_VarExpr)->g_new(f_tmp2));
			}
			gc_assign(f_lhs,((new bb_expr_IndexExpr)->g_new(t_expr,t_index))->m_Semant());
		}
	}
	return 0;
}
int bb_stmt_AssignStmt::m_OnSemant(){
	gc_assign(f_rhs,f_rhs->m_Semant());
	gc_assign(f_lhs,f_lhs->m_SemantSet(f_op,f_rhs));
	if(((dynamic_cast<bb_expr_InvokeExpr*>(f_lhs))!=0) || ((dynamic_cast<bb_expr_InvokeMemberExpr*>(f_lhs))!=0)){
		f_rhs=0;
		return 0;
	}
	int t_kludge=0;
	String t_=f_op;
	if(t_==String(L"=",1)){
		gc_assign(f_rhs,f_rhs->m_Cast(f_lhs->f_exprType,0));
		t_kludge=0;
	}else{
		if(t_==String(L"*=",2) || t_==String(L"/=",2) || t_==String(L"+=",2) || t_==String(L"-=",2)){
			if(((dynamic_cast<bb_type_NumericType*>(f_lhs->f_exprType))!=0) && ((f_lhs->f_exprType->m_EqualsType(f_rhs->f_exprType))!=0)){
				t_kludge=0;
				if(bb_config_ENV_LANG==String(L"js",2)){
					if(f_op==String(L"/=",2) && ((dynamic_cast<bb_type_IntType*>(f_lhs->f_exprType))!=0)){
						t_kludge=1;
					}
				}
			}else{
				t_kludge=1;
			}
		}else{
			if(t_==String(L"&=",2) || t_==String(L"|=",2) || t_==String(L"~=",2) || t_==String(L"shl=",4) || t_==String(L"shr=",4) || t_==String(L"mod=",4)){
				if(((dynamic_cast<bb_type_IntType*>(f_lhs->f_exprType))!=0) && ((f_lhs->f_exprType->m_EqualsType(f_rhs->f_exprType))!=0)){
					t_kludge=0;
				}else{
					t_kludge=1;
				}
			}else{
				bb_config_InternalErr(String(L"Internal error",14));
			}
		}
	}
	if(bb_config_ENV_LANG==String()){
		t_kludge=1;
	}
	if((t_kludge)!=0){
		m_FixSideEffects();
		gc_assign(f_rhs,((new bb_expr_BinaryMathExpr)->g_new(f_op.Slice(0,-1),f_lhs,f_rhs))->m_Semant()->m_Cast(f_lhs->f_exprType,0));
		f_op=String(L"=",1);
	}
	return 0;
}
String bb_stmt_AssignStmt::m_Trans(){
	bb_config__errInfo=f_errInfo;
	if((f_tmp1)!=0){
		f_tmp1->m_Semant();
	}
	if((f_tmp2)!=0){
		f_tmp2->m_Semant();
	}
	return bb_translator__trans->m_TransAssignStmt(this);
}
void bb_stmt_AssignStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_lhs);
	gc_mark_q(f_rhs);
	gc_mark_q(f_tmp1);
	gc_mark_q(f_tmp2);
}
bb_stmt_ForStmt::bb_stmt_ForStmt(){
	f_init=0;
	f_expr=0;
	f_incr=0;
	f_block=0;
}
bb_stmt_ForStmt* bb_stmt_ForStmt::g_new(bb_stmt_Stmt* t_init,bb_expr_Expr* t_expr,bb_stmt_Stmt* t_incr,bb_decl_BlockDecl* t_block){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_init,t_init);
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_incr,t_incr);
	gc_assign(this->f_block,t_block);
	return this;
}
bb_stmt_ForStmt* bb_stmt_ForStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_ForStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_ForStmt)->g_new(f_init->m_Copy2(t_scope),f_expr->m_Copy(),f_incr->m_Copy2(t_scope),f_block->m_CopyBlock(t_scope)));
}
int bb_stmt_ForStmt::m_OnSemant(){
	bb_decl_PushEnv(f_block);
	f_init->m_Semant();
	gc_assign(f_expr,f_expr->m_Semant());
	bb_decl__loopnest+=1;
	f_block->m_Semant();
	bb_decl__loopnest-=1;
	f_incr->m_Semant();
	bb_decl_PopEnv();
	bb_stmt_AssignStmt* t_assop=dynamic_cast<bb_stmt_AssignStmt*>(f_incr);
	bb_expr_BinaryExpr* t_addop=dynamic_cast<bb_expr_BinaryExpr*>(t_assop->f_rhs);
	String t_stpval=t_addop->f_rhs->m_Eval();
	if(t_stpval.StartsWith(String(L"-",1))){
		bb_expr_BinaryExpr* t_bexpr=dynamic_cast<bb_expr_BinaryExpr*>(f_expr);
		String t_=t_bexpr->f_op;
		if(t_==String(L"<",1)){
			t_bexpr->f_op=String(L">",1);
		}else{
			if(t_==String(L"<=",2)){
				t_bexpr->f_op=String(L">=",2);
			}
		}
	}
	return 0;
}
String bb_stmt_ForStmt::m_Trans(){
	return bb_translator__trans->m_TransForStmt(this);
}
void bb_stmt_ForStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_init);
	gc_mark_q(f_expr);
	gc_mark_q(f_incr);
	gc_mark_q(f_block);
}
bb_expr_VarExpr::bb_expr_VarExpr(){
	f_decl=0;
}
bb_expr_VarExpr* bb_expr_VarExpr::g_new(bb_decl_VarDecl* t_decl){
	bb_expr_Expr::g_new();
	gc_assign(this->f_decl,t_decl);
	return this;
}
bb_expr_VarExpr* bb_expr_VarExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_VarExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	if(!((f_decl->m_IsSemanted())!=0)){
		bb_config_Err(String(L"Internal error - decl not semanted: ",36)+f_decl->f_ident);
	}
	gc_assign(f_exprType,f_decl->f_type);
	return (this);
}
String bb_expr_VarExpr::m_ToString(){
	return String(L"VarExpr(",8)+f_decl->m_ToString()+String(L")",1);
}
bool bb_expr_VarExpr::m_SideEffects(){
	return false;
}
bb_expr_Expr* bb_expr_VarExpr::m_SemantSet(String t_op,bb_expr_Expr* t_rhs){
	return m_Semant();
}
String bb_expr_VarExpr::m_Trans(){
	m_Semant();
	return bb_translator__trans->m_TransVarExpr(this);
}
String bb_expr_VarExpr::m_TransVar(){
	m_Semant();
	return bb_translator__trans->m_TransVarExpr(this);
}
void bb_expr_VarExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_decl);
}
bb_stmt_CatchStmt::bb_stmt_CatchStmt(){
	f_init=0;
	f_block=0;
}
bb_stmt_CatchStmt* bb_stmt_CatchStmt::g_new(bb_decl_LocalDecl* t_init,bb_decl_BlockDecl* t_block){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_init,t_init);
	gc_assign(this->f_block,t_block);
	return this;
}
bb_stmt_CatchStmt* bb_stmt_CatchStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_CatchStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_CatchStmt)->g_new(dynamic_cast<bb_decl_LocalDecl*>(f_init->m_Copy()),f_block->m_CopyBlock(t_scope)));
}
int bb_stmt_CatchStmt::m_OnSemant(){
	f_init->m_Semant();
	if(!((dynamic_cast<bb_type_ObjectType*>(f_init->f_type))!=0)){
		bb_config_Err(String(L"Variable type must extend Throwable",35));
	}
	if(!((f_init->f_type->m_GetClass()->m_IsThrowable())!=0)){
		bb_config_Err(String(L"Variable type must extend Throwable",35));
	}
	f_block->m_InsertDecl(f_init);
	f_block->m_Semant();
	return 0;
}
String bb_stmt_CatchStmt::m_Trans(){
	return String();
}
void bb_stmt_CatchStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_init);
	gc_mark_q(f_block);
}
bb_stack_Stack6::bb_stack_Stack6(){
	f_data=Array<bb_stmt_CatchStmt* >();
	f_length=0;
}
bb_stack_Stack6* bb_stack_Stack6::g_new(){
	return this;
}
bb_stack_Stack6* bb_stack_Stack6::g_new2(Array<bb_stmt_CatchStmt* > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack6::m_Push16(bb_stmt_CatchStmt* t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	gc_assign(f_data[f_length],t_value);
	f_length+=1;
	return 0;
}
int bb_stack_Stack6::m_Push17(Array<bb_stmt_CatchStmt* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push16(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack6::m_Push18(Array<bb_stmt_CatchStmt* > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push16(t_values[t_i]);
	}
	return 0;
}
int bb_stack_Stack6::m_Length(){
	return f_length;
}
Array<bb_stmt_CatchStmt* > bb_stack_Stack6::m_ToArray(){
	Array<bb_stmt_CatchStmt* > t_t=Array<bb_stmt_CatchStmt* >(f_length);
	for(int t_i=0;t_i<f_length;t_i=t_i+1){
		gc_assign(t_t[t_i],f_data[t_i]);
	}
	return t_t;
}
void bb_stack_Stack6::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
bb_stmt_TryStmt::bb_stmt_TryStmt(){
	f_block=0;
	f_catches=Array<bb_stmt_CatchStmt* >();
}
bb_stmt_TryStmt* bb_stmt_TryStmt::g_new(bb_decl_BlockDecl* t_block,Array<bb_stmt_CatchStmt* > t_catches){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_block,t_block);
	gc_assign(this->f_catches,t_catches);
	return this;
}
bb_stmt_TryStmt* bb_stmt_TryStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_TryStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	Array<bb_stmt_CatchStmt* > t_tcatches=this->f_catches.Slice(0);
	for(int t_i=0;t_i<t_tcatches.Length();t_i=t_i+1){
		gc_assign(t_tcatches[t_i],dynamic_cast<bb_stmt_CatchStmt*>(t_tcatches[t_i]->m_Copy2(t_scope)));
	}
	return ((new bb_stmt_TryStmt)->g_new(f_block->m_CopyBlock(t_scope),t_tcatches));
}
int bb_stmt_TryStmt::m_OnSemant(){
	f_block->m_Semant();
	for(int t_i=0;t_i<f_catches.Length();t_i=t_i+1){
		f_catches[t_i]->m_Semant();
		for(int t_j=0;t_j<t_i;t_j=t_j+1){
			if((f_catches[t_i]->f_init->f_type->m_ExtendsType(f_catches[t_j]->f_init->f_type))!=0){
				bb_config_PushErr(f_catches[t_i]->f_errInfo);
				bb_config_Err(String(L"Catch variable class extends earlier catch variable class",57));
			}
		}
	}
	return 0;
}
String bb_stmt_TryStmt::m_Trans(){
	return bb_translator__trans->m_TransTryStmt(this);
}
void bb_stmt_TryStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_block);
	gc_mark_q(f_catches);
}
bb_stmt_ThrowStmt::bb_stmt_ThrowStmt(){
	f_expr=0;
}
bb_stmt_ThrowStmt* bb_stmt_ThrowStmt::g_new(bb_expr_Expr* t_expr){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_expr,t_expr);
	return this;
}
bb_stmt_ThrowStmt* bb_stmt_ThrowStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_ThrowStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_ThrowStmt)->g_new(f_expr->m_Copy()));
}
int bb_stmt_ThrowStmt::m_OnSemant(){
	gc_assign(f_expr,f_expr->m_Semant());
	if(!((dynamic_cast<bb_type_ObjectType*>(f_expr->f_exprType))!=0)){
		bb_config_Err(String(L"Expression type must extend Throwable",37));
	}
	if(!((f_expr->f_exprType->m_GetClass()->m_IsThrowable())!=0)){
		bb_config_Err(String(L"Expression type must extend Throwable",37));
	}
	return 0;
}
String bb_stmt_ThrowStmt::m_Trans(){
	return bb_translator__trans->m_TransThrowStmt(this);
}
void bb_stmt_ThrowStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_expr);
}
bb_stmt_ExprStmt::bb_stmt_ExprStmt(){
	f_expr=0;
}
bb_stmt_ExprStmt* bb_stmt_ExprStmt::g_new(bb_expr_Expr* t_expr){
	bb_stmt_Stmt::g_new();
	gc_assign(this->f_expr,t_expr);
	return this;
}
bb_stmt_ExprStmt* bb_stmt_ExprStmt::g_new2(){
	bb_stmt_Stmt::g_new();
	return this;
}
bb_stmt_Stmt* bb_stmt_ExprStmt::m_OnCopy2(bb_decl_ScopeDecl* t_scope){
	return ((new bb_stmt_ExprStmt)->g_new(f_expr->m_Copy()));
}
int bb_stmt_ExprStmt::m_OnSemant(){
	gc_assign(f_expr,f_expr->m_Semant());
	if(!((f_expr)!=0)){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	return 0;
}
String bb_stmt_ExprStmt::m_Trans(){
	return bb_translator__trans->m_TransExprStmt(this);
}
void bb_stmt_ExprStmt::mark(){
	bb_stmt_Stmt::mark();
	gc_mark_q(f_expr);
}
bb_decl_AppDecl* bb_parser_ParseApp(String t_path){
	bb_decl_AppDecl* t_app=(new bb_decl_AppDecl)->g_new();
	String t_source=bb_preprocessor_PreProcess(t_path);
	bb_toker_Toker* t_toker=(new bb_toker_Toker)->g_new(t_path,t_source);
	bb_parser_Parser* t_parser=(new bb_parser_Parser)->g_new(t_toker,t_app,0,0);
	t_parser->m_ParseMain();
	return t_app;
}
bb_reflector_Reflector::bb_reflector_Reflector(){
	f_debug=false;
	f_refmod=0;
	f_langmod=0;
	f_boxesmod=0;
	f_munged=(new bb_map_StringMap4)->g_new();
	f_modpaths=(new bb_map_StringMap)->g_new();
	f_modexprs=(new bb_map_StringMap)->g_new();
	f_refmods=(new bb_set_StringSet)->g_new();
	f_classdecls=(new bb_stack_Stack7)->g_new();
	f_classids=(new bb_map_StringMap4)->g_new();
	f_output=(new bb_stack_StringStack)->g_new();
}
bb_reflector_Reflector* bb_reflector_Reflector::g_new(){
	return this;
}
String bb_reflector_Reflector::m_ModPath(bb_decl_ModuleDecl* t_mdecl){
	String t_p=t_mdecl->f_filepath.Replace(String(L"\\",1),String(L"/",1));
	if(!t_p.EndsWith(String(L".monkey",7))){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	t_p=t_p.Slice(0,-7);
	Array<String > t_=bb_config_GetCfgVar(String(L"MODPATH",7)).Split(String(L";",1));
	int t_2=0;
	while(t_2<t_.Length()){
		String t_dir=t_[t_2];
		t_2=t_2+1;
		if(!((t_dir).Length()!=0) || t_dir==String(L".",1)){
			continue;
		}
		t_dir=t_dir.Replace(String(L"\\",1),String(L"/",1));
		if(!t_dir.EndsWith(String(L"/",1))){
			t_dir=t_dir+String(L"/",1);
		}
		if(!t_p.StartsWith(t_dir)){
			continue;
		}
		t_p=t_p.Slice(t_dir.Length()-1);
		int t_i=t_p.FindLast(String(L"/",1));
		if(t_i!=-1){
			String t_e=t_p.Slice(t_i);
			if(t_p.EndsWith(t_e+t_e)){
				t_p=t_p.Slice(0,t_i);
			}
		}
		t_p=t_p.Slice(1).Replace(String(L"/",1),String(L".",1));
		return t_p;
	}
	Error(String(L"Invalid module path for module:",31)+t_mdecl->f_filepath);
	return String();
}
bool bb_reflector_Reflector::g_MatchPath(String t_text,String t_pattern){
	Array<String > t_alts=t_pattern.Split(String(L"|",1));
	Array<String > t_=t_alts;
	int t_2=0;
	while(t_2<t_.Length()){
		String t_alt=t_[t_2];
		t_2=t_2+1;
		if(!((t_alt).Length()!=0)){
			continue;
		}
		Array<String > t_bits=t_alt.Split(String(L"*",1));
		if(t_bits.Length()==1){
			if(t_bits[0]==t_text){
				return true;
			}
			continue;
		}
		if(!t_text.StartsWith(t_bits[0])){
			continue;
		}
		int t_i=t_bits[0].Length();
		for(int t_j=1;t_j<t_bits.Length()-1;t_j=t_j+1){
			String t_bit=t_bits[t_j];
			t_i=t_text.Find(t_bit,t_i);
			if(t_i==-1){
				break;
			}
			t_i+=t_bit.Length();
		}
		if(t_i!=-1 && t_text.Slice(t_i).EndsWith(t_bits[t_bits.Length()-1])){
			return true;
		}
	}
	return false;
}
String bb_reflector_Reflector::m_Mung(String t_ident){
	if(f_debug){
		t_ident=String(L"R",1)+t_ident;
		t_ident=t_ident.Replace(String(L"_",1),String(L"_0",2));
		t_ident=t_ident.Replace(String(L"[",1),String(L"_1",2));
		t_ident=t_ident.Replace(String(L"]",1),String(L"_2",2));
		t_ident=t_ident.Replace(String(L"<",1),String(L"_3",2));
		t_ident=t_ident.Replace(String(L">",1),String(L"_4",2));
		t_ident=t_ident.Replace(String(L",",1),String(L"_5",2));
		t_ident=t_ident.Replace(String(L".",1),String(L"_",1));
	}else{
		t_ident=String(L"R",1);
	}
	if(f_munged->m_Contains(t_ident)){
		int t_n=f_munged->m_Get(t_ident);
		t_n+=1;
		f_munged->m_Set4(t_ident,t_n);
		t_ident=t_ident+String(t_n);
	}else{
		f_munged->m_Set4(t_ident,1);
	}
	return t_ident;
}
bool bb_reflector_Reflector::m_ValidClass(bb_decl_ClassDecl* t_cdecl){
	if(t_cdecl->f_munged==String(L"Object",6)){
		return true;
	}
	if(t_cdecl->f_munged==String(L"ThrowableObject",15)){
		return true;
	}
	if(!((t_cdecl->m_ExtendsObject())!=0)){
		return false;
	}
	if(!f_refmods->m_Contains(t_cdecl->m_ModuleScope()->f_filepath)){
		return false;
	}
	Array<bb_type_Type* > t_=t_cdecl->f_instArgs;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_type_Type* t_arg=t_[t_2];
		t_2=t_2+1;
		if(((dynamic_cast<bb_type_ObjectType*>(t_arg))!=0) && !m_ValidClass(t_arg->m_GetClass())){
			return false;
		}
	}
	if((t_cdecl->f_superClass)!=0){
		return m_ValidClass(t_cdecl->f_superClass);
	}
	return true;
}
String bb_reflector_Reflector::m_TypeExpr(bb_type_Type* t_ty,bool t_path){
	if((dynamic_cast<bb_type_VoidType*>(t_ty))!=0){
		return String(L"Void",4);
	}
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"Bool",4);
	}
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		return String(L"Int",3);
	}
	if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
		return String(L"Float",5);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"String",6);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return m_TypeExpr(dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType,t_path)+String(L"[]",2);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return m_DeclExpr((t_ty->m_GetClass()),t_path);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_reflector_Reflector::m_DeclExpr(bb_decl_Decl* t_decl,bool t_path){
	if(t_path && ((dynamic_cast<bb_decl_ClassDecl*>(t_decl->f_scope))!=0)){
		return t_decl->f_ident;
	}
	bb_decl_ModuleDecl* t_mdecl=dynamic_cast<bb_decl_ModuleDecl*>(t_decl);
	if((t_mdecl)!=0){
		if(t_path){
			return f_modpaths->m_Get(t_mdecl->f_filepath);
		}
		return f_modexprs->m_Get(t_mdecl->f_filepath);
	}
	bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl);
	if(((t_cdecl)!=0) && t_cdecl->f_munged==String(L"Object",6)){
		if(t_path){
			return String(L"monkey.lang.Object",18);
		}
		return String(L"Object",6);
	}
	if(((t_cdecl)!=0) && t_cdecl->f_munged==String(L"ThrowableObject",15)){
		if(t_path){
			return String(L"monkey.lang.Throwable",21);
		}
		return String(L"Throwable",9);
	}
	String t_ident=m_DeclExpr((t_decl->f_scope),t_path)+String(L".",1)+t_decl->f_ident;
	if(((t_cdecl)!=0) && ((t_cdecl->f_instArgs).Length()!=0)){
		String t_t=String();
		Array<bb_type_Type* > t_=t_cdecl->f_instArgs;
		int t_2=0;
		while(t_2<t_.Length()){
			bb_type_Type* t_arg=t_[t_2];
			t_2=t_2+1;
			if((t_t).Length()!=0){
				t_t=t_t+String(L",",1);
			}
			t_t=t_t+m_TypeExpr(t_arg,t_path);
		}
		t_ident=t_ident+(String(L"<",1)+t_t+String(L">",1));
	}
	return t_ident;
}
int bb_reflector_Reflector::m_Emit(String t_t){
	f_output->m_Push(t_t);
	return 0;
}
bool bb_reflector_Reflector::m_ValidType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return m_ValidType(dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return m_ValidClass(t_ty->m_GetClass());
	}
	return true;
}
String bb_reflector_Reflector::m_TypeInfo(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_VoidType*>(t_ty))!=0){
		return String(L"Null",4);
	}
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"_boolClass",10);
	}
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		return String(L"_intClass",9);
	}
	if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
		return String(L"_floatClass",11);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"_stringClass",12);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		bb_type_Type* t_elemType=dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType;
		String t_name=String(L"monkey.boxes.ArrayObject<",25)+m_TypeExpr(t_elemType,true)+String(L">",1);
		if(f_classids->m_Contains(t_name)){
			return String(L"_classes[",9)+String(f_classids->m_Get(t_name))+String(L"]",1);
		}
		if(f_debug){
			Print(String(L"Instantiating class: ",21)+t_name);
		}
		bb_type_Type* t_[]={t_elemType};
		bb_decl_ClassDecl* t_cdecl=f_boxesmod->m_FindType(String(L"ArrayObject",11),Array<bb_type_Type* >(t_,1))->m_GetClass();
		bb_list_Enumerator2* t_2=t_cdecl->m_Decls()->m_ObjectEnumerator();
		while(t_2->m_HasNext()){
			bb_decl_Decl* t_decl=t_2->m_NextObject();
			if(!((dynamic_cast<bb_decl_AliasDecl*>(t_decl))!=0)){
				t_decl->m_Semant();
			}
		}
		int t_id=f_classdecls->m_Length();
		f_classids->m_Set4(t_name,t_id);
		f_classdecls->m_Push19(t_cdecl);
		return String(L"_classes[",9)+String(t_id)+String(L"]",1);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		String t_name2=m_DeclExpr((t_ty->m_GetClass()),true);
		if(f_classids->m_Contains(t_name2)){
			return String(L"_classes[",9)+String(f_classids->m_Get(t_name2))+String(L"]",1);
		}
		return String(L"_unknownClass",13);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
int bb_reflector_Reflector::m_Attrs(bb_decl_Decl* t_decl){
	return t_decl->f_attrs>>8&255;
}
String bb_reflector_Reflector::m_Box(bb_type_Type* t_ty,String t_expr){
	if((dynamic_cast<bb_type_VoidType*>(t_ty))!=0){
		return t_expr;
	}
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"New BoolObject(",15)+t_expr+String(L")",1);
	}
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		return String(L"New IntObject(",14)+t_expr+String(L")",1);
	}
	if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
		return String(L"New FloatObject(",16)+t_expr+String(L")",1);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"New StringObject(",17)+t_expr+String(L")",1);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return String(L"New ArrayObject<",16)+m_TypeExpr(dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType,false)+String(L">(",2)+t_expr+String(L")",1);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return t_expr;
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_reflector_Reflector::m_Emit2(bb_decl_ConstDecl* t_tdecl){
	if(!m_ValidType(t_tdecl->f_type)){
		return String();
	}
	String t_name=m_DeclExpr((t_tdecl),true);
	String t_expr=m_DeclExpr((t_tdecl),false);
	String t_type=m_TypeInfo(t_tdecl->f_type);
	return String(L"New ConstInfo(\"",15)+t_name+String(L"\",",2)+String(m_Attrs(t_tdecl))+String(L",",1)+t_type+String(L",",1)+m_Box(t_tdecl->f_type,t_expr)+String(L")",1);
}
String bb_reflector_Reflector::m_Unbox(bb_type_Type* t_ty,String t_expr){
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"BoolObject(",11)+t_expr+String(L").value",7);
	}
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		return String(L"IntObject(",10)+t_expr+String(L").value",7);
	}
	if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
		return String(L"FloatObject(",12)+t_expr+String(L").value",7);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"StringObject(",13)+t_expr+String(L").value",7);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return String(L"ArrayObject<",12)+m_TypeExpr(dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType,false)+String(L">(",2)+t_expr+String(L").value",7);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return m_DeclExpr((t_ty->m_GetClass()),false)+String(L"(",1)+t_expr+String(L")",1);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_reflector_Reflector::m_Emit3(bb_decl_ClassDecl* t_cdecl){
	if((t_cdecl->f_args).Length()!=0){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	String t_name=m_DeclExpr((t_cdecl),true);
	String t_expr=m_DeclExpr((t_cdecl),false);
	String t_ident=m_Mung(t_name);
	String t_sclass=String(L"Null",4);
	if((t_cdecl->f_superClass)!=0){
		t_sclass=m_TypeInfo(t_cdecl->f_superClass->f_objectType);
	}
	String t_ifaces=String();
	Array<bb_decl_ClassDecl* > t_=t_cdecl->f_implments;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ClassDecl* t_idecl=t_[t_2];
		t_2=t_2+1;
		if((t_ifaces).Length()!=0){
			t_ifaces=t_ifaces+String(L",",1);
		}
		t_ifaces=t_ifaces+m_TypeInfo(t_idecl->f_objectType);
	}
	bb_stack_StringStack* t_consts=(new bb_stack_StringStack)->g_new();
	bb_stack_StringStack* t_globals=(new bb_stack_StringStack)->g_new();
	bb_stack_StringStack* t_fields=(new bb_stack_StringStack)->g_new();
	bb_stack_StringStack* t_methods=(new bb_stack_StringStack)->g_new();
	bb_stack_StringStack* t_functions=(new bb_stack_StringStack)->g_new();
	bb_stack_StringStack* t_ctors=(new bb_stack_StringStack)->g_new();
	bb_list_Enumerator2* t_3=t_cdecl->m_Decls()->m_ObjectEnumerator();
	while(t_3->m_HasNext()){
		bb_decl_Decl* t_decl=t_3->m_NextObject();
		if((dynamic_cast<bb_decl_AliasDecl*>(t_decl))!=0){
			continue;
		}
		if(!((t_decl->m_IsSemanted())!=0)){
			continue;
		}
		bb_decl_ConstDecl* t_pdecl=dynamic_cast<bb_decl_ConstDecl*>(t_decl);
		if((t_pdecl)!=0){
			String t_p=m_Emit2(t_pdecl);
			if((t_p).Length()!=0){
				t_consts->m_Push(t_p);
			}
			continue;
		}
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl);
		if((t_gdecl)!=0){
			String t_g=m_Emit6(t_gdecl);
			if((t_g).Length()!=0){
				t_globals->m_Push(t_g);
			}
			continue;
		}
		bb_decl_FieldDecl* t_tdecl=dynamic_cast<bb_decl_FieldDecl*>(t_decl);
		if((t_tdecl)!=0){
			String t_f=m_Emit5(t_tdecl);
			if((t_f).Length()!=0){
				t_fields->m_Push(t_f);
			}
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
		if((t_fdecl)!=0){
			String t_f2=m_Emit4(t_fdecl);
			if((t_f2).Length()!=0){
				if(t_fdecl->m_IsCtor()){
					t_ctors->m_Push(t_f2);
				}else{
					if(t_fdecl->m_IsMethod()){
						t_methods->m_Push(t_f2);
					}else{
						t_functions->m_Push(t_f2);
					}
				}
			}
			continue;
		}
	}
	m_Emit(String(L"Class ",6)+t_ident+String(L" Extends ClassInfo",18));
	m_Emit(String(L" Method New()",13));
	m_Emit(String(L"  Super.New(\"",13)+t_name+String(L"\",",2)+String(m_Attrs(t_cdecl))+String(L",",1)+t_sclass+String(L",[",2)+t_ifaces+String(L"])",2));
	String t_4=t_name;
	if(t_4==String(L"monkey.boxes.BoolObject",23)){
		m_Emit(String(L"  _boolClass=Self",17));
	}else{
		if(t_4==String(L"monkey.boxes.IntObject",22)){
			m_Emit(String(L"  _intClass=Self",16));
		}else{
			if(t_4==String(L"monkey.boxes.FloatObject",24)){
				m_Emit(String(L"  _floatClass=Self",18));
			}else{
				if(t_4==String(L"monkey.boxes.StringObject",25)){
					m_Emit(String(L"  _stringClass=Self",19));
				}
			}
		}
	}
	m_Emit(String(L" End",4));
	if(t_name.StartsWith(String(L"monkey.boxes.ArrayObject<",25))){
		bb_type_Type* t_elemType=t_cdecl->f_instArgs[0];
		String t_elemExpr=m_TypeExpr(t_elemType,false);
		int t_i=t_elemExpr.Find(String(L"[]",2),0);
		if(t_i==-1){
			t_i=t_elemExpr.Length();
		}
		String t_ARRAY_PREFIX=f_modexprs->m_Get(f_boxesmod->f_filepath)+String(L".ArrayObject<",13);
		m_Emit(String(L" Method ElementType:ClassInfo() Property",40));
		m_Emit(String(L"  Return ",9)+m_TypeInfo(t_elemType));
		m_Emit(String(L" End",4));
		m_Emit(String(L" Method ArrayLength:Int(i:Object) Property",42));
		m_Emit(String(L"  Return ",9)+t_ARRAY_PREFIX+t_elemExpr+String(L">(i).value.Length",17));
		m_Emit(String(L" End",4));
		m_Emit(String(L" Method GetElement:Object(i:Object,e)",37));
		m_Emit(String(L"  Return ",9)+m_Box(t_elemType,t_ARRAY_PREFIX+t_elemExpr+String(L">(i).value[e]",13)));
		m_Emit(String(L" End",4));
		m_Emit(String(L" Method SetElement:Void(i:Object,e,v:Object)",44));
		m_Emit(String(L"  ",2)+t_ARRAY_PREFIX+t_elemExpr+String(L">(i).value[e]=",14)+m_Unbox(t_elemType,String(L"v",1)));
		m_Emit(String(L" End",4));
		m_Emit(String(L" Method NewArray:Object(l:Int)",30));
		m_Emit(String(L"  Return ",9)+m_Box((t_elemType->m_ArrayOf()),String(L"New ",4)+t_elemExpr.Slice(0,t_i)+String(L"[l]",3)+t_elemExpr.Slice(t_i)));
		m_Emit(String(L" End",4));
	}
	if(!((t_cdecl->m_IsAbstract())!=0) && !((t_cdecl->m_IsExtern())!=0)){
		m_Emit(String(L" Method NewInstance:Object()",28));
		m_Emit(String(L"  Return New ",13)+t_expr);
		m_Emit(String(L" End",4));
	}
	m_Emit(String(L" Method Init()",14));
	if((t_consts->m_Length())!=0){
		m_Emit(String(L"  _consts=new ConstInfo[",24)+String(t_consts->m_Length())+String(L"]",1));
		for(int t_i2=0;t_i2<t_consts->m_Length();t_i2=t_i2+1){
			m_Emit(String(L"  _consts[",10)+String(t_i2)+String(L"]=",2)+t_consts->m_Get2(t_i2));
		}
	}
	if((t_globals->m_Length())!=0){
		m_Emit(String(L"  _globals=new GlobalInfo[",26)+String(t_globals->m_Length())+String(L"]",1));
		for(int t_i3=0;t_i3<t_globals->m_Length();t_i3=t_i3+1){
			m_Emit(String(L"  _globals[",11)+String(t_i3)+String(L"]=New ",6)+t_globals->m_Get2(t_i3));
		}
	}
	if((t_fields->m_Length())!=0){
		m_Emit(String(L"  _fields=New FieldInfo[",24)+String(t_fields->m_Length())+String(L"]",1));
		for(int t_i4=0;t_i4<t_fields->m_Length();t_i4=t_i4+1){
			m_Emit(String(L"  _fields[",10)+String(t_i4)+String(L"]=New ",6)+t_fields->m_Get2(t_i4));
		}
	}
	if((t_methods->m_Length())!=0){
		m_Emit(String(L"  _methods=New MethodInfo[",26)+String(t_methods->m_Length())+String(L"]",1));
		for(int t_i5=0;t_i5<t_methods->m_Length();t_i5=t_i5+1){
			m_Emit(String(L"  _methods[",11)+String(t_i5)+String(L"]=New ",6)+t_methods->m_Get2(t_i5));
		}
	}
	if((t_functions->m_Length())!=0){
		m_Emit(String(L"  _functions=New FunctionInfo[",30)+String(t_functions->m_Length())+String(L"]",1));
		for(int t_i6=0;t_i6<t_functions->m_Length();t_i6=t_i6+1){
			m_Emit(String(L"  _functions[",13)+String(t_i6)+String(L"]=New ",6)+t_functions->m_Get2(t_i6));
		}
	}
	if((t_ctors->m_Length())!=0){
		m_Emit(String(L"  _ctors=New FunctionInfo[",26)+String(t_ctors->m_Length())+String(L"]",1));
		for(int t_i7=0;t_i7<t_ctors->m_Length();t_i7=t_i7+1){
			m_Emit(String(L"  _ctors[",9)+String(t_i7)+String(L"]=New ",6)+t_ctors->m_Get2(t_i7));
		}
	}
	m_Emit(String(L" InitR()",8));
	m_Emit(String(L" End",4));
	m_Emit(String(L"End",3));
	return t_ident;
}
String bb_reflector_Reflector::m_Emit4(bb_decl_FuncDecl* t_fdecl){
	if(!m_ValidType(t_fdecl->f_retType)){
		return String();
	}
	Array<bb_decl_ArgDecl* > t_=t_fdecl->f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_arg=t_[t_2];
		t_2=t_2+1;
		if(!m_ValidType(t_arg->f_type)){
			return String();
		}
	}
	String t_name=m_DeclExpr((t_fdecl),true);
	String t_expr=m_DeclExpr((t_fdecl),false);
	String t_ident=m_Mung(t_name);
	String t_rtype=m_TypeInfo(t_fdecl->f_retType);
	String t_base=String(L"FunctionInfo",12);
	if(t_fdecl->m_IsMethod()){
		String t_clas=m_DeclExpr((t_fdecl->m_ClassScope()),false);
		t_expr=t_clas+String(L"(i).",4)+t_fdecl->f_ident;
		t_base=String(L"MethodInfo",10);
	}
	Array<String > t_argtys=Array<String >(t_fdecl->f_argDecls.Length());
	for(int t_i=0;t_i<t_argtys.Length();t_i=t_i+1){
		t_argtys[t_i]=m_TypeInfo(t_fdecl->f_argDecls[t_i]->f_type);
	}
	m_Emit(String(L"Class ",6)+t_ident+String(L" Extends ",9)+t_base);
	m_Emit(String(L" Method New()",13));
	m_Emit(String(L"  Super.New(\"",13)+t_name+String(L"\",",2)+String(m_Attrs(t_fdecl))+String(L",",1)+t_rtype+String(L",[",2)+String(L",",1).Join(t_argtys)+String(L"])",2));
	m_Emit(String(L" End",4));
	if(t_fdecl->m_IsMethod()){
		m_Emit(String(L" Method Invoke:Object(i:Object,p:Object[])",42));
	}else{
		m_Emit(String(L" Method Invoke:Object(p:Object[])",33));
	}
	bb_stack_StringStack* t_args=(new bb_stack_StringStack)->g_new();
	for(int t_i2=0;t_i2<t_fdecl->f_argDecls.Length();t_i2=t_i2+1){
		bb_decl_ArgDecl* t_arg2=t_fdecl->f_argDecls[t_i2];
		t_args->m_Push(m_Unbox(t_arg2->f_type,String(L"p[",2)+String(t_i2)+String(L"]",1)));
	}
	if(t_fdecl->m_IsCtor()){
		bb_decl_ClassDecl* t_cdecl=t_fdecl->m_ClassScope();
		if((t_cdecl->m_IsAbstract())!=0){
			m_Emit(String(L"  Return Null",13));
		}else{
			m_Emit(String(L"  Return New ",13)+m_DeclExpr((t_cdecl),false)+String(L"(",1)+t_args->m_Join(String(L",",1))+String(L")",1));
		}
	}else{
		if((dynamic_cast<bb_type_VoidType*>(t_fdecl->f_retType))!=0){
			m_Emit(String(L"  ",2)+t_expr+String(L"(",1)+t_args->m_Join(String(L",",1))+String(L")",1));
		}else{
			m_Emit(String(L"  Return ",9)+m_Box(t_fdecl->f_retType,t_expr+String(L"(",1)+t_args->m_Join(String(L",",1))+String(L")",1)));
		}
	}
	m_Emit(String(L" End",4));
	m_Emit(String(L"End",3));
	return t_ident;
}
String bb_reflector_Reflector::m_Emit5(bb_decl_FieldDecl* t_tdecl){
	if(!m_ValidType(t_tdecl->f_type)){
		return String();
	}
	String t_name=t_tdecl->f_ident;
	String t_ident=m_Mung(t_name);
	String t_type=m_TypeInfo(t_tdecl->f_type);
	String t_clas=m_DeclExpr((t_tdecl->m_ClassScope()),false);
	String t_expr=t_clas+String(L"(i).",4)+t_tdecl->f_ident;
	m_Emit(String(L"Class ",6)+t_ident+String(L" Extends FieldInfo",18));
	m_Emit(String(L" Method New()",13));
	m_Emit(String(L"  Super.New(\"",13)+t_name+String(L"\",",2)+String(m_Attrs(t_tdecl))+String(L",",1)+t_type+String(L")",1));
	m_Emit(String(L" End",4));
	m_Emit(String(L" Method GetValue:Object(i:Object)",33));
	m_Emit(String(L"  Return ",9)+m_Box(t_tdecl->f_type,t_expr));
	m_Emit(String(L" End",4));
	m_Emit(String(L" Method SetValue:Void(i:Object,v:Object)",40));
	m_Emit(String(L"  ",2)+t_expr+String(L"=",1)+m_Unbox(t_tdecl->f_type,String(L"v",1)));
	m_Emit(String(L" End",4));
	m_Emit(String(L"End",3));
	return t_ident;
}
String bb_reflector_Reflector::m_Emit6(bb_decl_GlobalDecl* t_gdecl){
	if(!m_ValidType(t_gdecl->f_type)){
		return String();
	}
	String t_name=m_DeclExpr((t_gdecl),true);
	String t_expr=m_DeclExpr((t_gdecl),false);
	String t_ident=m_Mung(t_name);
	String t_type=m_TypeInfo(t_gdecl->f_type);
	m_Emit(String(L"Class ",6)+t_ident+String(L" Extends GlobalInfo",19));
	m_Emit(String(L" Method New()",13));
	m_Emit(String(L"  Super.New(\"",13)+t_name+String(L"\",",2)+String(m_Attrs(t_gdecl))+String(L",",1)+t_type+String(L")",1));
	m_Emit(String(L" End",4));
	m_Emit(String(L" Method GetValue:Object()",25));
	m_Emit(String(L"  Return ",9)+m_Box(t_gdecl->f_type,t_expr));
	m_Emit(String(L" End",4));
	m_Emit(String(L" Method SetValue:Void(v:Object)",31));
	m_Emit(String(L"  ",2)+t_expr+String(L"=",1)+m_Unbox(t_gdecl->f_type,String(L"v",1)));
	m_Emit(String(L" End",4));
	m_Emit(String(L"End",3));
	return t_ident;
}
int bb_reflector_Reflector::m_Semant3(bb_decl_AppDecl* t_app){
	String t_filter=bb_config_GetCfgVar(String(L"REFLECTION_FILTER",17));
	if(!((t_filter).Length()!=0)){
		return 0;
	}
	t_filter=t_filter.Replace(String(L";",1),String(L"|",1));
	f_debug=bb_config_GetCfgVar(String(L"DEBUG_REFLECTION",16))==String(L"1",1);
	bb_map_ValueEnumerator* t_=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_ModuleDecl* t_mdecl=t_->m_NextObject();
		String t_path=m_ModPath(t_mdecl);
		if(t_path==String(L"reflection",10)){
			gc_assign(f_refmod,t_mdecl);
		}else{
			if(t_path==String(L"monkey.lang",11)){
				gc_assign(f_langmod,t_mdecl);
			}else{
				if(t_path==String(L"monkey.boxes",12)){
					gc_assign(f_boxesmod,t_mdecl);
				}
			}
		}
	}
	if(!((f_refmod)!=0)){
		Error(String(L"reflection module not found!",28));
	}
	if(f_debug){
		Print(String(L"Semanting all",13));
	}
	bb_map_ValueEnumerator* t_2=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_ModuleDecl* t_mdecl2=t_2->m_NextObject();
		String t_path2=m_ModPath(t_mdecl2);
		if(t_mdecl2!=f_boxesmod && t_mdecl2!=f_langmod && !bb_reflector_Reflector::g_MatchPath(t_path2,t_filter)){
			continue;
		}
		String t_expr=m_Mung(t_path2);
		f_refmod->m_InsertDecl((new bb_decl_AliasDecl)->g_new(t_expr,0,(t_mdecl2)));
		f_modpaths->m_Set(t_mdecl2->f_filepath,t_path2);
		f_modexprs->m_Set(t_mdecl2->f_filepath,t_expr);
		f_refmods->m_Insert(t_mdecl2->f_filepath);
		t_mdecl2->m_SemantAll();
	}
	do{
		int t_n=t_app->f_allSemantedDecls->m_Count();
		bb_map_ValueEnumerator* t_3=t_app->f_imported->m_Values()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_ModuleDecl* t_mdecl3=t_3->m_NextObject();
			if(!f_refmods->m_Contains(t_mdecl3->f_filepath)){
				continue;
			}
			t_mdecl3->m_SemantAll();
		}
		t_n=t_app->f_allSemantedDecls->m_Count()-t_n;
		if(!((t_n)!=0)){
			break;
		}
		if(f_debug){
			Print(String(L"Semanting more: ",16)+String(t_n));
		}
	}while(!(false));
	bb_list_Enumerator2* t_4=t_app->f_allSemantedDecls->m_ObjectEnumerator();
	while(t_4->m_HasNext()){
		bb_decl_Decl* t_decl=t_4->m_NextObject();
		if(!f_refmods->m_Contains(t_decl->m_ModuleScope()->f_filepath)){
			continue;
		}
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl);
		if(((t_cdecl)!=0) && m_ValidClass(t_cdecl)){
			f_classids->m_Set4(m_DeclExpr((t_cdecl),true),f_classdecls->m_Length());
			f_classdecls->m_Push19(t_cdecl);
			continue;
		}
	}
	bb_stack_StringStack* t_classes=(new bb_stack_StringStack)->g_new();
	bb_stack_StringStack* t_consts=(new bb_stack_StringStack)->g_new();
	bb_stack_StringStack* t_globals=(new bb_stack_StringStack)->g_new();
	bb_stack_StringStack* t_functions=(new bb_stack_StringStack)->g_new();
	if(f_debug){
		Print(String(L"Generating reflection info",26));
	}
	bb_list_Enumerator2* t_5=t_app->f_allSemantedDecls->m_ObjectEnumerator();
	while(t_5->m_HasNext()){
		bb_decl_Decl* t_decl2=t_5->m_NextObject();
		if(!f_refmods->m_Contains(t_decl2->m_ModuleScope()->f_filepath)){
			continue;
		}
		bb_decl_ConstDecl* t_pdecl=dynamic_cast<bb_decl_ConstDecl*>(t_decl2);
		if((t_pdecl)!=0){
			String t_p=m_Emit2(t_pdecl);
			if((t_p).Length()!=0){
				t_consts->m_Push(t_p);
			}
			continue;
		}
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl2);
		if((t_gdecl)!=0){
			String t_g=m_Emit6(t_gdecl);
			if((t_g).Length()!=0){
				t_globals->m_Push(t_g);
			}
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl2);
		if((t_fdecl)!=0){
			String t_f=m_Emit4(t_fdecl);
			if((t_f).Length()!=0){
				t_functions->m_Push(t_f);
			}
			continue;
		}
	}
	if(f_debug){
		Print(String(L"Finalizing classes",18));
	}
	t_app->m_FinalizeClasses();
	if(f_debug){
		Print(String(L"Generating class reflection info",32));
	}
	for(int t_i=0;t_i<f_classdecls->m_Length();t_i=t_i+1){
		t_classes->m_Push(m_Emit3(f_classdecls->m_Get2(t_i)));
	}
	m_Emit(String(L"Global _init:=__init()",22));
	m_Emit(String(L"Function __init()",17));
	if((t_classes->m_Length())!=0){
		m_Emit(String(L" _classes=New ClassInfo[",24)+String(t_classes->m_Length())+String(L"]",1));
		for(int t_i2=0;t_i2<t_classes->m_Length();t_i2=t_i2+1){
			m_Emit(String(L" _classes[",10)+String(t_i2)+String(L"]=New ",6)+t_classes->m_Get2(t_i2));
		}
		for(int t_i3=0;t_i3<t_classes->m_Length();t_i3=t_i3+1){
			m_Emit(String(L" _classes[",10)+String(t_i3)+String(L"].Init()",8));
		}
	}
	if((t_consts->m_Length())!=0){
		m_Emit(String(L" _consts=new ConstInfo[",23)+String(t_consts->m_Length())+String(L"]",1));
		for(int t_i4=0;t_i4<t_consts->m_Length();t_i4=t_i4+1){
			m_Emit(String(L" _consts[",9)+String(t_i4)+String(L"]=",2)+t_consts->m_Get2(t_i4));
		}
	}
	if((t_globals->m_Length())!=0){
		m_Emit(String(L" _globals=New GlobalInfo[",25)+String(t_globals->m_Length())+String(L"]",1));
		for(int t_i5=0;t_i5<t_globals->m_Length();t_i5=t_i5+1){
			m_Emit(String(L" _globals[",10)+String(t_i5)+String(L"]=New ",6)+t_globals->m_Get2(t_i5));
		}
	}
	if((t_functions->m_Length())!=0){
		m_Emit(String(L" _functions=New FunctionInfo[",29)+String(t_functions->m_Length())+String(L"]",1));
		for(int t_i6=0;t_i6<t_functions->m_Length();t_i6=t_i6+1){
			m_Emit(String(L" _functions[",12)+String(t_i6)+String(L"]=New ",6)+t_functions->m_Get2(t_i6));
		}
	}
	m_Emit(String(L" _getClass=New __GetClass",25));
	m_Emit(String(L"End",3));
	m_Emit(String(L"Class __GetClass Extends _GetClass",34));
	m_Emit(String(L" Method GetClass:ClassInfo(o:Object)",36));
	for(int t_i7=t_classes->m_Length()-1;t_i7>=0;t_i7=t_i7+-1){
		String t_expr2=m_DeclExpr((f_classdecls->m_Get2(t_i7)),false);
		m_Emit(String(L"  If ",5)+t_expr2+String(L"(o)<>Null Return _classes[",26)+String(t_i7)+String(L"]",1));
	}
	m_Emit(String(L"  Return _unknownClass",22));
	m_Emit(String(L" End",4));
	m_Emit(String(L"End",3));
	String t_source=f_output->m_Join(String(L"\n",1));
	int t_attrs=8388608;
	if(f_debug){
		Print(String(L"Reflection source:\n",19)+t_source);
	}else{
		t_attrs|=4194304;
	}
	bb_parser_ParseSource(t_source,t_app,f_refmod,t_attrs);
	f_refmod->m_FindValDecl(String(L"_init",5));
	t_app->m_Semant();
	return 0;
}
void bb_reflector_Reflector::mark(){
	Object::mark();
	gc_mark_q(f_refmod);
	gc_mark_q(f_langmod);
	gc_mark_q(f_boxesmod);
	gc_mark_q(f_munged);
	gc_mark_q(f_modpaths);
	gc_mark_q(f_modexprs);
	gc_mark_q(f_refmods);
	gc_mark_q(f_classdecls);
	gc_mark_q(f_classids);
	gc_mark_q(f_output);
}
bb_map_MapValues::bb_map_MapValues(){
	f_map=0;
}
bb_map_MapValues* bb_map_MapValues::g_new(bb_map_Map3* t_map){
	gc_assign(this->f_map,t_map);
	return this;
}
bb_map_MapValues* bb_map_MapValues::g_new2(){
	return this;
}
bb_map_ValueEnumerator* bb_map_MapValues::m_ObjectEnumerator(){
	return (new bb_map_ValueEnumerator)->g_new(f_map->m_FirstNode());
}
void bb_map_MapValues::mark(){
	Object::mark();
	gc_mark_q(f_map);
}
bb_map_ValueEnumerator::bb_map_ValueEnumerator(){
	f_node=0;
}
bb_map_ValueEnumerator* bb_map_ValueEnumerator::g_new(bb_map_Node3* t_node){
	gc_assign(this->f_node,t_node);
	return this;
}
bb_map_ValueEnumerator* bb_map_ValueEnumerator::g_new2(){
	return this;
}
bool bb_map_ValueEnumerator::m_HasNext(){
	return f_node!=0;
}
bb_decl_ModuleDecl* bb_map_ValueEnumerator::m_NextObject(){
	bb_map_Node3* t_t=f_node;
	gc_assign(f_node,f_node->m_NextNode());
	return t_t->f_value;
}
void bb_map_ValueEnumerator::mark(){
	Object::mark();
	gc_mark_q(f_node);
}
bb_map_Map4::bb_map_Map4(){
	f_root=0;
}
bb_map_Map4* bb_map_Map4::g_new(){
	return this;
}
bb_map_Node4* bb_map_Map4::m_FindNode(String t_key){
	bb_map_Node4* t_node=f_root;
	while((t_node)!=0){
		int t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bool bb_map_Map4::m_Contains(String t_key){
	return m_FindNode(t_key)!=0;
}
int bb_map_Map4::m_Get(String t_key){
	bb_map_Node4* t_node=m_FindNode(t_key);
	if((t_node)!=0){
		return t_node->f_value;
	}
	return 0;
}
int bb_map_Map4::m_RotateLeft4(bb_map_Node4* t_node){
	bb_map_Node4* t_child=t_node->f_right;
	gc_assign(t_node->f_right,t_child->f_left);
	if((t_child->f_left)!=0){
		gc_assign(t_child->f_left->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_left){
			gc_assign(t_node->f_parent->f_left,t_child);
		}else{
			gc_assign(t_node->f_parent->f_right,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_left,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map4::m_RotateRight4(bb_map_Node4* t_node){
	bb_map_Node4* t_child=t_node->f_left;
	gc_assign(t_node->f_left,t_child->f_right);
	if((t_child->f_right)!=0){
		gc_assign(t_child->f_right->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_right){
			gc_assign(t_node->f_parent->f_right,t_child);
		}else{
			gc_assign(t_node->f_parent->f_left,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_right,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map4::m_InsertFixup4(bb_map_Node4* t_node){
	while(((t_node->f_parent)!=0) && t_node->f_parent->f_color==-1 && ((t_node->f_parent->f_parent)!=0)){
		if(t_node->f_parent==t_node->f_parent->f_parent->f_left){
			bb_map_Node4* t_uncle=t_node->f_parent->f_parent->f_right;
			if(((t_uncle)!=0) && t_uncle->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle->f_color=1;
				t_uncle->f_parent->f_color=-1;
				t_node=t_uncle->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_right){
					t_node=t_node->f_parent;
					m_RotateLeft4(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateRight4(t_node->f_parent->f_parent);
			}
		}else{
			bb_map_Node4* t_uncle2=t_node->f_parent->f_parent->f_left;
			if(((t_uncle2)!=0) && t_uncle2->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle2->f_color=1;
				t_uncle2->f_parent->f_color=-1;
				t_node=t_uncle2->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_left){
					t_node=t_node->f_parent;
					m_RotateRight4(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateLeft4(t_node->f_parent->f_parent);
			}
		}
	}
	f_root->f_color=1;
	return 0;
}
bool bb_map_Map4::m_Set4(String t_key,int t_value){
	bb_map_Node4* t_node=f_root;
	bb_map_Node4* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				t_node->f_value=t_value;
				return false;
			}
		}
	}
	t_node=(new bb_map_Node4)->g_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->f_right,t_node);
		}else{
			gc_assign(t_parent->f_left,t_node);
		}
		m_InsertFixup4(t_node);
	}else{
		gc_assign(f_root,t_node);
	}
	return true;
}
void bb_map_Map4::mark(){
	Object::mark();
	gc_mark_q(f_root);
}
bb_map_StringMap4::bb_map_StringMap4(){
}
bb_map_StringMap4* bb_map_StringMap4::g_new(){
	bb_map_Map4::g_new();
	return this;
}
int bb_map_StringMap4::m_Compare(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void bb_map_StringMap4::mark(){
	bb_map_Map4::mark();
}
bb_map_Node4::bb_map_Node4(){
	f_key=String();
	f_right=0;
	f_left=0;
	f_value=0;
	f_color=0;
	f_parent=0;
}
bb_map_Node4* bb_map_Node4::g_new(String t_key,int t_value,int t_color,bb_map_Node4* t_parent){
	this->f_key=t_key;
	this->f_value=t_value;
	this->f_color=t_color;
	gc_assign(this->f_parent,t_parent);
	return this;
}
bb_map_Node4* bb_map_Node4::g_new2(){
	return this;
}
void bb_map_Node4::mark(){
	Object::mark();
	gc_mark_q(f_right);
	gc_mark_q(f_left);
	gc_mark_q(f_parent);
}
bb_list_Enumerator3::bb_list_Enumerator3(){
	f__list=0;
	f__curr=0;
}
bb_list_Enumerator3* bb_list_Enumerator3::g_new(bb_list_List7* t_list){
	gc_assign(f__list,t_list);
	gc_assign(f__curr,t_list->f__head->f__succ);
	return this;
}
bb_list_Enumerator3* bb_list_Enumerator3::g_new2(){
	return this;
}
bool bb_list_Enumerator3::m_HasNext(){
	while(f__curr->f__succ->f__pred!=f__curr){
		gc_assign(f__curr,f__curr->f__succ);
	}
	return f__curr!=f__list->f__head;
}
bb_decl_ClassDecl* bb_list_Enumerator3::m_NextObject(){
	bb_decl_ClassDecl* t_data=f__curr->f__data;
	gc_assign(f__curr,f__curr->f__succ);
	return t_data;
}
void bb_list_Enumerator3::mark(){
	Object::mark();
	gc_mark_q(f__list);
	gc_mark_q(f__curr);
}
bb_stack_Stack7::bb_stack_Stack7(){
	f_data=Array<bb_decl_ClassDecl* >();
	f_length=0;
}
bb_stack_Stack7* bb_stack_Stack7::g_new(){
	return this;
}
bb_stack_Stack7* bb_stack_Stack7::g_new2(Array<bb_decl_ClassDecl* > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack7::m_Length(){
	return f_length;
}
int bb_stack_Stack7::m_Push19(bb_decl_ClassDecl* t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	gc_assign(f_data[f_length],t_value);
	f_length+=1;
	return 0;
}
int bb_stack_Stack7::m_Push20(Array<bb_decl_ClassDecl* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push19(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack7::m_Push21(Array<bb_decl_ClassDecl* > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push19(t_values[t_i]);
	}
	return 0;
}
bb_decl_ClassDecl* bb_stack_Stack7::m_Get2(int t_index){
	return f_data[t_index];
}
void bb_stack_Stack7::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
int bb_parser_ParseSource(String t_source,bb_decl_AppDecl* t_app,bb_decl_ModuleDecl* t_mdecl,int t_defattrs){
	bb_toker_Toker* t_toker=(new bb_toker_Toker)->g_new(String(L"$SOURCE",7),t_source);
	bb_parser_Parser* t_parser=(new bb_parser_Parser)->g_new(t_toker,t_app,t_mdecl,t_defattrs);
	t_parser->m_ParseMain();
	return 0;
}
bb_list_Enumerator4::bb_list_Enumerator4(){
	f__list=0;
	f__curr=0;
}
bb_list_Enumerator4* bb_list_Enumerator4::g_new(bb_list_List5* t_list){
	gc_assign(f__list,t_list);
	gc_assign(f__curr,t_list->f__head->f__succ);
	return this;
}
bb_list_Enumerator4* bb_list_Enumerator4::g_new2(){
	return this;
}
bool bb_list_Enumerator4::m_HasNext(){
	while(f__curr->f__succ->f__pred!=f__curr){
		gc_assign(f__curr,f__curr->f__succ);
	}
	return f__curr!=f__list->f__head;
}
String bb_list_Enumerator4::m_NextObject(){
	String t_data=f__curr->f__data;
	gc_assign(f__curr,f__curr->f__succ);
	return t_data;
}
void bb_list_Enumerator4::mark(){
	Object::mark();
	gc_mark_q(f__list);
	gc_mark_q(f__curr);
}
String bb_os_ExtractExt(String t_path){
	int t_i=t_path.FindLast(String(L".",1));
	if(t_i!=-1 && t_path.Find(String(L"/",1),t_i+1)==-1 && t_path.Find(String(L"\\",1),t_i+1)==-1){
		return t_path.Slice(t_i+1);
	}
	return String();
}
bb_translator_Translator::bb_translator_Translator(){
}
bb_translator_Translator* bb_translator_Translator::g_new(){
	return this;
}
void bb_translator_Translator::mark(){
	Object::mark();
}
bb_translator_Translator* bb_translator__trans;
Array<String > bb_os_LoadDir(String t_path,bool t_recursive,bool t_hidden){
	bb_list_StringList* t_dirs=(new bb_list_StringList)->g_new();
	bb_list_StringList* t_files=(new bb_list_StringList)->g_new();
	t_dirs->m_AddLast5(String());
	while(!t_dirs->m_IsEmpty()){
		String t_dir=t_dirs->m_RemoveFirst();
		Array<String > t_=LoadDir(t_path+String(L"/",1)+t_dir);
		int t_2=0;
		while(t_2<t_.Length()){
			String t_f=t_[t_2];
			t_2=t_2+1;
			if(!t_hidden && t_f.StartsWith(String(L".",1))){
				continue;
			}
			if((t_dir).Length()!=0){
				t_f=t_dir+String(L"/",1)+t_f;
			}
			int t_3=FileType(t_path+String(L"/",1)+t_f);
			if(t_3==1){
				t_files->m_AddLast5(t_f);
			}else{
				if(t_3==2){
					if(t_recursive){
						t_dirs->m_AddLast5(t_f);
					}else{
						t_files->m_AddLast5(t_f);
					}
				}
			}
		}
	}
	return t_files->m_ToArray();
}
int bb_os_DeleteDir(String t_path,bool t_recursive){
	if(!t_recursive){
		return DeleteDir(t_path);
	}
	int t_=FileType(t_path);
	if(t_==0){
		return 1;
	}else{
		if(t_==1){
			return 0;
		}
	}
	Array<String > t_2=LoadDir(t_path);
	int t_3=0;
	while(t_3<t_2.Length()){
		String t_f=t_2[t_3];
		t_3=t_3+1;
		if(t_f==String(L".",1) || t_f==String(L"..",2)){
			continue;
		}
		String t_fpath=t_path+String(L"/",1)+t_f;
		if(FileType(t_fpath)==2){
			if(!((bb_os_DeleteDir(t_fpath,true))!=0)){
				return 0;
			}
		}else{
			if(!((DeleteFile(t_fpath))!=0)){
				return 0;
			}
		}
	}
	return DeleteDir(t_path);
}
int bb_os_CopyDir(String t_srcpath,String t_dstpath,bool t_recursive,bool t_hidden){
	if(FileType(t_srcpath)!=2){
		return 0;
	}
	Array<String > t_files=LoadDir(t_srcpath);
	int t_=FileType(t_dstpath);
	if(t_==0){
		if(!((CreateDir(t_dstpath))!=0)){
			return 0;
		}
	}else{
		if(t_==1){
			return 0;
		}
	}
	Array<String > t_2=t_files;
	int t_3=0;
	while(t_3<t_2.Length()){
		String t_f=t_2[t_3];
		t_3=t_3+1;
		if(!t_hidden && t_f.StartsWith(String(L".",1))){
			continue;
		}
		String t_srcp=t_srcpath+String(L"/",1)+t_f;
		String t_dstp=t_dstpath+String(L"/",1)+t_f;
		int t_4=FileType(t_srcp);
		if(t_4==1){
			if(!((CopyFile(t_srcp,t_dstp))!=0)){
				return 0;
			}
		}else{
			if(t_4==2){
				if(t_recursive && !((bb_os_CopyDir(t_srcp,t_dstp,t_recursive,t_hidden))!=0)){
					return 0;
				}
			}
		}
	}
	return 1;
}
int bbMain(){
	Print(String(L"TRANS monkey compiler V1.42",27));
	bb_trans2_LoadConfig();
	if(AppArgs().Length()<2){
		Print(String(L"TRANS Usage: trans [-update] [-build] [-run] [-clean] [-config=...] [-target=...] [-cfgfile=...] [-modpath=...] <main_monkey_source_file>",137));
		Print(String(L"Valid targets: ",15)+bb_targets_ValidTargets());
		Print(String(L"Valid configs: debug release",28));
		ExitApp(0);
	}
	String t_srcpath=bb_trans2_StripQuotes(AppArgs()[AppArgs().Length()-1].Trim());
	if(FileType(t_srcpath)!=1){
		bb_target_Die(String(L"Invalid source file",19));
	}
	t_srcpath=RealPath(t_srcpath);
	bb_config_ENV_HOST=HostOS();
	bb_target_OPT_MODPATH=String(L".;",2)+bb_os_ExtractDir(t_srcpath)+String(L";",1)+RealPath(bb_os_ExtractDir(AppPath())+String(L"/../modules",11));
	bb_target_Target* t_target=0;
	for(int t_i=1;t_i<AppArgs().Length()-1;t_i=t_i+1){
		String t_arg=AppArgs()[t_i].Trim();
		int t_j=t_arg.Find(String(L"=",1),0);
		if(t_j==-1){
			String t_=t_arg.ToLower();
			if(t_==String(L"-safe",5)){
				bb_config_ENV_SAFEMODE=1;
			}else{
				if(t_==String(L"-clean",6)){
					bb_target_OPT_CLEAN=1;
				}else{
					if(t_==String(L"-check",6)){
						bb_target_OPT_ACTION=3;
					}else{
						if(t_==String(L"-update",7)){
							bb_target_OPT_ACTION=4;
						}else{
							if(t_==String(L"-build",6)){
								bb_target_OPT_ACTION=5;
							}else{
								if(t_==String(L"-run",4)){
									bb_target_OPT_ACTION=6;
								}else{
									bb_target_Die(String(L"Unrecognized command line option: ",34)+t_arg);
								}
							}
						}
					}
				}
			}
			continue;
		}
		String t_lhs=t_arg.Slice(0,t_j);
		String t_rhs=t_arg.Slice(t_j+1);
		if(t_lhs.StartsWith(String(L"-",1))){
			String t_2=t_lhs.ToLower();
			if(t_2==String(L"-cfgfile",8)){
			}else{
				if(t_2==String(L"-output",7)){
					bb_target_OPT_OUTPUT=t_rhs;
				}else{
					if(t_2==String(L"-config",7)){
						String t_3=t_rhs.ToLower();
						if(t_3==String(L"debug",5)){
							bb_target_CASED_CONFIG=String(L"Debug",5);
						}else{
							if(t_3==String(L"release",7)){
								bb_target_CASED_CONFIG=String(L"Release",7);
							}else{
								if(t_3==String(L"profile",7)){
									bb_target_CASED_CONFIG=String(L"Profile",7);
								}else{
									bb_target_Die(String(L"Command line error - invalid config: ",37)+t_rhs);
								}
							}
						}
					}else{
						if(t_2==String(L"-target",7)){
							t_target=bb_targets_SelectTarget(t_rhs.ToLower());
							if(!((t_target)!=0)){
								bb_target_Die(String(L"Command line error - invalid target: ",37)+t_rhs);
							}
						}else{
							if(t_2==String(L"-modpath",8)){
								bb_target_OPT_MODPATH=bb_trans2_StripQuotes(t_rhs);
							}else{
								bb_target_Die(String(L"Unrecognized command line option: ",34)+t_lhs);
							}
						}
					}
				}
			}
		}else{
			if(t_lhs.StartsWith(String(L"+",1))){
				bb_config_SetCfgVar(t_lhs.Slice(1),t_rhs);
			}else{
				bb_target_Die(String(L"Command line arg error: ",24)+t_arg);
			}
		}
	}
	if(!((t_target)!=0)){
		bb_target_Die(String(L"No target specified",19));
	}
	bb_config_ENV_CONFIG=bb_target_CASED_CONFIG.ToLower();
	if(!((bb_target_OPT_ACTION)!=0)){
		bb_target_OPT_ACTION=5;
	}
	t_target->m_Make(t_srcpath);
	return 0;
}
bb_translator_CTranslator::bb_translator_CTranslator(){
	f_funcMungs=(new bb_map_StringMap5)->g_new();
	f_mungedScopes=(new bb_map_StringMap6)->g_new();
	f_indent=String();
	f_lines=(new bb_list_StringList)->g_new();
	f_emitDebugInfo=false;
	f_unreachable=0;
	f_broken=0;
}
bb_translator_CTranslator* bb_translator_CTranslator::g_new(){
	bb_translator_Translator::g_new();
	return this;
}
int bb_translator_CTranslator::m_MungMethodDecl(bb_decl_FuncDecl* t_fdecl){
	if((t_fdecl->f_munged).Length()!=0){
		return 0;
	}
	if((t_fdecl->f_overrides)!=0){
		m_MungMethodDecl(t_fdecl->f_overrides);
		t_fdecl->f_munged=t_fdecl->f_overrides->f_munged;
		return 0;
	}
	bb_decl_FuncDeclList* t_funcs=f_funcMungs->m_Get(t_fdecl->f_ident);
	if((t_funcs)!=0){
		bb_list_Enumerator* t_=t_funcs->m_ObjectEnumerator();
		while(t_->m_HasNext()){
			bb_decl_FuncDecl* t_tdecl=t_->m_NextObject();
			if(t_fdecl->f_argDecls.Length()==t_tdecl->f_argDecls.Length()){
				int t_match=1;
				for(int t_i=0;t_i<t_fdecl->f_argDecls.Length();t_i=t_i+1){
					bb_type_Type* t_ty=t_fdecl->f_argDecls[t_i]->f_type;
					bb_type_Type* t_ty2=t_tdecl->f_argDecls[t_i]->f_type;
					if((t_ty->m_EqualsType(t_ty2))!=0){
						continue;
					}
					t_match=0;
					break;
				}
				if((t_match)!=0){
					t_fdecl->f_munged=t_tdecl->f_munged;
					return 0;
				}
			}
		}
	}else{
		t_funcs=(new bb_decl_FuncDeclList)->g_new();
		f_funcMungs->m_Set5(t_fdecl->f_ident,t_funcs);
	}
	t_fdecl->f_munged=String(L"m_",2)+t_fdecl->f_ident;
	if(!t_funcs->m_IsEmpty()){
		t_fdecl->f_munged=t_fdecl->f_munged+String(t_funcs->m_Count()+1);
	}
	t_funcs->m_AddLast2(t_fdecl);
	return 0;
}
int bb_translator_CTranslator::m_MungDecl(bb_decl_Decl* t_decl){
	if((t_decl->f_munged).Length()!=0){
		return 0;
	}
	bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
	if(((t_fdecl)!=0) && t_fdecl->m_IsMethod()){
		return m_MungMethodDecl(t_fdecl);
	}
	String t_mscope=String();
	String t_cscope=String();
	if((t_decl->m_ClassScope())!=0){
		t_cscope=t_decl->m_ClassScope()->f_munged;
	}
	if((t_decl->m_ModuleScope())!=0){
		t_mscope=t_decl->m_ModuleScope()->f_munged;
	}
	String t_id=t_decl->f_ident;
	String t_munged=String();
	String t_scope=String();
	if((dynamic_cast<bb_decl_LocalDecl*>(t_decl))!=0){
		t_scope=String(L"$",1);
		t_munged=String(L"t_",2)+t_id;
	}else{
		if((dynamic_cast<bb_decl_FieldDecl*>(t_decl))!=0){
			t_scope=t_cscope;
			t_munged=String(L"f_",2)+t_id;
		}else{
			if(((dynamic_cast<bb_decl_GlobalDecl*>(t_decl))!=0) || ((dynamic_cast<bb_decl_FuncDecl*>(t_decl))!=0)){
				if(((t_cscope).Length()!=0) && bb_config_ENV_LANG!=String(L"js",2)){
					t_scope=t_cscope;
					t_munged=String(L"g_",2)+t_id;
				}else{
					if((t_cscope).Length()!=0){
						t_munged=t_cscope+String(L"_",1)+t_id;
					}else{
						t_munged=t_mscope+String(L"_",1)+t_id;
					}
				}
			}else{
				if((dynamic_cast<bb_decl_ClassDecl*>(t_decl))!=0){
					t_munged=t_mscope+String(L"_",1)+t_id;
				}else{
					if((dynamic_cast<bb_decl_ModuleDecl*>(t_decl))!=0){
						t_munged=String(L"bb_",3)+t_id;
					}else{
						Print(String(L"OOPS1",5));
						bb_config_InternalErr(String(L"Internal error",14));
					}
				}
			}
		}
	}
	bb_set_StringSet* t_set=f_mungedScopes->m_Get(t_scope);
	if((t_set)!=0){
		if(t_set->m_Contains(t_munged)){
			int t_id2=1;
			do{
				t_id2+=1;
				String t_t=t_munged+String(t_id2);
				if(t_set->m_Contains(t_t)){
					continue;
				}
				t_munged=t_t;
				break;
			}while(!(false));
		}
	}else{
		if(t_scope==String(L"$",1)){
			Print(String(L"OOPS2",5));
			bb_config_InternalErr(String(L"Internal error",14));
		}
		t_set=(new bb_set_StringSet)->g_new();
		f_mungedScopes->m_Set6(t_scope,t_set);
	}
	t_set->m_Insert(t_munged);
	t_decl->f_munged=t_munged;
	return 0;
}
int bb_translator_CTranslator::m_Emit(String t_t){
	if(!((t_t).Length()!=0)){
		return 0;
	}
	if(t_t.StartsWith(String(L"}",1))){
		f_indent=f_indent.Slice(0,f_indent.Length()-1);
	}
	f_lines->m_AddLast5(f_indent+t_t);
	if(t_t.EndsWith(String(L"{",1))){
		f_indent=f_indent+String(L"\t",1);
	}
	return 0;
}
int bb_translator_CTranslator::m_BeginLocalScope(){
	f_mungedScopes->m_Set6(String(L"$",1),(new bb_set_StringSet)->g_new());
	return 0;
}
String bb_translator_CTranslator::m_Bra(String t_str){
	if(t_str.StartsWith(String(L"(",1)) && t_str.EndsWith(String(L")",1))){
		int t_n=1;
		for(int t_i=1;t_i<t_str.Length()-1;t_i=t_i+1){
			String t_=t_str.Slice(t_i,t_i+1);
			if(t_==String(L"(",1)){
				t_n+=1;
			}else{
				if(t_==String(L")",1)){
					t_n-=1;
					if(!((t_n)!=0)){
						return String(L"(",1)+t_str+String(L")",1);
					}
				}
			}
		}
		if(t_n==1){
			return t_str;
		}
	}
	return String(L"(",1)+t_str+String(L")",1);
}
int bb_translator_CTranslator::m_EmitEnter(bb_decl_FuncDecl* t_func){
	return 0;
}
int bb_translator_CTranslator::m_EmitEnterBlock(){
	return 0;
}
int bb_translator_CTranslator::m_EmitSetErr(String t_errInfo){
	return 0;
}
String bb_translator_CTranslator::m_CreateLocal(bb_expr_Expr* t_expr){
	bb_decl_LocalDecl* t_tmp=(new bb_decl_LocalDecl)->g_new(String(),0,t_expr->f_exprType,t_expr);
	m_MungDecl(t_tmp);
	m_Emit(m_TransLocalDecl(t_tmp->f_munged,t_expr)+String(L";",1));
	return t_tmp->f_munged;
}
String bb_translator_CTranslator::m_TransExprNS(bb_expr_Expr* t_expr){
	if(!t_expr->m_SideEffects()){
		return t_expr->m_Trans();
	}
	return m_CreateLocal(t_expr);
}
int bb_translator_CTranslator::m_EmitLeave(){
	return 0;
}
int bb_translator_CTranslator::m_EmitLeaveBlock(){
	return 0;
}
int bb_translator_CTranslator::m_EmitBlock(bb_decl_BlockDecl* t_block,bool t_realBlock){
	bb_decl_PushEnv(t_block);
	bb_decl_FuncDecl* t_func=dynamic_cast<bb_decl_FuncDecl*>(t_block);
	if((t_func)!=0){
		f_emitDebugInfo=bb_config_ENV_CONFIG!=String(L"release",7);
		if((t_func->f_attrs&4194304)!=0){
			f_emitDebugInfo=false;
		}
		if(f_emitDebugInfo){
			m_EmitEnter(t_func);
		}
	}else{
		if(f_emitDebugInfo && t_realBlock){
			m_EmitEnterBlock();
		}
	}
	bb_list_Enumerator5* t_=t_block->f_stmts->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_stmt_Stmt* t_stmt=t_->m_NextObject();
		bb_config__errInfo=t_stmt->f_errInfo;
		if((f_unreachable)!=0){
			break;
		}
		if(f_emitDebugInfo){
			bb_stmt_ReturnStmt* t_rs=dynamic_cast<bb_stmt_ReturnStmt*>(t_stmt);
			if((t_rs)!=0){
				if((t_rs->f_expr)!=0){
					if((t_stmt->f_errInfo).Length()!=0){
						m_EmitSetErr(t_stmt->f_errInfo);
					}
					String t_t_expr=m_TransExprNS(t_rs->f_expr);
					m_EmitLeave();
					m_Emit(String(L"return ",7)+t_t_expr+String(L";",1));
				}else{
					m_EmitLeave();
					m_Emit(String(L"return;",7));
				}
				f_unreachable=1;
				continue;
			}
			if((t_stmt->f_errInfo).Length()!=0){
				m_EmitSetErr(t_stmt->f_errInfo);
			}
		}
		String t_t=t_stmt->m_Trans();
		if((t_t).Length()!=0){
			m_Emit(t_t+String(L";",1));
		}
	}
	bb_config__errInfo=String();
	int t_unr=f_unreachable;
	f_unreachable=0;
	if((t_unr)!=0){
		if(((t_func)!=0) && bb_config_ENV_LANG==String(L"as",2) && !((dynamic_cast<bb_type_VoidType*>(t_func->f_retType))!=0)){
			if(t_block->f_stmts->m_IsEmpty() || !((dynamic_cast<bb_stmt_ReturnStmt*>(t_block->f_stmts->m_Last()))!=0)){
				m_Emit(String(L"return ",7)+m_TransValue(t_func->f_retType,String())+String(L";",1));
			}
		}
	}else{
		if((t_func)!=0){
			if(f_emitDebugInfo){
				m_EmitLeave();
			}
			if(!((dynamic_cast<bb_type_VoidType*>(t_func->f_retType))!=0)){
				if(t_func->m_IsCtor()){
					m_Emit(String(L"return this;",12));
				}else{
					if((t_func->m_ModuleScope()->m_IsStrict())!=0){
						bb_config__errInfo=t_func->f_errInfo;
						bb_config_Err(String(L"Missing return statement.",25));
					}
					m_Emit(String(L"return ",7)+m_TransValue(t_func->f_retType,String())+String(L";",1));
				}
			}
		}else{
			if(f_emitDebugInfo && t_realBlock){
				m_EmitLeaveBlock();
			}
		}
	}
	bb_decl_PopEnv();
	return t_unr;
}
int bb_translator_CTranslator::m_EndLocalScope(){
	f_mungedScopes->m_Set6(String(L"$",1),0);
	return 0;
}
String bb_translator_CTranslator::m_JoinLines(){
	String t_code=f_lines->m_Join(String(L"\n",1));
	f_lines->m_Clear();
	return t_code;
}
String bb_translator_CTranslator::m_Enquote(String t_str){
	return bb_config_Enquote(t_str,bb_config_ENV_LANG);
}
int bb_translator_CTranslator::m_ExprPri(bb_expr_Expr* t_expr){
	if((dynamic_cast<bb_expr_NewObjectExpr*>(t_expr))!=0){
		return 3;
	}else{
		if((dynamic_cast<bb_expr_UnaryExpr*>(t_expr))!=0){
			String t_=dynamic_cast<bb_expr_UnaryExpr*>(t_expr)->f_op;
			if(t_==String(L"+",1) || t_==String(L"-",1) || t_==String(L"~",1) || t_==String(L"not",3)){
				return 3;
			}
			bb_config_InternalErr(String(L"Internal error",14));
		}else{
			if((dynamic_cast<bb_expr_BinaryExpr*>(t_expr))!=0){
				String t_2=dynamic_cast<bb_expr_BinaryExpr*>(t_expr)->f_op;
				if(t_2==String(L"*",1) || t_2==String(L"/",1) || t_2==String(L"mod",3)){
					return 4;
				}else{
					if(t_2==String(L"+",1) || t_2==String(L"-",1)){
						return 5;
					}else{
						if(t_2==String(L"shl",3) || t_2==String(L"shr",3)){
							return 6;
						}else{
							if(t_2==String(L"<",1) || t_2==String(L"<=",2) || t_2==String(L">",1) || t_2==String(L">=",2)){
								return 7;
							}else{
								if(t_2==String(L"=",1) || t_2==String(L"<>",2)){
									return 8;
								}else{
									if(t_2==String(L"&",1)){
										return 9;
									}else{
										if(t_2==String(L"~",1)){
											return 10;
										}else{
											if(t_2==String(L"|",1)){
												return 11;
											}else{
												if(t_2==String(L"and",3)){
													return 12;
												}else{
													if(t_2==String(L"or",2)){
														return 13;
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
				bb_config_InternalErr(String(L"Internal error",14));
			}
		}
	}
	return 2;
}
String bb_translator_CTranslator::m_TransSubExpr(bb_expr_Expr* t_expr,int t_pri){
	String t_t_expr=t_expr->m_Trans();
	if(m_ExprPri(t_expr)>t_pri){
		t_t_expr=m_Bra(t_t_expr);
	}
	return t_t_expr;
}
String bb_translator_CTranslator::m_TransStmtExpr(bb_expr_StmtExpr* t_expr){
	String t_t=t_expr->f_stmt->m_Trans();
	if((t_t).Length()!=0){
		m_Emit(t_t+String(L";",1));
	}
	return t_expr->f_expr->m_Trans();
}
String bb_translator_CTranslator::m_TransVarExpr(bb_expr_VarExpr* t_expr){
	bb_decl_VarDecl* t_decl=t_expr->f_decl;
	if(t_decl->f_munged.StartsWith(String(L"$",1))){
		return m_TransIntrinsicExpr((t_decl),0,Array<bb_expr_Expr* >());
	}
	if((dynamic_cast<bb_decl_LocalDecl*>(t_decl))!=0){
		return t_decl->f_munged;
	}
	if((dynamic_cast<bb_decl_FieldDecl*>(t_decl))!=0){
		return m_TransField(dynamic_cast<bb_decl_FieldDecl*>(t_decl),0);
	}
	if((dynamic_cast<bb_decl_GlobalDecl*>(t_decl))!=0){
		return m_TransGlobal(dynamic_cast<bb_decl_GlobalDecl*>(t_decl));
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_translator_CTranslator::m_TransMemberVarExpr(bb_expr_MemberVarExpr* t_expr){
	bb_decl_VarDecl* t_decl=t_expr->f_decl;
	if(t_decl->f_munged.StartsWith(String(L"$",1))){
		return m_TransIntrinsicExpr((t_decl),t_expr->f_expr,Array<bb_expr_Expr* >());
	}
	if((dynamic_cast<bb_decl_FieldDecl*>(t_decl))!=0){
		return m_TransField(dynamic_cast<bb_decl_FieldDecl*>(t_decl),t_expr->f_expr);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_translator_CTranslator::m_TransInvokeExpr(bb_expr_InvokeExpr* t_expr){
	bb_decl_FuncDecl* t_decl=t_expr->f_decl;
	String t_t=String();
	if(t_decl->f_munged.StartsWith(String(L"$",1))){
		return m_TransIntrinsicExpr((t_decl),0,t_expr->f_args);
	}
	if((t_decl)!=0){
		return m_TransFunc(t_decl,t_expr->f_args,0);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_translator_CTranslator::m_TransInvokeMemberExpr(bb_expr_InvokeMemberExpr* t_expr){
	bb_decl_FuncDecl* t_decl=t_expr->f_decl;
	String t_t=String();
	if(t_decl->f_munged.StartsWith(String(L"$",1))){
		return m_TransIntrinsicExpr((t_decl),t_expr->f_expr,t_expr->f_args);
	}
	if((t_decl)!=0){
		return m_TransFunc(t_decl,t_expr->f_args,t_expr->f_expr);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_translator_CTranslator::m_TransInvokeSuperExpr(bb_expr_InvokeSuperExpr* t_expr){
	bb_decl_FuncDecl* t_decl=t_expr->f_funcDecl;
	String t_t=String();
	if(t_decl->f_munged.StartsWith(String(L"$",1))){
		return m_TransIntrinsicExpr((t_decl),(t_expr),Array<bb_expr_Expr* >());
	}
	if((t_decl)!=0){
		return m_TransSuperFunc(t_decl,t_expr->f_args);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_translator_CTranslator::m_TransExprStmt(bb_stmt_ExprStmt* t_stmt){
	return t_stmt->f_expr->m_TransStmt();
}
String bb_translator_CTranslator::m_TransAssignOp(String t_op){
	String t_=t_op;
	if(t_==String(L"~=",2)){
		return String(L"^=",2);
	}else{
		if(t_==String(L"mod=",4)){
			return String(L"%=",2);
		}else{
			if(t_==String(L"shl=",4)){
				return String(L"<<=",3);
			}else{
				if(t_==String(L"shr=",4)){
					return String(L">>=",3);
				}
			}
		}
	}
	return t_op;
}
String bb_translator_CTranslator::m_TransAssignStmt2(bb_stmt_AssignStmt* t_stmt){
	return t_stmt->f_lhs->m_TransVar()+m_TransAssignOp(t_stmt->f_op)+t_stmt->f_rhs->m_Trans();
}
String bb_translator_CTranslator::m_TransAssignStmt(bb_stmt_AssignStmt* t_stmt){
	if(!((t_stmt->f_rhs)!=0)){
		return t_stmt->f_lhs->m_Trans();
	}
	if((t_stmt->f_tmp1)!=0){
		m_MungDecl(t_stmt->f_tmp1);
		m_Emit(m_TransLocalDecl(t_stmt->f_tmp1->f_munged,t_stmt->f_tmp1->f_init)+String(L";",1));
	}
	if((t_stmt->f_tmp2)!=0){
		m_MungDecl(t_stmt->f_tmp2);
		m_Emit(m_TransLocalDecl(t_stmt->f_tmp2->f_munged,t_stmt->f_tmp2->f_init)+String(L";",1));
	}
	return m_TransAssignStmt2(t_stmt);
}
String bb_translator_CTranslator::m_TransReturnStmt(bb_stmt_ReturnStmt* t_stmt){
	String t_t=String(L"return",6);
	if((t_stmt->f_expr)!=0){
		t_t=t_t+(String(L" ",1)+t_stmt->f_expr->m_Trans());
	}
	f_unreachable=1;
	return t_t;
}
String bb_translator_CTranslator::m_TransContinueStmt(bb_stmt_ContinueStmt* t_stmt){
	f_unreachable=1;
	return String(L"continue",8);
}
String bb_translator_CTranslator::m_TransBreakStmt(bb_stmt_BreakStmt* t_stmt){
	f_unreachable=1;
	f_broken+=1;
	return String(L"break",5);
}
String bb_translator_CTranslator::m_TransBlock(bb_decl_BlockDecl* t_block){
	m_EmitBlock(t_block,false);
	return String();
}
String bb_translator_CTranslator::m_TransDeclStmt(bb_stmt_DeclStmt* t_stmt){
	bb_decl_LocalDecl* t_decl=dynamic_cast<bb_decl_LocalDecl*>(t_stmt->f_decl);
	if((t_decl)!=0){
		m_MungDecl(t_decl);
		return m_TransLocalDecl(t_decl->f_munged,t_decl->f_init);
	}
	bb_decl_ConstDecl* t_cdecl=dynamic_cast<bb_decl_ConstDecl*>(t_stmt->f_decl);
	if((t_cdecl)!=0){
		return String();
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_translator_CTranslator::m_TransIfStmt(bb_stmt_IfStmt* t_stmt){
	if((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_expr))!=0){
		if((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_expr)->f_value).Length()!=0){
			if(!t_stmt->f_thenBlock->f_stmts->m_IsEmpty()){
				m_Emit(String(L"if(true){",9));
				if((m_EmitBlock(t_stmt->f_thenBlock,true))!=0){
					f_unreachable=1;
				}
				m_Emit(String(L"}",1));
			}
		}else{
			if(!t_stmt->f_elseBlock->f_stmts->m_IsEmpty()){
				m_Emit(String(L"if(true){",9));
				if((m_EmitBlock(t_stmt->f_elseBlock,true))!=0){
					f_unreachable=1;
				}
				m_Emit(String(L"}",1));
			}
		}
	}else{
		if(!t_stmt->f_elseBlock->f_stmts->m_IsEmpty()){
			m_Emit(String(L"if",2)+m_Bra(t_stmt->f_expr->m_Trans())+String(L"{",1));
			int t_unr=m_EmitBlock(t_stmt->f_thenBlock,true);
			m_Emit(String(L"}else{",6));
			int t_unr2=m_EmitBlock(t_stmt->f_elseBlock,true);
			m_Emit(String(L"}",1));
			if(((t_unr)!=0) && ((t_unr2)!=0)){
				f_unreachable=1;
			}
		}else{
			m_Emit(String(L"if",2)+m_Bra(t_stmt->f_expr->m_Trans())+String(L"{",1));
			int t_unr3=m_EmitBlock(t_stmt->f_thenBlock,true);
			m_Emit(String(L"}",1));
		}
	}
	return String();
}
String bb_translator_CTranslator::m_TransWhileStmt(bb_stmt_WhileStmt* t_stmt){
	int t_nbroken=f_broken;
	m_Emit(String(L"while",5)+m_Bra(t_stmt->f_expr->m_Trans())+String(L"{",1));
	int t_unr=m_EmitBlock(t_stmt->f_block,true);
	m_Emit(String(L"}",1));
	if(f_broken==t_nbroken && ((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_expr))!=0) && ((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_expr)->f_value).Length()!=0)){
		f_unreachable=1;
	}
	f_broken=t_nbroken;
	return String();
}
String bb_translator_CTranslator::m_TransRepeatStmt(bb_stmt_RepeatStmt* t_stmt){
	int t_nbroken=f_broken;
	m_Emit(String(L"do{",3));
	int t_unr=m_EmitBlock(t_stmt->f_block,true);
	m_Emit(String(L"}while(!",8)+m_Bra(t_stmt->f_expr->m_Trans())+String(L");",2));
	if(f_broken==t_nbroken && ((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_expr))!=0) && !((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_expr)->f_value).Length()!=0)){
		f_unreachable=1;
	}
	f_broken=t_nbroken;
	return String();
}
String bb_translator_CTranslator::m_TransForStmt(bb_stmt_ForStmt* t_stmt){
	int t_nbroken=f_broken;
	String t_init=t_stmt->f_init->m_Trans();
	String t_expr=t_stmt->f_expr->m_Trans();
	String t_incr=t_stmt->f_incr->m_Trans();
	m_Emit(String(L"for(",4)+t_init+String(L";",1)+t_expr+String(L";",1)+t_incr+String(L"){",2));
	int t_unr=m_EmitBlock(t_stmt->f_block,true);
	m_Emit(String(L"}",1));
	if(f_broken==t_nbroken && ((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_expr))!=0) && ((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_expr)->f_value).Length()!=0)){
		f_unreachable=1;
	}
	f_broken=t_nbroken;
	return String();
}
String bb_translator_CTranslator::m_TransTryStmt(bb_stmt_TryStmt* t_stmt){
	bb_config_Err(String(L"TODO!",5));
	return String();
}
String bb_translator_CTranslator::m_TransThrowStmt(bb_stmt_ThrowStmt* t_stmt){
	f_unreachable=1;
	return String(L"throw ",6)+t_stmt->f_expr->m_Trans();
}
String bb_translator_CTranslator::m_TransUnaryOp(String t_op){
	String t_=t_op;
	if(t_==String(L"+",1)){
		return String(L"+",1);
	}else{
		if(t_==String(L"-",1)){
			return String(L"-",1);
		}else{
			if(t_==String(L"~",1)){
				return t_op;
			}else{
				if(t_==String(L"not",3)){
					return String(L"!",1);
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_translator_CTranslator::m_TransBinaryOp(String t_op,String t_rhs){
	String t_=t_op;
	if(t_==String(L"+",1) || t_==String(L"-",1)){
		if(t_rhs.StartsWith(t_op)){
			return t_op+String(L" ",1);
		}
		return t_op;
	}else{
		if(t_==String(L"*",1) || t_==String(L"/",1)){
			return t_op;
		}else{
			if(t_==String(L"shl",3)){
				return String(L"<<",2);
			}else{
				if(t_==String(L"shr",3)){
					return String(L">>",2);
				}else{
					if(t_==String(L"mod",3)){
						return String(L" % ",3);
					}else{
						if(t_==String(L"and",3)){
							return String(L" && ",4);
						}else{
							if(t_==String(L"or",2)){
								return String(L" || ",4);
							}else{
								if(t_==String(L"=",1)){
									return String(L"==",2);
								}else{
									if(t_==String(L"<>",2)){
										return String(L"!=",2);
									}else{
										if(t_==String(L"<",1) || t_==String(L"<=",2) || t_==String(L">",1) || t_==String(L">=",2)){
											return t_op;
										}else{
											if(t_==String(L"&",1) || t_==String(L"|",1)){
												return t_op;
											}else{
												if(t_==String(L"~",1)){
													return String(L"^",1);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
void bb_translator_CTranslator::mark(){
	bb_translator_Translator::mark();
	gc_mark_q(f_funcMungs);
	gc_mark_q(f_mungedScopes);
	gc_mark_q(f_lines);
}
bb_jstranslator_JsTranslator::bb_jstranslator_JsTranslator(){
}
bb_jstranslator_JsTranslator* bb_jstranslator_JsTranslator::g_new(){
	bb_translator_CTranslator::g_new();
	return this;
}
int bb_jstranslator_JsTranslator::m_EmitFuncDecl(bb_decl_FuncDecl* t_decl){
	m_BeginLocalScope();
	String t_args=String();
	Array<bb_decl_ArgDecl* > t_=t_decl->f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_arg=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_arg);
		if((t_args).Length()!=0){
			t_args=t_args+String(L",",1);
		}
		t_args=t_args+t_arg->f_munged;
	}
	t_args=m_Bra(t_args);
	if(t_decl->m_IsCtor()){
		m_Emit(String(L"function ",9)+t_decl->f_munged+t_args+String(L"{",1));
	}else{
		if(t_decl->m_IsMethod()){
			m_Emit(t_decl->m_ClassScope()->f_munged+String(L".prototype.",11)+t_decl->f_munged+String(L"=function",9)+t_args+String(L"{",1));
		}else{
			m_Emit(String(L"function ",9)+t_decl->f_munged+t_args+String(L"{",1));
		}
	}
	if(!((t_decl->m_IsAbstract())!=0)){
		m_EmitBlock((t_decl),true);
	}
	m_Emit(String(L"}",1));
	m_EndLocalScope();
	return 0;
}
int bb_jstranslator_JsTranslator::m_EmitClassDecl(bb_decl_ClassDecl* t_classDecl){
	if((t_classDecl->m_IsInterface())!=0){
		return 0;
	}
	String t_classid=t_classDecl->f_munged;
	String t_superid=t_classDecl->f_superClass->f_munged;
	m_Emit(String(L"function ",9)+t_classid+String(L"(){",3));
	m_Emit(t_superid+String(L".call(this);",12));
	bb_list_Enumerator2* t_=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		bb_decl_FieldDecl* t_fdecl=dynamic_cast<bb_decl_FieldDecl*>(t_decl);
		if((t_fdecl)!=0){
			m_Emit(String(L"this.",5)+t_fdecl->f_munged+String(L"=",1)+t_fdecl->f_init->m_Trans()+String(L";",1));
		}
	}
	String t_impls=String();
	bb_decl_ClassDecl* t_tdecl=t_classDecl;
	bb_set_StringSet* t_iset=(new bb_set_StringSet)->g_new();
	while((t_tdecl)!=0){
		Array<bb_decl_ClassDecl* > t_2=t_tdecl->f_implmentsAll;
		int t_3=0;
		while(t_3<t_2.Length()){
			bb_decl_ClassDecl* t_iface=t_2[t_3];
			t_3=t_3+1;
			String t_t=t_iface->f_munged;
			if(t_iset->m_Contains(t_t)){
				continue;
			}
			t_iset->m_Insert(t_t);
			if((t_impls).Length()!=0){
				t_impls=t_impls+String(L",",1);
			}
			t_impls=t_impls+(t_t+String(L":1",2));
		}
		t_tdecl=t_tdecl->f_superClass;
	}
	if((t_impls).Length()!=0){
		m_Emit(String(L"this.implments={",16)+t_impls+String(L"};",2));
	}
	m_Emit(String(L"}",1));
	if(t_superid!=String(L"Object",6)){
		m_Emit(t_classid+String(L".prototype=extend_class(",24)+t_superid+String(L");",2));
	}
	bb_list_Enumerator2* t_4=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_4->m_HasNext()){
		bb_decl_Decl* t_decl2=t_4->m_NextObject();
		if((t_decl2->m_IsExtern())!=0){
			continue;
		}
		bb_decl_FuncDecl* t_fdecl2=dynamic_cast<bb_decl_FuncDecl*>(t_decl2);
		if((t_fdecl2)!=0){
			m_EmitFuncDecl(t_fdecl2);
			continue;
		}
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl2);
		if((t_gdecl)!=0){
			m_Emit(String(L"var ",4)+t_gdecl->f_munged+String(L";",1));
			continue;
		}
	}
	return 0;
}
String bb_jstranslator_JsTranslator::m_TransApp(bb_decl_AppDecl* t_app){
	t_app->f_mainFunc->f_munged=String(L"bbMain",6);
	bb_map_ValueEnumerator* t_=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_ModuleDecl* t_decl=t_->m_NextObject();
		m_MungDecl(t_decl);
	}
	bb_list_Enumerator2* t_2=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_Decl* t_decl2=t_2->m_NextObject();
		m_MungDecl(t_decl2);
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl2);
		if(!((t_cdecl)!=0)){
			continue;
		}
		bb_list_Enumerator2* t_3=t_cdecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl3=t_3->m_NextObject();
			m_MungDecl(t_decl3);
		}
	}
	bb_list_Enumerator2* t_4=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_4->m_HasNext()){
		bb_decl_Decl* t_decl4=t_4->m_NextObject();
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl4);
		if((t_gdecl)!=0){
			m_Emit(String(L"var ",4)+t_gdecl->f_munged+String(L";",1));
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl4);
		if((t_fdecl)!=0){
			m_EmitFuncDecl(t_fdecl);
			continue;
		}
		bb_decl_ClassDecl* t_cdecl2=dynamic_cast<bb_decl_ClassDecl*>(t_decl4);
		if((t_cdecl2)!=0){
			m_EmitClassDecl(t_cdecl2);
			continue;
		}
	}
	m_Emit(String(L"function bbInit(){",18));
	bb_list_Enumerator6* t_5=t_app->f_semantedGlobals->m_ObjectEnumerator();
	while(t_5->m_HasNext()){
		bb_decl_GlobalDecl* t_decl5=t_5->m_NextObject();
		m_Emit(t_decl5->f_munged+String(L"=",1)+t_decl5->f_init->m_Trans()+String(L";",1));
	}
	m_Emit(String(L"}",1));
	return m_JoinLines();
}
String bb_jstranslator_JsTranslator::m_TransValue(bb_type_Type* t_ty,String t_value){
	if((t_value).Length()!=0){
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"true",4);
		}
		if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
			return t_value;
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return m_Enquote(t_value);
		}
	}else{
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"false",5);
		}
		if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
			return String(L"0",1);
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return String(L"\"\"",2);
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
			return String(L"[]",2);
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
			return String(L"null",4);
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_jstranslator_JsTranslator::m_TransLocalDecl(String t_munged,bb_expr_Expr* t_init){
	return String(L"var ",4)+t_munged+String(L"=",1)+t_init->m_Trans();
}
int bb_jstranslator_JsTranslator::m_EmitEnter(bb_decl_FuncDecl* t_func){
	m_Emit(String(L"push_err();",11));
	return 0;
}
int bb_jstranslator_JsTranslator::m_EmitSetErr(String t_info){
	m_Emit(String(L"err_info=\"",10)+t_info.Replace(String(L"\\",1),String(L"/",1))+String(L"\";",2));
	return 0;
}
int bb_jstranslator_JsTranslator::m_EmitLeave(){
	m_Emit(String(L"pop_err();",10));
	return 0;
}
String bb_jstranslator_JsTranslator::m_TransGlobal(bb_decl_GlobalDecl* t_decl){
	return t_decl->f_munged;
}
String bb_jstranslator_JsTranslator::m_TransField(bb_decl_FieldDecl* t_decl,bb_expr_Expr* t_lhs){
	String t_t_lhs=String(L"this",4);
	if((t_lhs)!=0){
		t_t_lhs=m_TransSubExpr(t_lhs,2);
		if(bb_config_ENV_CONFIG==String(L"debug",5)){
			t_t_lhs=String(L"dbg_object",10)+m_Bra(t_t_lhs);
		}
	}
	return t_t_lhs+String(L".",1)+t_decl->f_munged;
}
String bb_jstranslator_JsTranslator::m_TransArgs(Array<bb_expr_Expr* > t_args,String t_first){
	String t_t=t_first;
	Array<bb_expr_Expr* > t_=t_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_arg=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_arg->m_Trans();
	}
	return m_Bra(t_t);
}
String bb_jstranslator_JsTranslator::m_TransFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args,bb_expr_Expr* t_lhs){
	if(t_decl->m_IsMethod()){
		String t_t_lhs=String(L"this",4);
		if((t_lhs)!=0){
			t_t_lhs=m_TransSubExpr(t_lhs,2);
		}
		return t_t_lhs+String(L".",1)+t_decl->f_munged+m_TransArgs(t_args,String());
	}
	return t_decl->f_munged+m_TransArgs(t_args,String());
}
String bb_jstranslator_JsTranslator::m_TransSuperFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args){
	if(t_decl->m_IsCtor()){
		return t_decl->f_munged+String(L".call",5)+m_TransArgs(t_args,String(L"this",4));
	}
	return t_decl->f_scope->f_munged+String(L".prototype.",11)+t_decl->f_munged+String(L".call",5)+m_TransArgs(t_args,String(L"this",4));
}
String bb_jstranslator_JsTranslator::m_TransConstExpr(bb_expr_ConstExpr* t_expr){
	return m_TransValue(t_expr->f_exprType,t_expr->f_value);
}
String bb_jstranslator_JsTranslator::m_TransNewObjectExpr(bb_expr_NewObjectExpr* t_expr){
	String t_t=String(L"new ",4)+t_expr->f_classDecl->f_munged;
	if((t_expr->f_ctor)!=0){
		t_t=t_expr->f_ctor->f_munged+String(L".call",5)+m_TransArgs(t_expr->f_args,t_t);
	}else{
		t_t=String(L"(",1)+t_t+String(L")",1);
	}
	return t_t;
}
String bb_jstranslator_JsTranslator::m_TransNewArrayExpr(bb_expr_NewArrayExpr* t_expr){
	String t_texpr=t_expr->f_expr->m_Trans();
	bb_type_Type* t_ty=dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType)->f_elemType;
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"new_bool_array(",15)+t_texpr+String(L")",1);
	}
	if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
		return String(L"new_number_array(",17)+t_texpr+String(L")",1);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"new_string_array(",17)+t_texpr+String(L")",1);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return String(L"new_object_array(",17)+t_texpr+String(L")",1);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return String(L"new_array_array(",16)+t_texpr+String(L")",1);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_jstranslator_JsTranslator::m_TransSelfExpr(bb_expr_SelfExpr* t_expr){
	return String(L"this",4);
}
String bb_jstranslator_JsTranslator::m_TransCastExpr(bb_expr_CastExpr* t_expr){
	bb_type_Type* t_dst=t_expr->f_exprType;
	bb_type_Type* t_src=t_expr->f_expr->f_exprType;
	String t_texpr=m_Bra(t_expr->f_expr->m_Trans());
	if((dynamic_cast<bb_type_BoolType*>(t_dst))!=0){
		if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
			return t_texpr;
		}
		if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=0",3));
		}
		if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=0.0",5));
		}
		if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L".length!=0",10));
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L".length!=0",10));
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=null",6));
		}
	}else{
		if((dynamic_cast<bb_type_IntType*>(t_dst))!=0){
			if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
				return m_Bra(t_texpr+String(L"?1:0",4));
			}
			if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
				return t_texpr;
			}
			if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
				return m_Bra(t_texpr+String(L"|0",2));
			}
			if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
				return String(L"parseInt",8)+m_Bra(t_texpr+String(L",10",3));
			}
		}else{
			if((dynamic_cast<bb_type_FloatType*>(t_dst))!=0){
				if((dynamic_cast<bb_type_NumericType*>(t_src))!=0){
					return t_texpr;
				}
				if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
					return String(L"parseFloat",10)+t_texpr;
				}
			}else{
				if((dynamic_cast<bb_type_StringType*>(t_dst))!=0){
					if((dynamic_cast<bb_type_NumericType*>(t_src))!=0){
						return String(L"String",6)+t_texpr;
					}
					if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
						return t_texpr;
					}
				}else{
					if(((dynamic_cast<bb_type_ObjectType*>(t_dst))!=0) && ((dynamic_cast<bb_type_ObjectType*>(t_src))!=0)){
						if((t_src->m_GetClass()->m_ExtendsClass(t_dst->m_GetClass()))!=0){
							return t_texpr;
						}else{
							if((t_dst->m_GetClass()->m_IsInterface())!=0){
								return String(L"object_implements",17)+m_Bra(t_texpr+String(L",\"",2)+t_dst->m_GetClass()->f_munged+String(L"\"",1));
							}else{
								return String(L"object_downcast",15)+m_Bra(t_texpr+String(L",",1)+t_dst->m_GetClass()->f_munged);
							}
						}
					}
				}
			}
		}
	}
	bb_config_Err(String(L"JS translator can't convert ",28)+t_src->m_ToString()+String(L" to ",4)+t_dst->m_ToString());
	return String();
}
String bb_jstranslator_JsTranslator::m_TransUnaryExpr(bb_expr_UnaryExpr* t_expr){
	int t_pri=m_ExprPri(t_expr);
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,t_pri);
	return m_TransUnaryOp(t_expr->f_op)+t_t_expr;
}
String bb_jstranslator_JsTranslator::m_TransBinaryExpr(bb_expr_BinaryExpr* t_expr){
	int t_pri=m_ExprPri(t_expr);
	String t_t_lhs=m_TransSubExpr(t_expr->f_lhs,t_pri);
	String t_t_rhs=m_TransSubExpr(t_expr->f_rhs,t_pri-1);
	String t_t_expr=t_t_lhs+m_TransBinaryOp(t_expr->f_op,t_t_rhs)+t_t_rhs;
	if(t_expr->f_op==String(L"/",1) && ((dynamic_cast<bb_type_IntType*>(t_expr->f_exprType))!=0)){
		t_t_expr=m_Bra(m_Bra(t_t_expr)+String(L"|0",2));
	}
	return t_t_expr;
}
String bb_jstranslator_JsTranslator::m_TransIndexExpr(bb_expr_IndexExpr* t_expr){
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,2);
	if((dynamic_cast<bb_type_StringType*>(t_expr->f_expr->f_exprType))!=0){
		String t_t_index=t_expr->f_index->m_Trans();
		return t_t_expr+String(L".charCodeAt(",12)+t_t_index+String(L")",1);
	}else{
		if(bb_config_ENV_CONFIG==String(L"debug",5)){
			String t_t_index2=t_expr->f_index->m_Trans();
			return String(L"dbg_array(",10)+t_t_expr+String(L",",1)+t_t_index2+String(L")[dbg_index]",12);
		}else{
			String t_t_index3=t_expr->f_index->m_Trans();
			return t_t_expr+String(L"[",1)+t_t_index3+String(L"]",1);
		}
	}
}
String bb_jstranslator_JsTranslator::m_TransSliceExpr(bb_expr_SliceExpr* t_expr){
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,2);
	String t_t_args=String(L"0",1);
	if((t_expr->f_from)!=0){
		t_t_args=t_expr->f_from->m_Trans();
	}
	if((t_expr->f_term)!=0){
		t_t_args=t_t_args+(String(L",",1)+t_expr->f_term->m_Trans());
	}
	return t_t_expr+String(L".slice(",7)+t_t_args+String(L")",1);
}
String bb_jstranslator_JsTranslator::m_TransArrayExpr(bb_expr_ArrayExpr* t_expr){
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_expr->f_exprs;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_elem=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_elem->m_Trans();
	}
	return String(L"[",1)+t_t+String(L"]",1);
}
String bb_jstranslator_JsTranslator::m_TransTryStmt(bb_stmt_TryStmt* t_stmt){
	m_Emit(String(L"try{",4));
	int t_unr=m_EmitBlock(t_stmt->f_block,true);
	m_Emit(String(L"}catch(_eek_){",14));
	for(int t_i=0;t_i<t_stmt->f_catches.Length();t_i=t_i+1){
		bb_stmt_CatchStmt* t_c=t_stmt->f_catches[t_i];
		m_MungDecl(t_c->f_init);
		if((t_i)!=0){
			m_Emit(String(L"}else if(",9)+t_c->f_init->f_munged+String(L"=object_downcast(_eek_,",23)+t_c->f_init->f_type->m_GetClass()->f_munged+String(L")){",3));
		}else{
			m_Emit(String(L"if(",3)+t_c->f_init->f_munged+String(L"=object_downcast(_eek_,",23)+t_c->f_init->f_type->m_GetClass()->f_munged+String(L")){",3));
		}
		int t_unr2=m_EmitBlock(t_c->f_block,true);
	}
	m_Emit(String(L"}else{",6));
	m_Emit(String(L"throw _eek_;",12));
	m_Emit(String(L"}",1));
	m_Emit(String(L"}",1));
	return String();
}
String bb_jstranslator_JsTranslator::m_TransAssignStmt(bb_stmt_AssignStmt* t_stmt){
	if(bb_config_ENV_CONFIG==String(L"debug",5)){
		bb_expr_IndexExpr* t_ie=dynamic_cast<bb_expr_IndexExpr*>(t_stmt->f_lhs);
		if((t_ie)!=0){
			String t_t_rhs=t_stmt->f_rhs->m_Trans();
			String t_t_expr=t_ie->f_expr->m_Trans();
			String t_t_index=t_ie->f_index->m_Trans();
			m_Emit(String(L"dbg_array(",10)+t_t_expr+String(L",",1)+t_t_index+String(L")[dbg_index]",12)+m_TransAssignOp(t_stmt->f_op)+t_t_rhs);
			return String();
		}
	}
	return bb_translator_CTranslator::m_TransAssignStmt(t_stmt);
}
String bb_jstranslator_JsTranslator::m_TransIntrinsicExpr(bb_decl_Decl* t_decl,bb_expr_Expr* t_expr,Array<bb_expr_Expr* > t_args){
	String t_texpr=String();
	String t_arg0=String();
	String t_arg1=String();
	String t_arg2=String();
	if((t_expr)!=0){
		t_texpr=m_TransSubExpr(t_expr,2);
	}
	if(t_args.Length()>0 && ((t_args[0])!=0)){
		t_arg0=t_args[0]->m_Trans();
	}
	if(t_args.Length()>1 && ((t_args[1])!=0)){
		t_arg1=t_args[1]->m_Trans();
	}
	if(t_args.Length()>2 && ((t_args[2])!=0)){
		t_arg2=t_args[2]->m_Trans();
	}
	String t_id=t_decl->f_munged.Slice(1);
	String t_=t_id;
	if(t_==String(L"print",5)){
		return String(L"print",5)+m_Bra(t_arg0);
	}else{
		if(t_==String(L"error",5)){
			return String(L"error",5)+m_Bra(t_arg0);
		}else{
			if(t_==String(L"debuglog",8)){
				return String(L"debugLog",8)+m_Bra(t_arg0);
			}else{
				if(t_==String(L"debugstop",9)){
					return String(L"debugStop()",11);
				}else{
					if(t_==String(L"length",6)){
						return t_texpr+String(L".length",7);
					}else{
						if(t_==String(L"resize",6)){
							bb_type_Type* t_ty=dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType)->f_elemType;
							if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
								return String(L"resize_bool_array",17)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
								return String(L"resize_number_array",19)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
								return String(L"resize_string_array",19)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
								return String(L"resize_array_array",18)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
								return String(L"resize_object_array",19)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							bb_config_InternalErr(String(L"Internal error",14));
						}else{
							if(t_==String(L"compare",7)){
								return String(L"string_compare",14)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}else{
								if(t_==String(L"find",4)){
									return t_texpr+String(L".indexOf",8)+m_Bra(t_arg0+String(L",",1)+t_arg1);
								}else{
									if(t_==String(L"findlast",8)){
										return t_texpr+String(L".lastIndexOf",12)+m_Bra(t_arg0);
									}else{
										if(t_==String(L"findlast2",9)){
											return t_texpr+String(L".lastIndexOf",12)+m_Bra(t_arg0+String(L",",1)+t_arg1);
										}else{
											if(t_==String(L"trim",4)){
												return String(L"string_trim",11)+m_Bra(t_texpr);
											}else{
												if(t_==String(L"join",4)){
													return t_arg0+String(L".join",5)+m_Bra(t_texpr);
												}else{
													if(t_==String(L"split",5)){
														return t_texpr+String(L".split",6)+m_Bra(t_arg0);
													}else{
														if(t_==String(L"replace",7)){
															return String(L"string_replace",14)+m_Bra(t_texpr+String(L",",1)+t_arg0+String(L",",1)+t_arg1);
														}else{
															if(t_==String(L"tolower",7)){
																return t_texpr+String(L".toLowerCase()",14);
															}else{
																if(t_==String(L"toupper",7)){
																	return t_texpr+String(L".toUpperCase()",14);
																}else{
																	if(t_==String(L"contains",8)){
																		return m_Bra(t_texpr+String(L".indexOf",8)+m_Bra(t_arg0)+String(L"!=-1",4));
																	}else{
																		if(t_==String(L"startswith",10)){
																			return String(L"string_startswith",17)+m_Bra(t_texpr+String(L",",1)+t_arg0);
																		}else{
																			if(t_==String(L"endswith",8)){
																				return String(L"string_endswith",15)+m_Bra(t_texpr+String(L",",1)+t_arg0);
																			}else{
																				if(t_==String(L"tochars",7)){
																					return String(L"string_tochars",14)+m_Bra(t_texpr);
																				}else{
																					if(t_==String(L"fromchar",8)){
																						return String(L"String.fromCharCode",19)+m_Bra(t_arg0);
																					}else{
																						if(t_==String(L"fromchars",9)){
																							return String(L"string_fromchars",16)+m_Bra(t_arg0);
																						}else{
																							if(t_==String(L"sin",3) || t_==String(L"cos",3) || t_==String(L"tan",3)){
																								return String(L"Math.",5)+t_id+m_Bra(m_Bra(t_arg0)+String(L"*D2R",4));
																							}else{
																								if(t_==String(L"asin",4) || t_==String(L"acos",4) || t_==String(L"atan",4)){
																									return m_Bra(String(L"Math.",5)+t_id+m_Bra(t_arg0)+String(L"*R2D",4));
																								}else{
																									if(t_==String(L"atan2",5)){
																										return m_Bra(String(L"Math.",5)+t_id+m_Bra(t_arg0+String(L",",1)+t_arg1)+String(L"*R2D",4));
																									}else{
																										if(t_==String(L"sinr",4) || t_==String(L"cosr",4) || t_==String(L"tanr",4)){
																											return String(L"Math.",5)+t_id.Slice(0,-1)+m_Bra(t_arg0);
																										}else{
																											if(t_==String(L"asinr",5) || t_==String(L"acosr",5) || t_==String(L"atanr",5)){
																												return String(L"Math.",5)+t_id.Slice(0,-1)+m_Bra(t_arg0);
																											}else{
																												if(t_==String(L"atan2r",6)){
																													return String(L"Math.",5)+t_id.Slice(0,-1)+m_Bra(t_arg0+String(L",",1)+t_arg1);
																												}else{
																													if(t_==String(L"sqrt",4) || t_==String(L"floor",5) || t_==String(L"ceil",4) || t_==String(L"log",3) || t_==String(L"exp",3)){
																														return String(L"Math.",5)+t_id+m_Bra(t_arg0);
																													}else{
																														if(t_==String(L"pow",3)){
																															return String(L"Math.",5)+t_id+m_Bra(t_arg0+String(L",",1)+t_arg1);
																														}
																													}
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
void bb_jstranslator_JsTranslator::mark(){
	bb_translator_CTranslator::mark();
}
bool bb_target_MatchPath(String t_text,String t_pattern){
	t_text=String(L"/",1)+t_text;
	Array<String > t_alts=t_pattern.Split(String(L"|",1));
	Array<String > t_=t_alts;
	int t_2=0;
	while(t_2<t_.Length()){
		String t_alt=t_[t_2];
		t_2=t_2+1;
		if(!((t_alt).Length()!=0)){
			continue;
		}
		Array<String > t_bits=t_alt.Split(String(L"*",1));
		if(t_bits.Length()==1){
			if(t_bits[0]==t_text){
				return true;
			}
			continue;
		}
		if(!t_text.StartsWith(t_bits[0])){
			continue;
		}
		int t_i=t_bits[0].Length();
		for(int t_j=1;t_j<t_bits.Length()-1;t_j=t_j+1){
			String t_bit=t_bits[t_j];
			t_i=t_text.Find(t_bit,t_i);
			if(t_i==-1){
				break;
			}
			t_i+=t_bit.Length();
		}
		if(t_i!=-1 && t_text.Slice(t_i).EndsWith(t_bits[t_bits.Length()-1])){
			return true;
		}
	}
	return false;
}
String bb_target_ReplaceBlock(String t_text,String t_tag,String t_repText,String t_mark){
	String t_beginTag=t_mark+String(L"${",2)+t_tag+String(L"_BEGIN}",7);
	int t_i=t_text.Find(t_beginTag,0);
	if(t_i==-1){
		bb_target_Die(String(L"Error updating target project - can't find block begin tag '",60)+t_tag+String(L"'. You may need to delete target .build directory.",50));
	}
	t_i+=t_beginTag.Length();
	while(t_i<t_text.Length() && (int)t_text[t_i-1]!=10){
		t_i+=1;
	}
	String t_endTag=t_mark+String(L"${",2)+t_tag+String(L"_END}",5);
	int t_i2=t_text.Find(t_endTag,t_i-1);
	if(t_i2==-1){
		bb_target_Die(String(L"Error updating target project - can't find block end tag '",58)+t_tag+String(L"'.",2));
	}
	if(!((t_repText).Length()!=0) || (int)t_repText[t_repText.Length()-1]==10){
		t_i2+=1;
	}
	return t_text.Slice(0,t_i)+t_repText+t_text.Slice(t_i2);
}
String bb_config_Enquote(String t_str,String t_lang){
	String t_=t_lang;
	if(t_==String(L"cpp",3) || t_==String(L"java",4) || t_==String(L"as",2) || t_==String(L"js",2) || t_==String(L"cs",2)){
		t_str=t_str.Replace(String(L"\\",1),String(L"\\\\",2));
		t_str=t_str.Replace(String(L"\"",1),String(L"\\\"",2));
		t_str=t_str.Replace(String(L"\n",1),String(L"\\n",2));
		t_str=t_str.Replace(String(L"\r",1),String(L"\\r",2));
		t_str=t_str.Replace(String(L"\t",1),String(L"\\t",2));
		for(int t_i=0;t_i<t_str.Length();t_i=t_i+1){
			if((int)t_str[t_i]>=32 && (int)t_str[t_i]<128){
				continue;
			}
			String t_t=String();
			int t_n=(int)t_str[t_i];
			while((t_n)!=0){
				int t_c=(t_n&15)+48;
				if(t_c>=58){
					t_c+=39;
				}
				t_t=String((Char)(t_c),1)+t_t;
				t_n=t_n>>4&268435455;
			}
			if(!((t_t).Length()!=0)){
				t_t=String(L"0",1);
			}
			String t_2=t_lang;
			if(t_2==String(L"cpp",3)){
				t_t=String(L"\"L\"\\x",5)+t_t+String(L"\"L\"",3);
			}else{
				t_t=String(L"\\u",2)+(String(L"0000",4)+t_t).Slice(-4);
			}
			t_str=t_str.Slice(0,t_i)+t_t+t_str.Slice(t_i+1);
			t_i+=t_t.Length()-1;
		}
		String t_3=t_lang;
		if(t_3==String(L"cpp",3)){
			t_str=String(L"L\"",2)+t_str+String(L"\"",1);
		}else{
			t_str=String(L"\"",1)+t_str+String(L"\"",1);
		}
		return t_str;
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
bb_astranslator_AsTranslator::bb_astranslator_AsTranslator(){
}
bb_astranslator_AsTranslator* bb_astranslator_AsTranslator::g_new(){
	bb_translator_CTranslator::g_new();
	return this;
}
String bb_astranslator_AsTranslator::m_TransValue(bb_type_Type* t_ty,String t_value){
	if((t_value).Length()!=0){
		if(((dynamic_cast<bb_type_IntType*>(t_ty))!=0) && t_value.StartsWith(String(L"$",1))){
			return String(L"0x",2)+t_value.Slice(1);
		}
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"true",4);
		}
		if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
			return t_value;
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return m_Enquote(t_value);
		}
	}else{
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"false",5);
		}
		if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
			return String(L"0",1);
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return String(L"\"\"",2);
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
			return String(L"[]",2);
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
			return String(L"null",4);
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_astranslator_AsTranslator::m_TransType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_VoidType*>(t_ty))!=0){
		return String(L"void",4);
	}
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"Boolean",7);
	}
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		return String(L"int",3);
	}
	if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
		return String(L"Number",6);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"String",6);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return String(L"Array",5);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return dynamic_cast<bb_type_ObjectType*>(t_ty)->f_classDecl->f_munged;
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_astranslator_AsTranslator::m_TransLocalDecl(String t_munged,bb_expr_Expr* t_init){
	return String(L"var ",4)+t_munged+String(L":",1)+m_TransType(t_init->f_exprType)+String(L"=",1)+t_init->m_Trans();
}
int bb_astranslator_AsTranslator::m_EmitEnter(bb_decl_FuncDecl* t_func){
	m_Emit(String(L"pushErr();",10));
	return 0;
}
int bb_astranslator_AsTranslator::m_EmitSetErr(String t_info){
	m_Emit(String(L"_errInfo=\"",10)+t_info.Replace(String(L"\\",1),String(L"/",1))+String(L"\";",2));
	return 0;
}
int bb_astranslator_AsTranslator::m_EmitLeave(){
	m_Emit(String(L"popErr();",9));
	return 0;
}
String bb_astranslator_AsTranslator::m_TransValDecl(bb_decl_ValDecl* t_decl){
	return t_decl->f_munged+String(L":",1)+m_TransType(t_decl->f_type);
}
int bb_astranslator_AsTranslator::m_EmitFuncDecl(bb_decl_FuncDecl* t_decl){
	m_BeginLocalScope();
	String t_args=String();
	Array<bb_decl_ArgDecl* > t_=t_decl->f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_arg=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_arg);
		if((t_args).Length()!=0){
			t_args=t_args+String(L",",1);
		}
		t_args=t_args+m_TransValDecl(t_arg);
	}
	String t_t=String(L"function ",9)+t_decl->f_munged+m_Bra(t_args)+String(L":",1)+m_TransType(t_decl->f_retType);
	bb_decl_ClassDecl* t_cdecl=t_decl->m_ClassScope();
	if(((t_cdecl)!=0) && ((t_cdecl->m_IsInterface())!=0)){
		m_Emit(t_t+String(L";",1));
	}else{
		String t_q=String(L"internal ",9);
		if((t_cdecl)!=0){
			t_q=String(L"public ",7);
			if(t_decl->m_IsStatic()){
				t_q=t_q+String(L"static ",7);
			}
			if((t_decl->f_overrides)!=0){
				t_q=t_q+String(L"override ",9);
			}
		}
		m_Emit(t_q+t_t+String(L"{",1));
		if((t_decl->m_IsAbstract())!=0){
			if((dynamic_cast<bb_type_VoidType*>(t_decl->f_retType))!=0){
				m_Emit(String(L"return;",7));
			}else{
				m_Emit(String(L"return ",7)+m_TransValue(t_decl->f_retType,String())+String(L";",1));
			}
		}else{
			m_EmitBlock((t_decl),true);
		}
		m_Emit(String(L"}",1));
	}
	m_EndLocalScope();
	return 0;
}
int bb_astranslator_AsTranslator::m_EmitClassDecl(bb_decl_ClassDecl* t_classDecl){
	String t_classid=t_classDecl->f_munged;
	String t_superid=t_classDecl->f_superClass->f_munged;
	if((t_classDecl->m_IsInterface())!=0){
		String t_bases=String();
		Array<bb_decl_ClassDecl* > t_=t_classDecl->f_implments;
		int t_2=0;
		while(t_2<t_.Length()){
			bb_decl_ClassDecl* t_iface=t_[t_2];
			t_2=t_2+1;
			if((t_bases).Length()!=0){
				t_bases=t_bases+String(L",",1);
			}else{
				t_bases=String(L" extends ",9);
			}
			t_bases=t_bases+t_iface->f_munged;
		}
		m_Emit(String(L"interface ",10)+t_classid+t_bases+String(L"{",1));
		bb_list_Enumerator2* t_3=t_classDecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl=t_3->m_NextObject();
			bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
			if(!((t_fdecl)!=0)){
				continue;
			}
			m_EmitFuncDecl(t_fdecl);
		}
		m_Emit(String(L"}",1));
		return 0;
	}
	String t_bases2=String();
	Array<bb_decl_ClassDecl* > t_4=t_classDecl->f_implments;
	int t_5=0;
	while(t_5<t_4.Length()){
		bb_decl_ClassDecl* t_iface2=t_4[t_5];
		t_5=t_5+1;
		if((t_bases2).Length()!=0){
			t_bases2=t_bases2+String(L",",1);
		}else{
			t_bases2=String(L" implements ",12);
		}
		t_bases2=t_bases2+t_iface2->f_munged;
	}
	m_Emit(String(L"class ",6)+t_classid+String(L" extends ",9)+t_superid+t_bases2+String(L"{",1));
	bb_list_Enumerator2* t_6=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_6->m_HasNext()){
		bb_decl_Decl* t_decl2=t_6->m_NextObject();
		bb_decl_FieldDecl* t_tdecl=dynamic_cast<bb_decl_FieldDecl*>(t_decl2);
		if((t_tdecl)!=0){
			m_Emit(String(L"internal var ",13)+m_TransValDecl(t_tdecl)+String(L"=",1)+t_tdecl->f_init->m_Trans()+String(L";",1));
			continue;
		}
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl2);
		if((t_gdecl)!=0){
			m_Emit(String(L"internal static var ",20)+m_TransValDecl(t_gdecl)+String(L";",1));
			continue;
		}
		bb_decl_FuncDecl* t_fdecl2=dynamic_cast<bb_decl_FuncDecl*>(t_decl2);
		if((t_fdecl2)!=0){
			m_EmitFuncDecl(t_fdecl2);
			continue;
		}
	}
	m_Emit(String(L"}",1));
	return 0;
}
String bb_astranslator_AsTranslator::m_TransStatic(bb_decl_Decl* t_decl){
	if((t_decl->m_IsExtern())!=0){
		return t_decl->f_munged;
	}else{
		if(((bb_decl__env)!=0) && ((t_decl->f_scope)!=0) && t_decl->f_scope==(bb_decl__env->m_ClassScope())){
			return t_decl->f_munged;
		}else{
			if((dynamic_cast<bb_decl_ClassDecl*>(t_decl->f_scope))!=0){
				return t_decl->f_scope->f_munged+String(L".",1)+t_decl->f_munged;
			}else{
				if((dynamic_cast<bb_decl_ModuleDecl*>(t_decl->f_scope))!=0){
					return t_decl->f_munged;
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_astranslator_AsTranslator::m_TransGlobal(bb_decl_GlobalDecl* t_decl){
	return m_TransStatic(t_decl);
}
String bb_astranslator_AsTranslator::m_TransApp(bb_decl_AppDecl* t_app){
	t_app->f_mainFunc->f_munged=String(L"bbMain",6);
	bb_map_ValueEnumerator* t_=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_ModuleDecl* t_decl=t_->m_NextObject();
		m_MungDecl(t_decl);
	}
	bb_list_Enumerator2* t_2=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_Decl* t_decl2=t_2->m_NextObject();
		m_MungDecl(t_decl2);
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl2);
		if(!((t_cdecl)!=0)){
			continue;
		}
		bb_list_Enumerator2* t_3=t_cdecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl3=t_3->m_NextObject();
			if(((dynamic_cast<bb_decl_FuncDecl*>(t_decl3))!=0) && dynamic_cast<bb_decl_FuncDecl*>(t_decl3)->m_IsCtor()){
				t_decl3->f_ident=t_cdecl->f_ident+String(L"_",1)+t_decl3->f_ident;
			}
			m_MungDecl(t_decl3);
		}
	}
	bb_list_Enumerator2* t_4=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_4->m_HasNext()){
		bb_decl_Decl* t_decl4=t_4->m_NextObject();
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl4);
		if((t_gdecl)!=0){
			m_Emit(String(L"var ",4)+m_TransValDecl(t_gdecl)+String(L";",1));
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl4);
		if((t_fdecl)!=0){
			m_EmitFuncDecl(t_fdecl);
			continue;
		}
		bb_decl_ClassDecl* t_cdecl2=dynamic_cast<bb_decl_ClassDecl*>(t_decl4);
		if((t_cdecl2)!=0){
			m_EmitClassDecl(t_cdecl2);
			continue;
		}
	}
	m_BeginLocalScope();
	m_Emit(String(L"function bbInit():void{",23));
	bb_list_Enumerator6* t_5=t_app->f_semantedGlobals->m_ObjectEnumerator();
	while(t_5->m_HasNext()){
		bb_decl_GlobalDecl* t_decl5=t_5->m_NextObject();
		m_Emit(m_TransGlobal(t_decl5)+String(L"=",1)+t_decl5->f_init->m_Trans()+String(L";",1));
	}
	m_Emit(String(L"}",1));
	m_EndLocalScope();
	return m_JoinLines();
}
String bb_astranslator_AsTranslator::m_TransField(bb_decl_FieldDecl* t_decl,bb_expr_Expr* t_lhs){
	String t_t_lhs=String(L"this",4);
	if((t_lhs)!=0){
		t_t_lhs=m_TransSubExpr(t_lhs,2);
		if(bb_config_ENV_CONFIG==String(L"debug",5)){
			t_t_lhs=String(L"dbg_object",10)+m_Bra(t_t_lhs);
		}
	}
	return t_t_lhs+String(L".",1)+t_decl->f_munged;
}
String bb_astranslator_AsTranslator::m_TransArgs2(Array<bb_expr_Expr* > t_args){
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_arg=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_arg->m_Trans();
	}
	return m_Bra(t_t);
}
String bb_astranslator_AsTranslator::m_TransFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args,bb_expr_Expr* t_lhs){
	if(t_decl->m_IsMethod()){
		String t_t_lhs=String(L"this",4);
		if((t_lhs)!=0){
			t_t_lhs=m_TransSubExpr(t_lhs,2);
		}
		return t_t_lhs+String(L".",1)+t_decl->f_munged+m_TransArgs2(t_args);
	}
	return m_TransStatic(t_decl)+m_TransArgs2(t_args);
}
String bb_astranslator_AsTranslator::m_TransSuperFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args){
	return String(L"super.",6)+t_decl->f_munged+m_TransArgs2(t_args);
}
String bb_astranslator_AsTranslator::m_TransConstExpr(bb_expr_ConstExpr* t_expr){
	return m_TransValue(t_expr->f_exprType,t_expr->f_value);
}
String bb_astranslator_AsTranslator::m_TransNewObjectExpr(bb_expr_NewObjectExpr* t_expr){
	String t_t=String(L"(new ",5)+t_expr->f_classDecl->f_munged+String(L")",1);
	if((t_expr->f_ctor)!=0){
		t_t=t_t+(String(L".",1)+t_expr->f_ctor->f_munged+m_TransArgs2(t_expr->f_args));
	}
	return t_t;
}
String bb_astranslator_AsTranslator::m_TransNewArrayExpr(bb_expr_NewArrayExpr* t_expr){
	String t_texpr=t_expr->f_expr->m_Trans();
	bb_type_Type* t_ty=t_expr->f_ty;
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"new_bool_array(",15)+t_texpr+String(L")",1);
	}
	if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
		return String(L"new_number_array(",17)+t_texpr+String(L")",1);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"new_string_array(",17)+t_texpr+String(L")",1);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return String(L"new_object_array(",17)+t_texpr+String(L")",1);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return String(L"new_array_array(",16)+t_texpr+String(L")",1);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_astranslator_AsTranslator::m_TransSelfExpr(bb_expr_SelfExpr* t_expr){
	return String(L"this",4);
}
String bb_astranslator_AsTranslator::m_TransCastExpr(bb_expr_CastExpr* t_expr){
	bb_type_Type* t_dst=t_expr->f_exprType;
	bb_type_Type* t_src=t_expr->f_expr->f_exprType;
	String t_texpr=m_Bra(t_expr->f_expr->m_Trans());
	if((dynamic_cast<bb_type_BoolType*>(t_dst))!=0){
		if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
			return t_texpr;
		}
		if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=0",3));
		}
		if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=0.0",5));
		}
		if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L".length!=0",10));
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L".length!=0",10));
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=null",6));
		}
	}else{
		if((dynamic_cast<bb_type_IntType*>(t_dst))!=0){
			if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
				return m_Bra(t_texpr+String(L"?1:0",4));
			}
			if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
				return t_texpr;
			}
			if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
				return m_Bra(t_texpr+String(L"|0",2));
			}
			if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
				return String(L"parseInt",8)+m_Bra(t_texpr+String(L",10",3));
			}
		}else{
			if((dynamic_cast<bb_type_FloatType*>(t_dst))!=0){
				if((dynamic_cast<bb_type_NumericType*>(t_src))!=0){
					return t_texpr;
				}
				if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
					return String(L"parseFloat",10)+t_texpr;
				}
			}else{
				if((dynamic_cast<bb_type_StringType*>(t_dst))!=0){
					if((dynamic_cast<bb_type_NumericType*>(t_src))!=0){
						return String(L"String",6)+t_texpr;
					}
					if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
						return t_texpr;
					}
				}else{
					if(((dynamic_cast<bb_type_ObjectType*>(t_dst))!=0) && ((dynamic_cast<bb_type_ObjectType*>(t_src))!=0)){
						if((t_src->m_GetClass()->m_ExtendsClass(t_dst->m_GetClass()))!=0){
							return t_texpr;
						}else{
							return m_Bra(t_texpr+String(L" as ",4)+m_TransType(t_dst));
						}
					}
				}
			}
		}
	}
	bb_config_Err(String(L"AS translator can't convert ",28)+t_src->m_ToString()+String(L" to ",4)+t_dst->m_ToString());
	return String();
}
String bb_astranslator_AsTranslator::m_TransUnaryExpr(bb_expr_UnaryExpr* t_expr){
	int t_pri=m_ExprPri(t_expr);
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,t_pri);
	return m_TransUnaryOp(t_expr->f_op)+t_t_expr;
}
String bb_astranslator_AsTranslator::m_TransBinaryExpr(bb_expr_BinaryExpr* t_expr){
	int t_pri=m_ExprPri(t_expr);
	String t_t_lhs=m_TransSubExpr(t_expr->f_lhs,t_pri);
	String t_t_rhs=m_TransSubExpr(t_expr->f_rhs,t_pri-1);
	String t_t_expr=t_t_lhs+m_TransBinaryOp(t_expr->f_op,t_t_rhs)+t_t_rhs;
	if(t_expr->f_op==String(L"/",1) && ((dynamic_cast<bb_type_IntType*>(t_expr->f_exprType))!=0)){
		t_t_expr=m_Bra(m_Bra(t_t_expr)+String(L"|0",2));
	}
	return t_t_expr;
}
String bb_astranslator_AsTranslator::m_TransIndexExpr(bb_expr_IndexExpr* t_expr){
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,2);
	if((dynamic_cast<bb_type_StringType*>(t_expr->f_expr->f_exprType))!=0){
		String t_t_index=t_expr->f_index->m_Trans();
		return t_t_expr+String(L".charCodeAt(",12)+t_t_index+String(L")",1);
	}else{
		if(bb_config_ENV_CONFIG==String(L"debug",5)){
			String t_t_index2=t_expr->f_index->m_Trans();
			return String(L"dbg_array(",10)+t_t_expr+String(L",",1)+t_t_index2+String(L")[dbg_index]",12);
		}else{
			String t_t_index3=t_expr->f_index->m_Trans();
			return t_t_expr+String(L"[",1)+t_t_index3+String(L"]",1);
		}
	}
}
String bb_astranslator_AsTranslator::m_TransSliceExpr(bb_expr_SliceExpr* t_expr){
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,2);
	String t_t_args=String(L"0",1);
	if((t_expr->f_from)!=0){
		t_t_args=t_expr->f_from->m_Trans();
	}
	if((t_expr->f_term)!=0){
		t_t_args=t_t_args+(String(L",",1)+t_expr->f_term->m_Trans());
	}
	return t_t_expr+String(L".slice(",7)+t_t_args+String(L")",1);
}
String bb_astranslator_AsTranslator::m_TransArrayExpr(bb_expr_ArrayExpr* t_expr){
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_expr->f_exprs;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_elem=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_elem->m_Trans();
	}
	return String(L"[",1)+t_t+String(L"]",1);
}
String bb_astranslator_AsTranslator::m_TransIntrinsicExpr(bb_decl_Decl* t_decl,bb_expr_Expr* t_expr,Array<bb_expr_Expr* > t_args){
	String t_texpr=String();
	String t_arg0=String();
	String t_arg1=String();
	String t_arg2=String();
	if((t_expr)!=0){
		t_texpr=m_TransSubExpr(t_expr,2);
	}
	if(t_args.Length()>0 && ((t_args[0])!=0)){
		t_arg0=t_args[0]->m_Trans();
	}
	if(t_args.Length()>1 && ((t_args[1])!=0)){
		t_arg1=t_args[1]->m_Trans();
	}
	if(t_args.Length()>2 && ((t_args[2])!=0)){
		t_arg2=t_args[2]->m_Trans();
	}
	String t_id=t_decl->f_munged.Slice(1);
	String t_=t_id;
	if(t_==String(L"print",5)){
		return String(L"print",5)+m_Bra(t_arg0);
	}else{
		if(t_==String(L"error",5)){
			return String(L"error",5)+m_Bra(t_arg0);
		}else{
			if(t_==String(L"debuglog",8)){
				return String(L"debugLog",8)+m_Bra(t_arg0);
			}else{
				if(t_==String(L"debugstop",9)){
					return String(L"debugStop()",11);
				}else{
					if(t_==String(L"length",6)){
						return t_texpr+String(L".length",7);
					}else{
						if(t_==String(L"resize",6)){
							bb_type_Type* t_ty=dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType)->f_elemType;
							if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
								return String(L"resize_bool_array",17)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
								return String(L"resize_number_array",19)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
								return String(L"resize_string_array",19)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
								return String(L"resize_array_array",18)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
								return String(L"resize_object_array",19)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}
							bb_config_InternalErr(String(L"Internal error",14));
						}else{
							if(t_==String(L"compare",7)){
								return String(L"string_compare",14)+m_Bra(t_texpr+String(L",",1)+t_arg0);
							}else{
								if(t_==String(L"find",4)){
									return t_texpr+String(L".indexOf",8)+m_Bra(t_arg0+String(L",",1)+t_arg1);
								}else{
									if(t_==String(L"findlast",8)){
										return t_texpr+String(L".lastIndexOf",12)+m_Bra(t_arg0);
									}else{
										if(t_==String(L"findlast2",9)){
											return t_texpr+String(L".lastIndexOf",12)+m_Bra(t_arg0+String(L",",1)+t_arg1);
										}else{
											if(t_==String(L"trim",4)){
												return String(L"string_trim",11)+m_Bra(t_texpr);
											}else{
												if(t_==String(L"join",4)){
													return t_arg0+String(L".join",5)+m_Bra(t_texpr);
												}else{
													if(t_==String(L"split",5)){
														return t_texpr+String(L".split",6)+m_Bra(t_arg0);
													}else{
														if(t_==String(L"replace",7)){
															return String(L"string_replace",14)+m_Bra(t_texpr+String(L",",1)+t_arg0+String(L",",1)+t_arg1);
														}else{
															if(t_==String(L"tolower",7)){
																return t_texpr+String(L".toLowerCase()",14);
															}else{
																if(t_==String(L"toupper",7)){
																	return t_texpr+String(L".toUpperCase()",14);
																}else{
																	if(t_==String(L"contains",8)){
																		return m_Bra(t_texpr+String(L".indexOf",8)+m_Bra(t_arg0)+String(L"!=-1",4));
																	}else{
																		if(t_==String(L"startswith",10)){
																			return String(L"string_startswith",17)+m_Bra(t_texpr+String(L",",1)+t_arg0);
																		}else{
																			if(t_==String(L"endswith",8)){
																				return String(L"string_endswith",15)+m_Bra(t_texpr+String(L",",1)+t_arg0);
																			}else{
																				if(t_==String(L"tochars",7)){
																					return String(L"string_tochars",14)+m_Bra(t_texpr);
																				}else{
																					if(t_==String(L"fromchar",8)){
																						return String(L"String.fromCharCode",19)+m_Bra(t_arg0);
																					}else{
																						if(t_==String(L"fromchars",9)){
																							return String(L"string_fromchars",16)+m_Bra(t_arg0);
																						}else{
																							if(t_==String(L"sin",3) || t_==String(L"cos",3) || t_==String(L"tan",3)){
																								return String(L"Math.",5)+t_id+m_Bra(m_Bra(t_arg0)+String(L"*D2R",4));
																							}else{
																								if(t_==String(L"asin",4) || t_==String(L"acos",4) || t_==String(L"atan",4)){
																									return m_Bra(String(L"Math.",5)+t_id+m_Bra(t_arg0)+String(L"*R2D",4));
																								}else{
																									if(t_==String(L"atan2",5)){
																										return m_Bra(String(L"Math.",5)+t_id+m_Bra(t_arg0+String(L",",1)+t_arg1)+String(L"*R2D",4));
																									}else{
																										if(t_==String(L"sinr",4) || t_==String(L"cosr",4) || t_==String(L"tanr",4)){
																											return String(L"Math.",5)+t_id.Slice(0,-1)+m_Bra(t_arg0);
																										}else{
																											if(t_==String(L"asinr",5) || t_==String(L"acosr",5) || t_==String(L"atanr",5)){
																												return String(L"Math.",5)+t_id.Slice(0,-1)+m_Bra(t_arg0);
																											}else{
																												if(t_==String(L"atan2r",6)){
																													return String(L"Math.",5)+t_id.Slice(0,-1)+m_Bra(t_arg0+String(L",",1)+t_arg1);
																												}else{
																													if(t_==String(L"sqrt",4) || t_==String(L"floor",5) || t_==String(L"ceil",4) || t_==String(L"log",3) || t_==String(L"exp",3)){
																														return String(L"Math.",5)+t_id+m_Bra(t_arg0);
																													}else{
																														if(t_==String(L"pow",3)){
																															return String(L"Math.",5)+t_id+m_Bra(t_arg0+String(L",",1)+t_arg1);
																														}
																													}
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_astranslator_AsTranslator::m_TransTryStmt(bb_stmt_TryStmt* t_stmt){
	m_Emit(String(L"try{",4));
	int t_unr=m_EmitBlock(t_stmt->f_block,true);
	Array<bb_stmt_CatchStmt* > t_=t_stmt->f_catches;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_stmt_CatchStmt* t_c=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_c->f_init);
		m_Emit(String(L"}catch(",7)+t_c->f_init->f_munged+String(L":",1)+m_TransType(t_c->f_init->f_type)+String(L"){",2));
		int t_unr2=m_EmitBlock(t_c->f_block,true);
	}
	m_Emit(String(L"}",1));
	return String();
}
String bb_astranslator_AsTranslator::m_TransAssignStmt(bb_stmt_AssignStmt* t_stmt){
	if(bb_config_ENV_CONFIG==String(L"debug",5)){
		bb_expr_IndexExpr* t_ie=dynamic_cast<bb_expr_IndexExpr*>(t_stmt->f_lhs);
		if((t_ie)!=0){
			String t_t_rhs=t_stmt->f_rhs->m_Trans();
			String t_t_expr=t_ie->f_expr->m_Trans();
			String t_t_index=t_ie->f_index->m_Trans();
			m_Emit(String(L"dbg_array(",10)+t_t_expr+String(L",",1)+t_t_index+String(L")[dbg_index]",12)+m_TransAssignOp(t_stmt->f_op)+t_t_rhs);
			return String();
		}
	}
	return bb_translator_CTranslator::m_TransAssignStmt(t_stmt);
}
void bb_astranslator_AsTranslator::mark(){
	bb_translator_CTranslator::mark();
}
bb_javatranslator_JavaTranslator::bb_javatranslator_JavaTranslator(){
	f_unsafe=0;
}
bb_javatranslator_JavaTranslator* bb_javatranslator_JavaTranslator::g_new(){
	bb_translator_CTranslator::g_new();
	return this;
}
String bb_javatranslator_JavaTranslator::m_TransType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_VoidType*>(t_ty))!=0){
		return String(L"void",4);
	}
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"boolean",7);
	}
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		return String(L"int",3);
	}
	if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
		return String(L"float",5);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"String",6);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return m_TransType(dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType)+String(L"[]",2);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return t_ty->m_GetClass()->f_munged;
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_javatranslator_JavaTranslator::m_TransValue(bb_type_Type* t_ty,String t_value){
	if((t_value).Length()!=0){
		if(((dynamic_cast<bb_type_IntType*>(t_ty))!=0) && t_value.StartsWith(String(L"$",1))){
			return String(L"0x",2)+t_value.Slice(1);
		}
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"true",4);
		}
		if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
			return t_value;
		}
		if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
			return t_value+String(L"f",1);
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return m_Enquote(t_value);
		}
	}else{
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"false",5);
		}
		if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
			return String(L"0",1);
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return String(L"\"\"",2);
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
			bb_type_Type* t_elemTy=dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType;
			String t_t=String(L"[0]",3);
			while((dynamic_cast<bb_type_ArrayType*>(t_elemTy))!=0){
				t_elemTy=dynamic_cast<bb_type_ArrayType*>(t_elemTy)->f_elemType;
				t_t=t_t+String(L"[]",2);
			}
			return String(L"new ",4)+m_TransType(t_elemTy)+t_t;
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
			return String(L"null",4);
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_javatranslator_JavaTranslator::m_TransLocalDecl(String t_munged,bb_expr_Expr* t_init){
	return m_TransType(t_init->f_exprType)+String(L" ",1)+t_munged+String(L"=",1)+t_init->m_Trans();
}
int bb_javatranslator_JavaTranslator::m_EmitEnter(bb_decl_FuncDecl* t_func){
	if((f_unsafe)!=0){
		return 0;
	}
	m_Emit(String(L"bb_std_lang.pushErr();",22));
	return 0;
}
int bb_javatranslator_JavaTranslator::m_EmitSetErr(String t_info){
	if((f_unsafe)!=0){
		return 0;
	}
	m_Emit(String(L"bb_std_lang.errInfo=\"",21)+t_info.Replace(String(L"\\",1),String(L"/",1))+String(L"\";",2));
	return 0;
}
int bb_javatranslator_JavaTranslator::m_EmitLeave(){
	if((f_unsafe)!=0){
		return 0;
	}
	m_Emit(String(L"bb_std_lang.popErr();",21));
	return 0;
}
String bb_javatranslator_JavaTranslator::m_TransStatic(bb_decl_Decl* t_decl){
	if((t_decl->m_IsExtern())!=0){
		return t_decl->f_munged;
	}else{
		if(((bb_decl__env)!=0) && ((t_decl->f_scope)!=0) && t_decl->f_scope==(bb_decl__env->m_ClassScope())){
			return t_decl->f_munged;
		}else{
			if((dynamic_cast<bb_decl_ClassDecl*>(t_decl->f_scope))!=0){
				return t_decl->f_scope->f_munged+String(L".",1)+t_decl->f_munged;
			}else{
				if((dynamic_cast<bb_decl_ModuleDecl*>(t_decl->f_scope))!=0){
					return t_decl->f_scope->f_munged+String(L".",1)+t_decl->f_munged;
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_javatranslator_JavaTranslator::m_TransGlobal(bb_decl_GlobalDecl* t_decl){
	return m_TransStatic(t_decl);
}
int bb_javatranslator_JavaTranslator::m_EmitFuncDecl(bb_decl_FuncDecl* t_decl){
	f_unsafe=((t_decl->f_ident.EndsWith(String(L"__UNSAFE__",10)))?1:0);
	m_BeginLocalScope();
	String t_args=String();
	Array<bb_decl_ArgDecl* > t_=t_decl->f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_arg=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_arg);
		if((t_args).Length()!=0){
			t_args=t_args+String(L",",1);
		}
		t_args=t_args+(m_TransType(t_arg->f_type)+String(L" ",1)+t_arg->f_munged);
	}
	String t_t=String(L"public ",7)+m_TransType(t_decl->f_retType)+String(L" ",1)+t_decl->f_munged+m_Bra(t_args);
	if(((t_decl->m_ClassScope())!=0) && ((t_decl->m_ClassScope()->m_IsInterface())!=0)){
		m_Emit(t_t+String(L";",1));
	}else{
		if((t_decl->m_IsAbstract())!=0){
			m_Emit(String(L"abstract ",9)+t_t+String(L";",1));
		}else{
			String t_q=String();
			if(t_decl->m_IsStatic()){
				t_q=t_q+String(L"static ",7);
			}
			m_Emit(t_q+t_t+String(L"{",1));
			m_EmitBlock((t_decl),true);
			m_Emit(String(L"}",1));
		}
	}
	m_EndLocalScope();
	f_unsafe=0;
	return 0;
}
String bb_javatranslator_JavaTranslator::m_TransDecl(bb_decl_Decl* t_decl){
	String t_id=t_decl->f_munged;
	bb_decl_ValDecl* t_vdecl=dynamic_cast<bb_decl_ValDecl*>(t_decl);
	if((t_vdecl)!=0){
		return m_TransType(t_vdecl->f_type)+String(L" ",1)+t_id;
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
int bb_javatranslator_JavaTranslator::m_EmitClassDecl(bb_decl_ClassDecl* t_classDecl){
	String t_classid=t_classDecl->f_munged;
	String t_superid=t_classDecl->f_superClass->f_munged;
	if((t_classDecl->m_IsInterface())!=0){
		String t_bases=String();
		Array<bb_decl_ClassDecl* > t_=t_classDecl->f_implments;
		int t_2=0;
		while(t_2<t_.Length()){
			bb_decl_ClassDecl* t_iface=t_[t_2];
			t_2=t_2+1;
			if((t_bases).Length()!=0){
				t_bases=t_bases+String(L",",1);
			}else{
				t_bases=String(L" extends ",9);
			}
			t_bases=t_bases+t_iface->f_munged;
		}
		m_Emit(String(L"interface ",10)+t_classid+t_bases+String(L"{",1));
		bb_list_Enumerator2* t_3=t_classDecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl=t_3->m_NextObject();
			bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
			if(!((t_fdecl)!=0)){
				continue;
			}
			m_EmitFuncDecl(t_fdecl);
		}
		m_Emit(String(L"}",1));
		return 0;
	}
	String t_bases2=String();
	Array<bb_decl_ClassDecl* > t_4=t_classDecl->f_implments;
	int t_5=0;
	while(t_5<t_4.Length()){
		bb_decl_ClassDecl* t_iface2=t_4[t_5];
		t_5=t_5+1;
		if((t_bases2).Length()!=0){
			t_bases2=t_bases2+String(L",",1);
		}else{
			t_bases2=String(L" implements ",12);
		}
		t_bases2=t_bases2+t_iface2->f_munged;
	}
	String t_q=String();
	if((t_classDecl->m_IsAbstract())!=0){
		t_q=String(L"abstract ",9);
	}
	m_Emit(t_q+String(L"class ",6)+t_classid+String(L" extends ",9)+t_superid+t_bases2+String(L"{",1));
	bb_list_Enumerator2* t_6=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_6->m_HasNext()){
		bb_decl_Decl* t_decl2=t_6->m_NextObject();
		bb_decl_FieldDecl* t_tdecl=dynamic_cast<bb_decl_FieldDecl*>(t_decl2);
		if((t_tdecl)!=0){
			m_Emit(m_TransDecl(t_tdecl)+String(L"=",1)+t_tdecl->f_init->m_Trans()+String(L";",1));
			continue;
		}
		bb_decl_FuncDecl* t_fdecl2=dynamic_cast<bb_decl_FuncDecl*>(t_decl2);
		if((t_fdecl2)!=0){
			m_EmitFuncDecl(t_fdecl2);
			continue;
		}
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl2);
		if((t_gdecl)!=0){
			m_Emit(String(L"static ",7)+m_TransDecl(t_gdecl)+String(L";",1));
			continue;
		}
	}
	m_Emit(String(L"}",1));
	return 0;
}
String bb_javatranslator_JavaTranslator::m_TransApp(bb_decl_AppDecl* t_app){
	t_app->f_mainModule->f_munged=String(L"bb_",3);
	t_app->f_mainFunc->f_munged=String(L"bbMain",6);
	bb_map_ValueEnumerator* t_=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_ModuleDecl* t_decl=t_->m_NextObject();
		m_MungDecl(t_decl);
	}
	bb_list_Enumerator2* t_2=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_Decl* t_decl2=t_2->m_NextObject();
		m_MungDecl(t_decl2);
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl2);
		if(!((t_cdecl)!=0)){
			continue;
		}
		bb_list_Enumerator2* t_3=t_cdecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl3=t_3->m_NextObject();
			m_MungDecl(t_decl3);
		}
	}
	bb_list_Enumerator2* t_4=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_4->m_HasNext()){
		bb_decl_Decl* t_decl4=t_4->m_NextObject();
		bb_decl_ClassDecl* t_cdecl2=dynamic_cast<bb_decl_ClassDecl*>(t_decl4);
		if((t_cdecl2)!=0){
			m_EmitClassDecl(t_cdecl2);
		}
	}
	bb_map_ValueEnumerator* t_5=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_5->m_HasNext()){
		bb_decl_ModuleDecl* t_mdecl=t_5->m_NextObject();
		m_Emit(String(L"class ",6)+t_mdecl->f_munged+String(L"{",1));
		bb_list_Enumerator2* t_6=t_mdecl->m_Semanted()->m_ObjectEnumerator();
		while(t_6->m_HasNext()){
			bb_decl_Decl* t_decl5=t_6->m_NextObject();
			if(((t_decl5->m_IsExtern())!=0) || ((t_decl5->f_scope->m_ClassScope())!=0)){
				continue;
			}
			bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl5);
			if((t_gdecl)!=0){
				m_Emit(String(L"static ",7)+m_TransDecl(t_gdecl)+String(L";",1));
				continue;
			}
			bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl5);
			if((t_fdecl)!=0){
				m_EmitFuncDecl(t_fdecl);
				continue;
			}
		}
		if(t_mdecl==t_app->f_mainModule){
			m_BeginLocalScope();
			m_Emit(String(L"public static int bbInit(){",27));
			bb_list_Enumerator6* t_7=t_app->f_semantedGlobals->m_ObjectEnumerator();
			while(t_7->m_HasNext()){
				bb_decl_GlobalDecl* t_decl6=t_7->m_NextObject();
				m_Emit(m_TransGlobal(t_decl6)+String(L"=",1)+t_decl6->f_init->m_Trans()+String(L";",1));
			}
			m_Emit(String(L"return 0;",9));
			m_Emit(String(L"}",1));
			m_EndLocalScope();
		}
		m_Emit(String(L"}",1));
	}
	return m_JoinLines();
}
String bb_javatranslator_JavaTranslator::m_TransField(bb_decl_FieldDecl* t_decl,bb_expr_Expr* t_lhs){
	if((t_lhs)!=0){
		return m_TransSubExpr(t_lhs,2)+String(L".",1)+t_decl->f_munged;
	}
	return t_decl->f_munged;
}
String bb_javatranslator_JavaTranslator::m_TransArgs2(Array<bb_expr_Expr* > t_args){
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_arg=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_arg->m_Trans();
	}
	return m_Bra(t_t);
}
String bb_javatranslator_JavaTranslator::m_TransFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args,bb_expr_Expr* t_lhs){
	if(t_decl->m_IsMethod()){
		if((t_lhs)!=0){
			return m_TransSubExpr(t_lhs,2)+String(L".",1)+t_decl->f_munged+m_TransArgs2(t_args);
		}
		return t_decl->f_munged+m_TransArgs2(t_args);
	}
	return m_TransStatic(t_decl)+m_TransArgs2(t_args);
}
String bb_javatranslator_JavaTranslator::m_TransSuperFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args){
	return String(L"super.",6)+t_decl->f_munged+m_TransArgs2(t_args);
}
String bb_javatranslator_JavaTranslator::m_TransConstExpr(bb_expr_ConstExpr* t_expr){
	return m_TransValue(t_expr->f_exprType,t_expr->f_value);
}
String bb_javatranslator_JavaTranslator::m_TransNewObjectExpr(bb_expr_NewObjectExpr* t_expr){
	String t_t=String(L"(new ",5)+t_expr->f_classDecl->f_munged+String(L"())",3);
	if((t_expr->f_ctor)!=0){
		t_t=t_t+(String(L".",1)+t_expr->f_ctor->f_munged+m_TransArgs2(t_expr->f_args));
	}
	return t_t;
}
String bb_javatranslator_JavaTranslator::m_TransNewArrayExpr(bb_expr_NewArrayExpr* t_expr){
	String t_texpr=t_expr->f_expr->m_Trans();
	bb_type_Type* t_elemTy=dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType)->f_elemType;
	if((dynamic_cast<bb_type_StringType*>(t_elemTy))!=0){
		return String(L"bb_std_lang.stringArray",23)+m_Bra(t_texpr);
	}
	String t_t=String(L"[",1)+t_texpr+String(L"]",1);
	while((dynamic_cast<bb_type_ArrayType*>(t_elemTy))!=0){
		t_elemTy=dynamic_cast<bb_type_ArrayType*>(t_elemTy)->f_elemType;
		t_t=t_t+String(L"[]",2);
	}
	return String(L"new ",4)+m_TransType(t_elemTy)+t_t;
}
String bb_javatranslator_JavaTranslator::m_TransSelfExpr(bb_expr_SelfExpr* t_expr){
	return String(L"this",4);
}
String bb_javatranslator_JavaTranslator::m_TransCastExpr(bb_expr_CastExpr* t_expr){
	String t_texpr=m_Bra(t_expr->f_expr->m_Trans());
	bb_type_Type* t_dst=t_expr->f_exprType;
	bb_type_Type* t_src=t_expr->f_expr->f_exprType;
	if((dynamic_cast<bb_type_BoolType*>(t_dst))!=0){
		if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
			return t_texpr;
		}
		if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=0",3));
		}
		if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=0.0f",6));
		}
		if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L".length()!=0",12));
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L".length!=0",10));
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=null",6));
		}
	}else{
		if((dynamic_cast<bb_type_IntType*>(t_dst))!=0){
			if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
				return m_Bra(t_texpr+String(L"?1:0",4));
			}
			if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
				return t_texpr;
			}
			if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
				return String(L"(int)",5)+t_texpr;
			}
			if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
				return String(L"Integer.parseInt(",17)+t_texpr+String(L".trim())",8);
			}
		}else{
			if((dynamic_cast<bb_type_FloatType*>(t_dst))!=0){
				if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
					return String(L"(float)",7)+t_texpr;
				}
				if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
					return t_texpr;
				}
				if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
					return String(L"Float.parseFloat(",17)+t_texpr+String(L".trim())",8);
				}
			}else{
				if((dynamic_cast<bb_type_StringType*>(t_dst))!=0){
					if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
						return String(L"String.valueOf",14)+t_texpr;
					}
					if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
						return String(L"String.valueOf",14)+t_texpr;
					}
					if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
						return t_texpr;
					}
				}else{
					if(((dynamic_cast<bb_type_ObjectType*>(t_dst))!=0) && ((dynamic_cast<bb_type_ObjectType*>(t_src))!=0)){
						if((t_src->m_GetClass()->m_ExtendsClass(t_dst->m_GetClass()))!=0){
							return t_texpr;
						}else{
							bb_decl_LocalDecl* t_tmp=(new bb_decl_LocalDecl)->g_new(String(),0,t_src,0);
							m_MungDecl(t_tmp);
							m_Emit(m_TransType(t_src)+String(L" ",1)+t_tmp->f_munged+String(L"=",1)+t_expr->f_expr->m_Trans()+String(L";",1));
							return String(L"($t instanceof $c ? ($c)$t : null)",34).Replace(String(L"$t",2),t_tmp->f_munged).Replace(String(L"$c",2),m_TransType(t_dst));
						}
					}
				}
			}
		}
	}
	bb_config_Err(String(L"Java translator can't convert ",30)+t_src->m_ToString()+String(L" to ",4)+t_dst->m_ToString());
	return String();
}
String bb_javatranslator_JavaTranslator::m_TransUnaryExpr(bb_expr_UnaryExpr* t_expr){
	String t_texpr=t_expr->f_expr->m_Trans();
	if(m_ExprPri(t_expr->f_expr)>m_ExprPri(t_expr)){
		t_texpr=m_Bra(t_texpr);
	}
	return m_TransUnaryOp(t_expr->f_op)+t_texpr;
}
String bb_javatranslator_JavaTranslator::m_TransBinaryExpr(bb_expr_BinaryExpr* t_expr){
	String t_lhs=t_expr->f_lhs->m_Trans();
	String t_rhs=t_expr->f_rhs->m_Trans();
	if(((dynamic_cast<bb_expr_BinaryCompareExpr*>(t_expr))!=0) && ((dynamic_cast<bb_type_StringType*>(t_expr->f_lhs->f_exprType))!=0) && ((dynamic_cast<bb_type_StringType*>(t_expr->f_rhs->f_exprType))!=0)){
		return m_Bra(t_lhs+String(L".compareTo",10)+m_Bra(t_rhs)+m_TransBinaryOp(t_expr->f_op,String())+String(L"0",1));
	}
	int t_pri=m_ExprPri(t_expr);
	if(m_ExprPri(t_expr->f_lhs)>t_pri){
		t_lhs=m_Bra(t_lhs);
	}
	if(m_ExprPri(t_expr->f_rhs)>=t_pri){
		t_rhs=m_Bra(t_rhs);
	}
	return t_lhs+m_TransBinaryOp(t_expr->f_op,t_rhs)+t_rhs;
}
String bb_javatranslator_JavaTranslator::m_TransIndexExpr(bb_expr_IndexExpr* t_expr){
	String t_texpr=t_expr->f_expr->m_Trans();
	String t_index=t_expr->f_index->m_Trans();
	if((dynamic_cast<bb_type_StringType*>(t_expr->f_expr->f_exprType))!=0){
		return String(L"(int)",5)+t_texpr+String(L".charAt(",8)+t_index+String(L")",1);
	}
	return t_texpr+String(L"[",1)+t_index+String(L"]",1);
}
String bb_javatranslator_JavaTranslator::m_TransSliceExpr(bb_expr_SliceExpr* t_expr){
	String t_texpr=t_expr->f_expr->m_Trans();
	String t_from=String(L",0",2);
	String t_term=String();
	if((t_expr->f_from)!=0){
		t_from=String(L",",1)+t_expr->f_from->m_Trans();
	}
	if((t_expr->f_term)!=0){
		t_term=String(L",",1)+t_expr->f_term->m_Trans();
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType))!=0){
		return String(L"((",2)+m_TransType(t_expr->f_exprType)+String(L")bb_std_lang.sliceArray",23)+m_Bra(t_texpr+t_from+t_term)+String(L")",1);
	}else{
		if((dynamic_cast<bb_type_StringType*>(t_expr->f_exprType))!=0){
			return String(L"bb_std_lang.slice(",18)+t_texpr+t_from+t_term+String(L")",1);
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_javatranslator_JavaTranslator::m_TransArrayExpr(bb_expr_ArrayExpr* t_expr){
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_expr->f_exprs;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_elem=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_elem->m_Trans();
	}
	return String(L"new ",4)+m_TransType(t_expr->f_exprType)+String(L"{",1)+t_t+String(L"}",1);
}
String bb_javatranslator_JavaTranslator::m_TransIntrinsicExpr(bb_decl_Decl* t_decl,bb_expr_Expr* t_expr,Array<bb_expr_Expr* > t_args){
	String t_texpr=String();
	String t_arg0=String();
	String t_arg1=String();
	String t_arg2=String();
	if((t_expr)!=0){
		t_texpr=m_TransSubExpr(t_expr,2);
	}
	if(t_args.Length()>0 && ((t_args[0])!=0)){
		t_arg0=t_args[0]->m_Trans();
	}
	if(t_args.Length()>1 && ((t_args[1])!=0)){
		t_arg1=t_args[1]->m_Trans();
	}
	if(t_args.Length()>2 && ((t_args[2])!=0)){
		t_arg2=t_args[2]->m_Trans();
	}
	String t_id=t_decl->f_munged.Slice(1);
	String t_=t_id;
	if(t_==String(L"print",5)){
		return String(L"bb_std_lang.print",17)+m_Bra(t_arg0);
	}else{
		if(t_==String(L"error",5)){
			return String(L"bb_std_lang.error",17)+m_Bra(t_arg0);
		}else{
			if(t_==String(L"debuglog",8)){
				return String(L"bb_std_lang.debugLog",20)+m_Bra(t_arg0);
			}else{
				if(t_==String(L"debugstop",9)){
					return String(L"bb_std_lang.debugStop()",23);
				}else{
					if(t_==String(L"length",6)){
						if((dynamic_cast<bb_type_StringType*>(t_expr->f_exprType))!=0){
							return t_texpr+String(L".length()",9);
						}
						return String(L"bb_std_lang.arrayLength",23)+m_Bra(t_texpr);
					}else{
						if(t_==String(L"resize",6)){
							String t_fn=String(L"resizeArray",11);
							bb_type_Type* t_ty=dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType)->f_elemType;
							if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
								t_fn=String(L"resizeStringArray",17);
							}else{
								if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
									t_fn=String(L"resizeArrayArray",16);
								}
							}
							return String(L"(",1)+m_TransType(t_expr->f_exprType)+String(L")bb_std_lang.",13)+t_fn+m_Bra(t_texpr+String(L",",1)+t_arg0);
						}else{
							if(t_==String(L"compare",7)){
								return t_texpr+String(L".compareTo",10)+m_Bra(t_arg0);
							}else{
								if(t_==String(L"find",4)){
									return t_texpr+String(L".indexOf",8)+m_Bra(t_arg0+String(L",",1)+t_arg1);
								}else{
									if(t_==String(L"findlast",8)){
										return t_texpr+String(L".lastIndexOf",12)+m_Bra(t_arg0);
									}else{
										if(t_==String(L"findlast2",9)){
											return t_texpr+String(L".lastIndexOf",12)+m_Bra(t_arg0+String(L",",1)+t_arg1);
										}else{
											if(t_==String(L"trim",4)){
												return t_texpr+String(L".trim()",7);
											}else{
												if(t_==String(L"join",4)){
													return String(L"bb_std_lang.join",16)+m_Bra(t_texpr+String(L",",1)+t_arg0);
												}else{
													if(t_==String(L"split",5)){
														return String(L"bb_std_lang.split",17)+m_Bra(t_texpr+String(L",",1)+t_arg0);
													}else{
														if(t_==String(L"replace",7)){
															return String(L"bb_std_lang.replace",19)+m_Bra(t_texpr+String(L",",1)+t_arg0+String(L",",1)+t_arg1);
														}else{
															if(t_==String(L"tolower",7)){
																return t_texpr+String(L".toLowerCase()",14);
															}else{
																if(t_==String(L"toupper",7)){
																	return t_texpr+String(L".toUpperCase()",14);
																}else{
																	if(t_==String(L"contains",8)){
																		return m_Bra(t_texpr+String(L".indexOf",8)+m_Bra(t_arg0)+String(L"!=-1",4));
																	}else{
																		if(t_==String(L"startswith",10)){
																			return t_texpr+String(L".startsWith",11)+m_Bra(t_arg0);
																		}else{
																			if(t_==String(L"endswith",8)){
																				return t_texpr+String(L".endsWith",9)+m_Bra(t_arg0);
																			}else{
																				if(t_==String(L"tochars",7)){
																					return String(L"bb_std_lang.toChars",19)+m_Bra(t_texpr);
																				}else{
																					if(t_==String(L"fromchar",8)){
																						return String(L"String.valueOf",14)+m_Bra(String(L"(char)",6)+m_Bra(t_arg0));
																					}else{
																						if(t_==String(L"fromchars",9)){
																							return String(L"bb_std_lang.fromChars",21)+m_Bra(t_arg0);
																						}else{
																							if(t_==String(L"sin",3) || t_==String(L"cos",3) || t_==String(L"tan",3)){
																								return String(L"(float)Math.",12)+t_id+m_Bra(m_Bra(t_arg0)+String(L"*bb_std_lang.D2R",16));
																							}else{
																								if(t_==String(L"asin",4) || t_==String(L"acos",4) || t_==String(L"atan",4)){
																									return String(L"(float)",7)+m_Bra(String(L"Math.",5)+t_id+m_Bra(t_arg0)+String(L"*bb_std_lang.R2D",16));
																								}else{
																									if(t_==String(L"atan2",5)){
																										return String(L"(float)",7)+m_Bra(String(L"Math.",5)+t_id+m_Bra(t_arg0+String(L",",1)+t_arg1)+String(L"*bb_std_lang.R2D",16));
																									}else{
																										if(t_==String(L"sinr",4) || t_==String(L"cosr",4) || t_==String(L"tanr",4)){
																											return String(L"(float)Math.",12)+t_id.Slice(0,-1)+m_Bra(t_arg0);
																										}else{
																											if(t_==String(L"asinr",5) || t_==String(L"acosr",5) || t_==String(L"atanr",5)){
																												return String(L"(float)Math.",12)+t_id.Slice(0,-1)+m_Bra(t_arg0);
																											}else{
																												if(t_==String(L"atan2r",6)){
																													return String(L"(float)Math.",12)+t_id.Slice(0,-1)+m_Bra(t_arg0+String(L",",1)+t_arg1);
																												}else{
																													if(t_==String(L"sqrt",4) || t_==String(L"floor",5) || t_==String(L"ceil",4) || t_==String(L"log",3) || t_==String(L"exp",3)){
																														return String(L"(float)Math.",12)+t_id+m_Bra(t_arg0);
																													}else{
																														if(t_==String(L"pow",3)){
																															return String(L"(float)Math.",12)+t_id+m_Bra(t_arg0+String(L",",1)+t_arg1);
																														}
																													}
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_javatranslator_JavaTranslator::m_TransTryStmt(bb_stmt_TryStmt* t_stmt){
	m_Emit(String(L"try{",4));
	int t_unr=m_EmitBlock(t_stmt->f_block,true);
	Array<bb_stmt_CatchStmt* > t_=t_stmt->f_catches;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_stmt_CatchStmt* t_c=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_c->f_init);
		m_Emit(String(L"}catch(",7)+m_TransType(t_c->f_init->f_type)+String(L" ",1)+t_c->f_init->f_munged+String(L"){",2));
		int t_unr2=m_EmitBlock(t_c->f_block,true);
	}
	m_Emit(String(L"}",1));
	return String();
}
void bb_javatranslator_JavaTranslator::mark(){
	bb_translator_CTranslator::mark();
}
bb_cpptranslator_CppTranslator::bb_cpptranslator_CppTranslator(){
	f_dbgLocals=(new bb_stack_Stack8)->g_new();
	f_lastDbgInfo=String();
	f_unsafe=0;
}
bb_cpptranslator_CppTranslator* bb_cpptranslator_CppTranslator::g_new(){
	bb_translator_CTranslator::g_new();
	return this;
}
String bb_cpptranslator_CppTranslator::m_TransType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_VoidType*>(t_ty))!=0){
		return String(L"void",4);
	}
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"bool",4);
	}
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		return String(L"int",3);
	}
	if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
		return String(L"Float",5);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"String",6);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return String(L"Array<",6)+m_TransRefType(dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType)+String(L" >",2);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return t_ty->m_GetClass()->f_munged+String(L"*",1);
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cpptranslator_CppTranslator::m_TransRefType(bb_type_Type* t_ty){
	return m_TransType(t_ty);
}
String bb_cpptranslator_CppTranslator::m_TransValue(bb_type_Type* t_ty,String t_value){
	if((t_value).Length()!=0){
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"true",4);
		}
		if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
			return t_value;
		}
		if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
			return String(L"FLOAT(",6)+t_value+String(L")",1);
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return String(L"String(",7)+m_Enquote(t_value)+String(L",",1)+String(t_value.Length())+String(L")",1);
		}
	}else{
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"false",5);
		}
		if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
			return String(L"0",1);
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return String(L"String()",8);
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
			return String(L"Array<",6)+m_TransRefType(dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType)+String(L" >()",4);
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
			return String(L"0",1);
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cpptranslator_CppTranslator::m_TransLocalDecl(String t_munged,bb_expr_Expr* t_init){
	return m_TransType(t_init->f_exprType)+String(L" ",1)+t_munged+String(L"=",1)+t_init->m_Trans();
}
int bb_cpptranslator_CppTranslator::m_BeginLocalScope(){
	bb_translator_CTranslator::m_BeginLocalScope();
	return 0;
}
int bb_cpptranslator_CppTranslator::m_EndLocalScope(){
	bb_translator_CTranslator::m_EndLocalScope();
	f_dbgLocals->m_Clear();
	f_lastDbgInfo=String();
	return 0;
}
int bb_cpptranslator_CppTranslator::m_EmitEnter(bb_decl_FuncDecl* t_func){
	if((f_unsafe)!=0){
		return 0;
	}
	String t_id=t_func->f_ident;
	if((dynamic_cast<bb_decl_ClassDecl*>(t_func->f_scope))!=0){
		t_id=t_func->f_scope->f_ident+String(L".",1)+t_id;
	}
	m_Emit(String(L"DBG_ENTER(\"",11)+t_id+String(L"\")",2));
	if(t_func->m_IsCtor() || t_func->m_IsMethod()){
		m_Emit(t_func->f_scope->f_munged+String(L" *self=this;",12));
		m_Emit(String(L"DBG_LOCAL(self,\"Self\")",22));
	}
	return 0;
}
int bb_cpptranslator_CppTranslator::m_EmitEnterBlock(){
	if((f_unsafe)!=0){
		return 0;
	}
	m_Emit(String(L"DBG_BLOCK();",12));
	return 0;
}
int bb_cpptranslator_CppTranslator::m_EmitSetErr(String t_info){
	if((f_unsafe)!=0){
		return 0;
	}
	if(t_info==f_lastDbgInfo){
		return 0;
	}
	f_lastDbgInfo=t_info;
	bb_stack_Enumerator* t_=f_dbgLocals->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_LocalDecl* t_decl=t_->m_NextObject();
		if((t_decl->f_ident).Length()!=0){
			m_Emit(String(L"DBG_LOCAL(",10)+t_decl->f_munged+String(L",\"",2)+t_decl->f_ident+String(L"\")",2));
		}
	}
	f_dbgLocals->m_Clear();
	m_Emit(String(L"DBG_INFO(\"",10)+t_info.Replace(String(L"\\",1),String(L"/",1))+String(L"\");",3));
	return 0;
}
int bb_cpptranslator_CppTranslator::m_EmitLeaveBlock(){
	f_dbgLocals->m_Clear();
	return 0;
}
String bb_cpptranslator_CppTranslator::m_TransStatic(bb_decl_Decl* t_decl){
	if((t_decl->m_IsExtern())!=0){
		return t_decl->f_munged;
	}else{
		if((dynamic_cast<bb_decl_ClassDecl*>(t_decl->f_scope))!=0){
			return t_decl->f_scope->f_munged+String(L"::",2)+t_decl->f_munged;
		}else{
			if((dynamic_cast<bb_decl_ModuleDecl*>(t_decl->f_scope))!=0){
				return t_decl->f_munged;
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cpptranslator_CppTranslator::m_TransGlobal(bb_decl_GlobalDecl* t_decl){
	return m_TransStatic(t_decl);
}
int bb_cpptranslator_CppTranslator::m_EmitFuncProto(bb_decl_FuncDecl* t_decl){
	String t_args=String();
	Array<bb_decl_ArgDecl* > t_=t_decl->f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_arg=t_[t_2];
		t_2=t_2+1;
		if((t_args).Length()!=0){
			t_args=t_args+String(L",",1);
		}
		t_args=t_args+m_TransType(t_arg->f_type);
	}
	String t_t=m_TransType(t_decl->f_retType)+String(L" ",1)+t_decl->f_munged+m_Bra(t_args);
	if((t_decl->m_IsAbstract())!=0){
		t_t=t_t+String(L"=0",2);
	}
	String t_q=String();
	if(t_decl->m_IsMethod()){
		t_q=t_q+String(L"virtual ",8);
	}
	if(t_decl->m_IsStatic() && ((t_decl->m_ClassScope())!=0)){
		t_q=t_q+String(L"static ",7);
	}
	m_Emit(t_q+t_t+String(L";",1));
	return 0;
}
int bb_cpptranslator_CppTranslator::m_EmitClassProto(bb_decl_ClassDecl* t_classDecl){
	String t_classid=t_classDecl->f_munged;
	String t_superid=t_classDecl->f_superClass->f_munged;
	if((t_classDecl->m_IsInterface())!=0){
		String t_bases=String();
		Array<bb_decl_ClassDecl* > t_=t_classDecl->f_implments;
		int t_2=0;
		while(t_2<t_.Length()){
			bb_decl_ClassDecl* t_iface=t_[t_2];
			t_2=t_2+1;
			if((t_bases).Length()!=0){
				t_bases=t_bases+String(L",",1);
			}else{
				t_bases=String(L" : ",3);
			}
			t_bases=t_bases+(String(L"public virtual ",15)+t_iface->f_munged);
		}
		if(!((t_bases).Length()!=0)){
			t_bases=String(L" : public virtual gc_interface",30);
		}
		m_Emit(String(L"class ",6)+t_classid+t_bases+String(L"{",1));
		m_Emit(String(L"public:",7));
		bb_stack_Stack9* t_emitted=(new bb_stack_Stack9)->g_new();
		bb_list_Enumerator2* t_3=t_classDecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl=t_3->m_NextObject();
			bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
			if(!((t_fdecl)!=0)){
				continue;
			}
			m_EmitFuncProto(t_fdecl);
			t_emitted->m_Push25(t_fdecl);
		}
		Array<bb_decl_ClassDecl* > t_4=t_classDecl->f_implmentsAll;
		int t_5=0;
		while(t_5<t_4.Length()){
			bb_decl_ClassDecl* t_iface2=t_4[t_5];
			t_5=t_5+1;
			bb_list_Enumerator2* t_6=t_iface2->m_Semanted()->m_ObjectEnumerator();
			while(t_6->m_HasNext()){
				bb_decl_Decl* t_decl2=t_6->m_NextObject();
				bb_decl_FuncDecl* t_fdecl2=dynamic_cast<bb_decl_FuncDecl*>(t_decl2);
				if(!((t_fdecl2)!=0)){
					continue;
				}
				int t_found=0;
				bb_stack_Enumerator2* t_7=t_emitted->m_ObjectEnumerator();
				while(t_7->m_HasNext()){
					bb_decl_FuncDecl* t_fdecl22=t_7->m_NextObject();
					if(t_fdecl2->f_ident!=t_fdecl22->f_ident){
						continue;
					}
					if(!t_fdecl2->m_EqualsFunc(t_fdecl22)){
						continue;
					}
					t_found=1;
					break;
				}
				if((t_found)!=0){
					continue;
				}
				m_EmitFuncProto(t_fdecl2);
				t_emitted->m_Push25(t_fdecl2);
			}
		}
		m_Emit(String(L"};",2));
		return 0;
	}
	String t_bases2=String(L" : public ",10)+t_superid;
	Array<bb_decl_ClassDecl* > t_8=t_classDecl->f_implments;
	int t_9=0;
	while(t_9<t_8.Length()){
		bb_decl_ClassDecl* t_iface3=t_8[t_9];
		t_9=t_9+1;
		t_bases2=t_bases2+(String(L",public virtual ",16)+t_iface3->f_munged);
	}
	m_Emit(String(L"class ",6)+t_classid+t_bases2+String(L"{",1));
	m_Emit(String(L"public:",7));
	bb_list_Enumerator2* t_10=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_10->m_HasNext()){
		bb_decl_Decl* t_decl3=t_10->m_NextObject();
		bb_decl_FieldDecl* t_fdecl3=dynamic_cast<bb_decl_FieldDecl*>(t_decl3);
		if((t_fdecl3)!=0){
			m_Emit(m_TransRefType(t_fdecl3->f_type)+String(L" ",1)+t_fdecl3->f_munged+String(L";",1));
			continue;
		}
	}
	m_Emit(t_classid+String(L"();",3));
	bb_list_Enumerator2* t_11=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_11->m_HasNext()){
		bb_decl_Decl* t_decl4=t_11->m_NextObject();
		bb_decl_FuncDecl* t_fdecl4=dynamic_cast<bb_decl_FuncDecl*>(t_decl4);
		if((t_fdecl4)!=0){
			m_EmitFuncProto(t_fdecl4);
			continue;
		}
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl4);
		if((t_gdecl)!=0){
			m_Emit(String(L"static ",7)+m_TransRefType(t_gdecl->f_type)+String(L" ",1)+t_gdecl->f_munged+String(L";",1));
			continue;
		}
	}
	m_Emit(String(L"void mark();",12));
	if(bb_config_ENV_CONFIG==String(L"debug",5)){
		m_Emit(String(L"String debug();",15));
	}
	m_Emit(String(L"};",2));
	if(bb_config_ENV_CONFIG==String(L"debug",5)){
		m_Emit(String(L"String dbg_type(",16)+t_classid+String(L"**p){return \"",13)+t_classDecl->f_ident+String(L"\";}",3));
	}
	return 0;
}
int bb_cpptranslator_CppTranslator::m_EmitFuncDecl(bb_decl_FuncDecl* t_decl){
	if((t_decl->m_IsAbstract())!=0){
		return 0;
	}
	f_unsafe=((t_decl->f_ident.EndsWith(String(L"__UNSAFE__",10)))?1:0);
	m_BeginLocalScope();
	String t_args=String();
	Array<bb_decl_ArgDecl* > t_=t_decl->f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_arg=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_arg);
		if((t_args).Length()!=0){
			t_args=t_args+String(L",",1);
		}
		t_args=t_args+(m_TransType(t_arg->f_type)+String(L" ",1)+t_arg->f_munged);
		f_dbgLocals->m_Push22(t_arg);
	}
	String t_id=t_decl->f_munged;
	if((t_decl->m_ClassScope())!=0){
		t_id=t_decl->m_ClassScope()->f_munged+String(L"::",2)+t_id;
	}
	m_Emit(m_TransType(t_decl->f_retType)+String(L" ",1)+t_id+m_Bra(t_args)+String(L"{",1));
	m_EmitBlock((t_decl),true);
	m_Emit(String(L"}",1));
	m_EndLocalScope();
	f_unsafe=0;
	return 0;
}
String bb_cpptranslator_CppTranslator::m_TransField(bb_decl_FieldDecl* t_decl,bb_expr_Expr* t_lhs){
	if((t_lhs)!=0){
		return m_TransSubExpr(t_lhs,2)+String(L"->",2)+t_decl->f_munged;
	}
	return t_decl->f_munged;
}
int bb_cpptranslator_CppTranslator::m_EmitMark(String t_id,bb_type_Type* t_ty,bool t_queue){
	if(((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0) || ((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0)){
		if(((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0) && !((t_ty->m_GetClass()->m_ExtendsObject())!=0)){
			return 0;
		}
		if(t_queue){
			m_Emit(String(L"gc_mark_q(",10)+t_id+String(L");",2));
		}else{
			m_Emit(String(L"gc_mark(",8)+t_id+String(L");",2));
		}
	}
	return 0;
}
int bb_cpptranslator_CppTranslator::m_EmitClassDecl(bb_decl_ClassDecl* t_classDecl){
	if((t_classDecl->m_IsInterface())!=0){
		return 0;
	}
	String t_classid=t_classDecl->f_munged;
	String t_superid=t_classDecl->f_superClass->f_munged;
	m_BeginLocalScope();
	m_Emit(t_classid+String(L"::",2)+t_classid+String(L"(){",3));
	bb_list_Enumerator2* t_=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_Decl* t_decl=t_->m_NextObject();
		bb_decl_FieldDecl* t_fdecl=dynamic_cast<bb_decl_FieldDecl*>(t_decl);
		if(!((t_fdecl)!=0)){
			continue;
		}
		m_Emit(m_TransField(t_fdecl,0)+String(L"=",1)+t_fdecl->f_init->m_Trans()+String(L";",1));
	}
	m_Emit(String(L"}",1));
	m_EndLocalScope();
	bb_list_Enumerator2* t_2=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_Decl* t_decl2=t_2->m_NextObject();
		bb_decl_FuncDecl* t_fdecl2=dynamic_cast<bb_decl_FuncDecl*>(t_decl2);
		if((t_fdecl2)!=0){
			m_EmitFuncDecl(t_fdecl2);
			continue;
		}
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl2);
		if((t_gdecl)!=0){
			m_Emit(m_TransRefType(t_gdecl->f_type)+String(L" ",1)+t_classid+String(L"::",2)+t_gdecl->f_munged+String(L";",1));
			continue;
		}
	}
	m_Emit(String(L"void ",5)+t_classid+String(L"::mark(){",9));
	if((t_classDecl->f_superClass)!=0){
		m_Emit(t_classDecl->f_superClass->f_munged+String(L"::mark();",9));
	}
	bb_list_Enumerator2* t_3=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_3->m_HasNext()){
		bb_decl_Decl* t_decl3=t_3->m_NextObject();
		bb_decl_FieldDecl* t_fdecl3=dynamic_cast<bb_decl_FieldDecl*>(t_decl3);
		if((t_fdecl3)!=0){
			m_EmitMark(m_TransField(t_fdecl3,0),t_fdecl3->f_type,true);
		}
	}
	m_Emit(String(L"}",1));
	if(bb_config_ENV_CONFIG==String(L"debug",5)){
		m_Emit(String(L"String ",7)+t_classid+String(L"::debug(){",10));
		m_Emit(String(L"String t=\"(",11)+t_classDecl->f_ident+String(L")\\n\";",5));
		if(((t_classDecl->f_superClass)!=0) && !((t_classDecl->f_superClass->m_IsExtern())!=0)){
			m_Emit(String(L"t=",2)+t_classDecl->f_superClass->f_munged+String(L"::debug()+t;",12));
		}
		bb_list_Enumerator2* t_4=t_classDecl->m_Decls()->m_ObjectEnumerator();
		while(t_4->m_HasNext()){
			bb_decl_Decl* t_decl4=t_4->m_NextObject();
			if(!((t_decl4->m_IsSemanted())!=0)){
				continue;
			}
			if((dynamic_cast<bb_decl_FieldDecl*>(t_decl4))!=0){
				m_Emit(String(L"t+=dbg_decl(\"",13)+t_decl4->f_ident+String(L"\",&",3)+t_decl4->f_munged+String(L");",2));
			}else{
				if((dynamic_cast<bb_decl_GlobalDecl*>(t_decl4))!=0){
					m_Emit(String(L"t+=dbg_decl(\"",13)+t_decl4->f_ident+String(L"\",&",3)+t_classDecl->f_munged+String(L"::",2)+t_decl4->f_munged+String(L");",2));
				}
			}
		}
		m_Emit(String(L"return t;",9));
		m_Emit(String(L"}",1));
	}
	return 0;
}
String bb_cpptranslator_CppTranslator::m_TransApp(bb_decl_AppDecl* t_app){
	t_app->f_mainFunc->f_munged=String(L"bbMain",6);
	bb_map_ValueEnumerator* t_=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_ModuleDecl* t_decl=t_->m_NextObject();
		m_MungDecl(t_decl);
	}
	bb_list_Enumerator2* t_2=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_Decl* t_decl2=t_2->m_NextObject();
		m_MungDecl(t_decl2);
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl2);
		if(!((t_cdecl)!=0)){
			continue;
		}
		m_Emit(String(L"class ",6)+t_decl2->f_munged+String(L";",1));
		bb_list_Enumerator2* t_3=t_cdecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl3=t_3->m_NextObject();
			m_MungDecl(t_decl3);
		}
	}
	bb_list_Enumerator2* t_4=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_4->m_HasNext()){
		bb_decl_Decl* t_decl4=t_4->m_NextObject();
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl4);
		if((t_gdecl)!=0){
			m_Emit(String(L"extern ",7)+m_TransRefType(t_gdecl->f_type)+String(L" ",1)+t_gdecl->f_munged+String(L";",1));
			continue;
		}
		bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl4);
		if((t_fdecl)!=0){
			m_EmitFuncProto(t_fdecl);
			continue;
		}
		bb_decl_ClassDecl* t_cdecl2=dynamic_cast<bb_decl_ClassDecl*>(t_decl4);
		if((t_cdecl2)!=0){
			m_EmitClassProto(t_cdecl2);
			continue;
		}
	}
	bb_list_Enumerator2* t_5=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_5->m_HasNext()){
		bb_decl_Decl* t_decl5=t_5->m_NextObject();
		bb_decl_GlobalDecl* t_gdecl2=dynamic_cast<bb_decl_GlobalDecl*>(t_decl5);
		if((t_gdecl2)!=0){
			m_Emit(m_TransRefType(t_gdecl2->f_type)+String(L" ",1)+t_gdecl2->f_munged+String(L";",1));
			continue;
		}
		bb_decl_FuncDecl* t_fdecl2=dynamic_cast<bb_decl_FuncDecl*>(t_decl5);
		if((t_fdecl2)!=0){
			m_EmitFuncDecl(t_fdecl2);
			continue;
		}
		bb_decl_ClassDecl* t_cdecl3=dynamic_cast<bb_decl_ClassDecl*>(t_decl5);
		if((t_cdecl3)!=0){
			m_EmitClassDecl(t_cdecl3);
			continue;
		}
	}
	m_BeginLocalScope();
	m_Emit(String(L"int bbInit(){",13));
	bb_list_Enumerator6* t_6=t_app->f_semantedGlobals->m_ObjectEnumerator();
	while(t_6->m_HasNext()){
		bb_decl_GlobalDecl* t_decl6=t_6->m_NextObject();
		String t_munged=m_TransGlobal(t_decl6);
		m_Emit(t_munged+String(L"=",1)+t_decl6->f_init->m_Trans()+String(L";",1));
		if(bb_config_ENV_CONFIG==String(L"debug",5)){
			m_Emit(String(L"DBG_GLOBAL(\"",12)+t_decl6->f_ident+String(L"\",&",3)+t_munged+String(L");",2));
		}
	}
	m_Emit(String(L"return 0;",9));
	m_Emit(String(L"}",1));
	m_EndLocalScope();
	m_Emit(String(L"void gc_mark(){",15));
	bb_list_Enumerator6* t_7=t_app->f_semantedGlobals->m_ObjectEnumerator();
	while(t_7->m_HasNext()){
		bb_decl_GlobalDecl* t_decl7=t_7->m_NextObject();
		m_EmitMark(m_TransGlobal(t_decl7),t_decl7->f_type,true);
	}
	m_Emit(String(L"}",1));
	return m_JoinLines();
}
int bb_cpptranslator_CppTranslator::m_CheckSafe(bb_decl_Decl* t_decl){
	if(!((f_unsafe)!=0) || ((t_decl->m_IsExtern())!=0) || t_decl->f_ident.EndsWith(String(L"__UNSAFE__",10))){
		return 0;
	}
	bb_config_Err(String(L"Unsafe call!!!!!",16));
	return 0;
}
String bb_cpptranslator_CppTranslator::m_TransArgs3(Array<bb_expr_Expr* > t_args,bb_decl_FuncDecl* t_decl){
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_arg=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_arg->m_Trans();
	}
	return m_Bra(t_t);
}
String bb_cpptranslator_CppTranslator::m_TransFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args,bb_expr_Expr* t_lhs){
	m_CheckSafe(t_decl);
	if(t_decl->m_IsMethod()){
		if((t_lhs)!=0){
			return m_TransSubExpr(t_lhs,2)+String(L"->",2)+t_decl->f_munged+m_TransArgs3(t_args,t_decl);
		}
		return t_decl->f_munged+m_TransArgs3(t_args,t_decl);
	}
	return m_TransStatic(t_decl)+m_TransArgs3(t_args,t_decl);
}
String bb_cpptranslator_CppTranslator::m_TransSuperFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args){
	m_CheckSafe(t_decl);
	return t_decl->m_ClassScope()->f_munged+String(L"::",2)+t_decl->f_munged+m_TransArgs3(t_args,t_decl);
}
String bb_cpptranslator_CppTranslator::m_TransConstExpr(bb_expr_ConstExpr* t_expr){
	return m_TransValue(t_expr->f_exprType,t_expr->f_value);
}
String bb_cpptranslator_CppTranslator::m_TransNewObjectExpr(bb_expr_NewObjectExpr* t_expr){
	String t_t=String(L"(new ",5)+t_expr->f_classDecl->f_munged+String(L")",1);
	if((t_expr->f_ctor)!=0){
		t_t=t_t+(String(L"->",2)+t_expr->f_ctor->f_munged+m_TransArgs3(t_expr->f_args,t_expr->f_ctor));
	}
	return t_t;
}
String bb_cpptranslator_CppTranslator::m_TransNewArrayExpr(bb_expr_NewArrayExpr* t_expr){
	String t_texpr=t_expr->f_expr->m_Trans();
	return String(L"Array<",6)+m_TransRefType(t_expr->f_ty)+String(L" >",2)+m_Bra(t_expr->f_expr->m_Trans());
}
String bb_cpptranslator_CppTranslator::m_TransSelfExpr(bb_expr_SelfExpr* t_expr){
	return String(L"this",4);
}
String bb_cpptranslator_CppTranslator::m_TransCastExpr(bb_expr_CastExpr* t_expr){
	String t_t=m_Bra(t_expr->f_expr->m_Trans());
	bb_type_Type* t_dst=t_expr->f_exprType;
	bb_type_Type* t_src=t_expr->f_expr->f_exprType;
	if((dynamic_cast<bb_type_BoolType*>(t_dst))!=0){
		if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
			return t_t;
		}
		if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
			return m_Bra(t_t+String(L"!=0",3));
		}
		if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
			return m_Bra(t_t+String(L"!=0",3));
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_src))!=0){
			return m_Bra(t_t+String(L".Length()!=0",12));
		}
		if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
			return m_Bra(t_t+String(L".Length()!=0",12));
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_src))!=0){
			return m_Bra(t_t+String(L"!=0",3));
		}
	}else{
		if((dynamic_cast<bb_type_IntType*>(t_dst))!=0){
			if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
				return m_Bra(t_t+String(L"?1:0",4));
			}
			if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
				return t_t;
			}
			if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
				return String(L"int",3)+m_Bra(t_t);
			}
			if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
				return t_t+String(L".ToInt()",8);
			}
		}else{
			if((dynamic_cast<bb_type_FloatType*>(t_dst))!=0){
				if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
					return String(L"Float",5)+m_Bra(t_t);
				}
				if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
					return t_t;
				}
				if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
					return t_t+String(L".ToFloat()",10);
				}
			}else{
				if((dynamic_cast<bb_type_StringType*>(t_dst))!=0){
					if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
						return String(L"String",6)+m_Bra(t_t);
					}
					if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
						return String(L"String",6)+m_Bra(t_t);
					}
					if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
						return t_t;
					}
				}else{
					if(((dynamic_cast<bb_type_ObjectType*>(t_dst))!=0) && ((dynamic_cast<bb_type_ObjectType*>(t_src))!=0)){
						if(((t_src->m_GetClass()->m_IsInterface())!=0) && !((t_dst->m_GetClass()->m_IsInterface())!=0)){
							return String(L"dynamic_cast<",13)+m_TransType(t_dst)+String(L">",1)+m_Bra(t_t);
						}else{
							if((t_src->m_GetClass()->m_ExtendsClass(t_dst->m_GetClass()))!=0){
								return t_t;
							}else{
								return String(L"dynamic_cast<",13)+m_TransType(t_dst)+String(L">",1)+m_Bra(t_t);
							}
						}
					}
				}
			}
		}
	}
	bb_config_Err(String(L"C++ translator can't convert ",29)+t_src->m_ToString()+String(L" to ",4)+t_dst->m_ToString());
	return String();
}
String bb_cpptranslator_CppTranslator::m_TransUnaryExpr(bb_expr_UnaryExpr* t_expr){
	int t_pri=m_ExprPri(t_expr);
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,t_pri);
	return m_TransUnaryOp(t_expr->f_op)+t_t_expr;
}
String bb_cpptranslator_CppTranslator::m_TransBinaryExpr(bb_expr_BinaryExpr* t_expr){
	int t_pri=m_ExprPri(t_expr);
	String t_t_lhs=m_TransSubExpr(t_expr->f_lhs,t_pri);
	String t_t_rhs=m_TransSubExpr(t_expr->f_rhs,t_pri-1);
	if(t_expr->f_op==String(L"mod",3) && ((dynamic_cast<bb_type_FloatType*>(t_expr->f_exprType))!=0)){
		return String(L"fmod(",5)+t_t_lhs+String(L",",1)+t_t_rhs+String(L")",1);
	}
	return t_t_lhs+m_TransBinaryOp(t_expr->f_op,t_t_rhs)+t_t_rhs;
}
String bb_cpptranslator_CppTranslator::m_TransIndexExpr(bb_expr_IndexExpr* t_expr){
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,2);
	String t_t_index=t_expr->f_index->m_Trans();
	if((dynamic_cast<bb_type_StringType*>(t_expr->f_expr->f_exprType))!=0){
		return String(L"(int)",5)+t_t_expr+String(L"[",1)+t_t_index+String(L"]",1);
	}
	if(bb_config_ENV_CONFIG==String(L"debug",5)){
		return t_t_expr+String(L".At(",4)+t_t_index+String(L")",1);
	}
	return t_t_expr+String(L"[",1)+t_t_index+String(L"]",1);
}
String bb_cpptranslator_CppTranslator::m_TransSliceExpr(bb_expr_SliceExpr* t_expr){
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,2);
	String t_t_args=String(L"0",1);
	if((t_expr->f_from)!=0){
		t_t_args=t_expr->f_from->m_Trans();
	}
	if((t_expr->f_term)!=0){
		t_t_args=t_t_args+(String(L",",1)+t_expr->f_term->m_Trans());
	}
	return t_t_expr+String(L".Slice(",7)+t_t_args+String(L")",1);
}
String bb_cpptranslator_CppTranslator::m_TransArrayExpr(bb_expr_ArrayExpr* t_expr){
	bb_type_Type* t_elemType=dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType)->f_elemType;
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_expr->f_exprs;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_elem=t_[t_2];
		t_2=t_2+1;
		String t_e=t_elem->m_Trans();
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_e;
	}
	bb_decl_LocalDecl* t_tmp=(new bb_decl_LocalDecl)->g_new(String(),0,(bb_type_Type::g_voidType),0);
	m_MungDecl(t_tmp);
	m_Emit(m_TransRefType(t_elemType)+String(L" ",1)+t_tmp->f_munged+String(L"[]={",4)+t_t+String(L"};",2));
	return String(L"Array<",6)+m_TransRefType(t_elemType)+String(L" >(",3)+t_tmp->f_munged+String(L",",1)+String(t_expr->f_exprs.Length())+String(L")",1);
}
String bb_cpptranslator_CppTranslator::m_TransIntrinsicExpr(bb_decl_Decl* t_decl,bb_expr_Expr* t_expr,Array<bb_expr_Expr* > t_args){
	String t_texpr=String();
	String t_arg0=String();
	String t_arg1=String();
	String t_arg2=String();
	if((t_expr)!=0){
		t_texpr=m_TransSubExpr(t_expr,2);
	}
	if(t_args.Length()>0 && ((t_args[0])!=0)){
		t_arg0=t_args[0]->m_Trans();
	}
	if(t_args.Length()>1 && ((t_args[1])!=0)){
		t_arg1=t_args[1]->m_Trans();
	}
	if(t_args.Length()>2 && ((t_args[2])!=0)){
		t_arg2=t_args[2]->m_Trans();
	}
	String t_id=t_decl->f_munged.Slice(1);
	String t_id2=t_id.Slice(0,1).ToUpper()+t_id.Slice(1);
	String t_=t_id;
	if(t_==String(L"print",5)){
		return String(L"Print",5)+m_Bra(t_arg0);
	}else{
		if(t_==String(L"error",5)){
			return String(L"Error",5)+m_Bra(t_arg0);
		}else{
			if(t_==String(L"debuglog",8)){
				return String(L"DebugLog",8)+m_Bra(t_arg0);
			}else{
				if(t_==String(L"debugstop",9)){
					return String(L"DebugStop()",11);
				}else{
					if(t_==String(L"length",6)){
						return t_texpr+String(L".Length()",9);
					}else{
						if(t_==String(L"resize",6)){
							return t_texpr+String(L".Resize",7)+m_Bra(t_arg0);
						}else{
							if(t_==String(L"compare",7)){
								return t_texpr+String(L".Compare",8)+m_Bra(t_arg0);
							}else{
								if(t_==String(L"find",4)){
									return t_texpr+String(L".Find",5)+m_Bra(t_arg0+String(L",",1)+t_arg1);
								}else{
									if(t_==String(L"findlast",8)){
										return t_texpr+String(L".FindLast",9)+m_Bra(t_arg0);
									}else{
										if(t_==String(L"findlast2",9)){
											return t_texpr+String(L".FindLast",9)+m_Bra(t_arg0+String(L",",1)+t_arg1);
										}else{
											if(t_==String(L"trim",4)){
												return t_texpr+String(L".Trim()",7);
											}else{
												if(t_==String(L"join",4)){
													return t_texpr+String(L".Join",5)+m_Bra(t_arg0);
												}else{
													if(t_==String(L"split",5)){
														return t_texpr+String(L".Split",6)+m_Bra(t_arg0);
													}else{
														if(t_==String(L"replace",7)){
															return t_texpr+String(L".Replace",8)+m_Bra(t_arg0+String(L",",1)+t_arg1);
														}else{
															if(t_==String(L"tolower",7)){
																return t_texpr+String(L".ToLower()",10);
															}else{
																if(t_==String(L"toupper",7)){
																	return t_texpr+String(L".ToUpper()",10);
																}else{
																	if(t_==String(L"contains",8)){
																		return t_texpr+String(L".Contains",9)+m_Bra(t_arg0);
																	}else{
																		if(t_==String(L"startswith",10)){
																			return t_texpr+String(L".StartsWith",11)+m_Bra(t_arg0);
																		}else{
																			if(t_==String(L"endswith",8)){
																				return t_texpr+String(L".EndsWith",9)+m_Bra(t_arg0);
																			}else{
																				if(t_==String(L"tochars",7)){
																					return t_texpr+String(L".ToChars()",10);
																				}else{
																					if(t_==String(L"fromchar",8)){
																						return String(L"String",6)+m_Bra(String(L"(Char)",6)+m_Bra(t_arg0)+String(L",1",2));
																					}else{
																						if(t_==String(L"fromchars",9)){
																							return String(L"String::FromChars",17)+m_Bra(t_arg0);
																						}else{
																							if(t_==String(L"sin",3) || t_==String(L"cos",3) || t_==String(L"tan",3)){
																								return String(L"(Float)",7)+t_id+m_Bra(m_Bra(t_arg0)+String(L"*D2R",4));
																							}else{
																								if(t_==String(L"asin",4) || t_==String(L"acos",4) || t_==String(L"atan",4)){
																									return String(L"(Float)",7)+m_Bra(t_id+m_Bra(t_arg0)+String(L"*R2D",4));
																								}else{
																									if(t_==String(L"atan2",5)){
																										return String(L"(Float)",7)+m_Bra(t_id+m_Bra(t_arg0+String(L",",1)+t_arg1)+String(L"*R2D",4));
																									}else{
																										if(t_==String(L"sinr",4) || t_==String(L"cosr",4) || t_==String(L"tanr",4)){
																											return String(L"(Float)",7)+t_id.Slice(0,-1)+m_Bra(t_arg0);
																										}else{
																											if(t_==String(L"asinr",5) || t_==String(L"acosr",5) || t_==String(L"atanr",5)){
																												return String(L"(Float)",7)+t_id.Slice(0,-1)+m_Bra(t_arg0);
																											}else{
																												if(t_==String(L"atan2r",6)){
																													return String(L"(Float)",7)+t_id.Slice(0,-1)+m_Bra(t_arg0+String(L",",1)+t_arg1);
																												}else{
																													if(t_==String(L"sqrt",4) || t_==String(L"floor",5) || t_==String(L"ceil",4) || t_==String(L"log",3) || t_==String(L"exp",3)){
																														return String(L"(Float)",7)+t_id+m_Bra(t_arg0);
																													}else{
																														if(t_==String(L"pow",3)){
																															return String(L"(Float)",7)+t_id+m_Bra(t_arg0+String(L",",1)+t_arg1);
																														}
																													}
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cpptranslator_CppTranslator::m_TransTryStmt(bb_stmt_TryStmt* t_stmt){
	m_Emit(String(L"try{",4));
	int t_unr=m_EmitBlock(t_stmt->f_block,true);
	Array<bb_stmt_CatchStmt* > t_=t_stmt->f_catches;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_stmt_CatchStmt* t_c=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_c->f_init);
		m_Emit(String(L"}catch(",7)+m_TransType(t_c->f_init->f_type)+String(L" ",1)+t_c->f_init->f_munged+String(L"){",2));
		f_dbgLocals->m_Push22(t_c->f_init);
		int t_unr2=m_EmitBlock(t_c->f_block,true);
	}
	m_Emit(String(L"}",1));
	return String();
}
String bb_cpptranslator_CppTranslator::m_TransDeclStmt(bb_stmt_DeclStmt* t_stmt){
	bb_decl_LocalDecl* t_decl=dynamic_cast<bb_decl_LocalDecl*>(t_stmt->f_decl);
	if((t_decl)!=0){
		if((t_decl->f_ident).Length()!=0){
			f_dbgLocals->m_Push22(t_decl);
		}
		m_MungDecl(t_decl);
		return m_TransLocalDecl(t_decl->f_munged,t_decl->f_init);
	}
	bb_decl_ConstDecl* t_cdecl=dynamic_cast<bb_decl_ConstDecl*>(t_stmt->f_decl);
	if((t_cdecl)!=0){
		return String();
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cpptranslator_CppTranslator::m_TransAssignStmt2(bb_stmt_AssignStmt* t_stmt){
	bb_type_Type* t_ty=t_stmt->f_lhs->f_exprType;
	if(((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0) || ((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0)){
		if(((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0) && ((dynamic_cast<bb_expr_ConstExpr*>(t_stmt->f_rhs))!=0)){
			return bb_translator_CTranslator::m_TransAssignStmt2(t_stmt);
		}
		if(((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0) && !((t_ty->m_GetClass()->m_ExtendsObject())!=0)){
			return bb_translator_CTranslator::m_TransAssignStmt2(t_stmt);
		}
		bb_expr_VarExpr* t_varExpr=dynamic_cast<bb_expr_VarExpr*>(t_stmt->f_lhs);
		if(((t_varExpr)!=0) && ((dynamic_cast<bb_decl_LocalDecl*>(t_varExpr->f_decl))!=0)){
			return bb_translator_CTranslator::m_TransAssignStmt2(t_stmt);
		}
		String t_t_lhs=t_stmt->f_lhs->m_TransVar();
		String t_t_rhs=t_stmt->f_rhs->m_Trans();
		return String(L"gc_assign(",10)+t_t_lhs+String(L",",1)+t_t_rhs+String(L")",1);
	}
	return bb_translator_CTranslator::m_TransAssignStmt2(t_stmt);
}
void bb_cpptranslator_CppTranslator::mark(){
	bb_translator_CTranslator::mark();
	gc_mark_q(f_dbgLocals);
}
bb_cstranslator_CsTranslator::bb_cstranslator_CsTranslator(){
}
bb_cstranslator_CsTranslator* bb_cstranslator_CsTranslator::g_new(){
	bb_translator_CTranslator::g_new();
	return this;
}
String bb_cstranslator_CsTranslator::m_TransType(bb_type_Type* t_ty){
	if((dynamic_cast<bb_type_VoidType*>(t_ty))!=0){
		return String(L"void",4);
	}
	if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
		return String(L"bool",4);
	}
	if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
		return String(L"int",3);
	}
	if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
		return String(L"float",5);
	}
	if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
		return String(L"String",6);
	}
	if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
		return m_TransType(dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType)+String(L"[]",2);
	}
	if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
		return t_ty->m_GetClass()->f_munged;
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cstranslator_CsTranslator::m_TransValue(bb_type_Type* t_ty,String t_value){
	if((t_value).Length()!=0){
		if(((dynamic_cast<bb_type_IntType*>(t_ty))!=0) && t_value.StartsWith(String(L"$",1))){
			return String(L"0x",2)+t_value.Slice(1);
		}
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"true",4);
		}
		if((dynamic_cast<bb_type_IntType*>(t_ty))!=0){
			return t_value;
		}
		if((dynamic_cast<bb_type_FloatType*>(t_ty))!=0){
			return t_value+String(L"f",1);
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return m_Enquote(t_value);
		}
	}else{
		if((dynamic_cast<bb_type_BoolType*>(t_ty))!=0){
			return String(L"false",5);
		}
		if((dynamic_cast<bb_type_NumericType*>(t_ty))!=0){
			return String(L"0",1);
		}
		if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
			return String(L"\"\"",2);
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
			bb_type_Type* t_elemTy=dynamic_cast<bb_type_ArrayType*>(t_ty)->f_elemType;
			String t_t=String(L"[0]",3);
			while((dynamic_cast<bb_type_ArrayType*>(t_elemTy))!=0){
				t_elemTy=dynamic_cast<bb_type_ArrayType*>(t_elemTy)->f_elemType;
				t_t=t_t+String(L"[]",2);
			}
			return String(L"new ",4)+m_TransType(t_elemTy)+t_t;
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_ty))!=0){
			return String(L"null",4);
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cstranslator_CsTranslator::m_TransLocalDecl(String t_munged,bb_expr_Expr* t_init){
	return m_TransType(t_init->f_exprType)+String(L" ",1)+t_munged+String(L"=",1)+t_init->m_Trans();
}
int bb_cstranslator_CsTranslator::m_EmitEnter(bb_decl_FuncDecl* t_func){
	m_Emit(String(L"bb_std_lang.pushErr();",22));
	return 0;
}
int bb_cstranslator_CsTranslator::m_EmitSetErr(String t_info){
	m_Emit(String(L"bb_std_lang.errInfo=\"",21)+t_info.Replace(String(L"\\",1),String(L"/",1))+String(L"\";",2));
	return 0;
}
int bb_cstranslator_CsTranslator::m_EmitLeave(){
	m_Emit(String(L"bb_std_lang.popErr();",21));
	return 0;
}
String bb_cstranslator_CsTranslator::m_TransStatic(bb_decl_Decl* t_decl){
	if((t_decl->m_IsExtern())!=0){
		return t_decl->f_munged;
	}else{
		if(((bb_decl__env)!=0) && ((t_decl->f_scope)!=0) && t_decl->f_scope==(bb_decl__env->m_ClassScope())){
			return t_decl->f_munged;
		}else{
			if((dynamic_cast<bb_decl_ClassDecl*>(t_decl->f_scope))!=0){
				return t_decl->f_scope->f_munged+String(L".",1)+t_decl->f_munged;
			}else{
				if((dynamic_cast<bb_decl_ModuleDecl*>(t_decl->f_scope))!=0){
					return t_decl->f_scope->f_munged+String(L".",1)+t_decl->f_munged;
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cstranslator_CsTranslator::m_TransGlobal(bb_decl_GlobalDecl* t_decl){
	return m_TransStatic(t_decl);
}
String bb_cstranslator_CsTranslator::m_TransField(bb_decl_FieldDecl* t_decl,bb_expr_Expr* t_lhs){
	if((t_lhs)!=0){
		return m_TransSubExpr(t_lhs,2)+String(L".",1)+t_decl->f_munged;
	}
	return t_decl->f_munged;
}
int bb_cstranslator_CsTranslator::m_EmitFuncDecl(bb_decl_FuncDecl* t_decl){
	m_BeginLocalScope();
	String t_args=String();
	Array<bb_decl_ArgDecl* > t_=t_decl->f_argDecls;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ArgDecl* t_arg=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_arg);
		if((t_args).Length()!=0){
			t_args=t_args+String(L",",1);
		}
		t_args=t_args+(m_TransType(t_arg->f_type)+String(L" ",1)+t_arg->f_munged);
	}
	String t_t=m_TransType(t_decl->f_retType)+String(L" ",1)+t_decl->f_munged+m_Bra(t_args);
	if(((t_decl->m_ClassScope())!=0) && ((t_decl->m_ClassScope()->m_IsInterface())!=0)){
		m_Emit(t_t+String(L";",1));
	}else{
		if((t_decl->m_IsAbstract())!=0){
			if((t_decl->f_overrides)!=0){
				m_Emit(String(L"public abstract override ",25)+t_t+String(L";",1));
			}else{
				m_Emit(String(L"public abstract ",16)+t_t+String(L";",1));
			}
		}else{
			String t_q=String(L"public ",7);
			if(t_decl->m_IsStatic()){
				t_q=t_q+String(L"static ",7);
			}else{
				if(((t_decl->f_overrides)!=0) && !t_decl->m_IsCtor()){
					t_q=t_q+String(L"override ",9);
				}else{
					t_q=t_q+String(L"virtual ",8);
				}
			}
			m_Emit(t_q+t_t+String(L"{",1));
			m_EmitBlock((t_decl),true);
			m_Emit(String(L"}",1));
		}
	}
	m_EndLocalScope();
	return 0;
}
String bb_cstranslator_CsTranslator::m_TransDecl(bb_decl_Decl* t_decl){
	bb_decl_ValDecl* t_vdecl=dynamic_cast<bb_decl_ValDecl*>(t_decl);
	if((t_vdecl)!=0){
		return m_TransType(t_vdecl->f_type)+String(L" ",1)+t_decl->f_munged;
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
int bb_cstranslator_CsTranslator::m_EmitClassDecl(bb_decl_ClassDecl* t_classDecl){
	String t_classid=t_classDecl->f_munged;
	if((t_classDecl->m_IsInterface())!=0){
		String t_bases=String();
		Array<bb_decl_ClassDecl* > t_=t_classDecl->f_implments;
		int t_2=0;
		while(t_2<t_.Length()){
			bb_decl_ClassDecl* t_iface=t_[t_2];
			t_2=t_2+1;
			if((t_bases).Length()!=0){
				t_bases=t_bases+String(L",",1);
			}else{
				t_bases=String(L" : ",3);
			}
			t_bases=t_bases+t_iface->f_munged;
		}
		m_Emit(String(L"interface ",10)+t_classid+t_bases+String(L"{",1));
		bb_list_Enumerator2* t_3=t_classDecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl=t_3->m_NextObject();
			bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl);
			if(!((t_fdecl)!=0)){
				continue;
			}
			m_EmitFuncDecl(t_fdecl);
		}
		m_Emit(String(L"}",1));
		return 0;
	}
	String t_superid=t_classDecl->f_superClass->f_munged;
	String t_bases2=String(L" : ",3)+t_superid;
	Array<bb_decl_ClassDecl* > t_4=t_classDecl->f_implments;
	int t_5=0;
	while(t_5<t_4.Length()){
		bb_decl_ClassDecl* t_iface2=t_4[t_5];
		t_5=t_5+1;
		t_bases2=t_bases2+(String(L",",1)+t_iface2->f_munged);
	}
	String t_q=String();
	if((t_classDecl->m_IsAbstract())!=0){
		t_q=t_q+String(L"abstract ",9);
	}
	m_Emit(t_q+String(L"class ",6)+t_classid+t_bases2+String(L"{",1));
	bb_list_Enumerator2* t_6=t_classDecl->m_Semanted()->m_ObjectEnumerator();
	while(t_6->m_HasNext()){
		bb_decl_Decl* t_decl2=t_6->m_NextObject();
		bb_decl_FieldDecl* t_tdecl=dynamic_cast<bb_decl_FieldDecl*>(t_decl2);
		if((t_tdecl)!=0){
			m_Emit(String(L"public ",7)+m_TransDecl(t_tdecl)+String(L"=",1)+t_tdecl->f_init->m_Trans()+String(L";",1));
			continue;
		}
		bb_decl_FuncDecl* t_fdecl2=dynamic_cast<bb_decl_FuncDecl*>(t_decl2);
		if((t_fdecl2)!=0){
			m_EmitFuncDecl(t_fdecl2);
			continue;
		}
		bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl2);
		if((t_gdecl)!=0){
			m_Emit(String(L"public static ",14)+m_TransDecl(t_gdecl)+String(L";",1));
			continue;
		}
	}
	m_Emit(String(L"}",1));
	return 0;
}
String bb_cstranslator_CsTranslator::m_TransApp(bb_decl_AppDecl* t_app){
	t_app->f_mainModule->f_munged=String(L"bb_",3);
	t_app->f_mainFunc->f_munged=String(L"bbMain",6);
	bb_map_ValueEnumerator* t_=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_->m_HasNext()){
		bb_decl_ModuleDecl* t_decl=t_->m_NextObject();
		m_MungDecl(t_decl);
	}
	bb_list_Enumerator2* t_2=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_2->m_HasNext()){
		bb_decl_Decl* t_decl2=t_2->m_NextObject();
		m_MungDecl(t_decl2);
		bb_decl_ClassDecl* t_cdecl=dynamic_cast<bb_decl_ClassDecl*>(t_decl2);
		if(!((t_cdecl)!=0)){
			continue;
		}
		bb_list_Enumerator2* t_3=t_cdecl->m_Semanted()->m_ObjectEnumerator();
		while(t_3->m_HasNext()){
			bb_decl_Decl* t_decl3=t_3->m_NextObject();
			if(((dynamic_cast<bb_decl_FuncDecl*>(t_decl3))!=0) && dynamic_cast<bb_decl_FuncDecl*>(t_decl3)->m_IsCtor()){
				t_decl3->f_ident=t_cdecl->f_ident+String(L"_",1)+t_decl3->f_ident;
			}
			m_MungDecl(t_decl3);
		}
	}
	bb_list_Enumerator2* t_4=t_app->m_Semanted()->m_ObjectEnumerator();
	while(t_4->m_HasNext()){
		bb_decl_Decl* t_decl4=t_4->m_NextObject();
		bb_decl_ClassDecl* t_cdecl2=dynamic_cast<bb_decl_ClassDecl*>(t_decl4);
		if((t_cdecl2)!=0){
			m_EmitClassDecl(t_cdecl2);
			continue;
		}
	}
	bb_map_ValueEnumerator* t_5=t_app->f_imported->m_Values()->m_ObjectEnumerator();
	while(t_5->m_HasNext()){
		bb_decl_ModuleDecl* t_mdecl=t_5->m_NextObject();
		m_Emit(String(L"class ",6)+t_mdecl->f_munged+String(L"{",1));
		bb_list_Enumerator2* t_6=t_mdecl->m_Semanted()->m_ObjectEnumerator();
		while(t_6->m_HasNext()){
			bb_decl_Decl* t_decl5=t_6->m_NextObject();
			if(((t_decl5->m_IsExtern())!=0) || ((t_decl5->f_scope->m_ClassScope())!=0)){
				continue;
			}
			bb_decl_GlobalDecl* t_gdecl=dynamic_cast<bb_decl_GlobalDecl*>(t_decl5);
			if((t_gdecl)!=0){
				m_Emit(String(L"public static ",14)+m_TransDecl(t_gdecl)+String(L";",1));
				continue;
			}
			bb_decl_FuncDecl* t_fdecl=dynamic_cast<bb_decl_FuncDecl*>(t_decl5);
			if((t_fdecl)!=0){
				m_EmitFuncDecl(t_fdecl);
				continue;
			}
		}
		if(t_mdecl==t_app->f_mainModule){
			m_BeginLocalScope();
			m_Emit(String(L"public static int bbInit(){",27));
			bb_list_Enumerator6* t_7=t_app->f_semantedGlobals->m_ObjectEnumerator();
			while(t_7->m_HasNext()){
				bb_decl_GlobalDecl* t_decl6=t_7->m_NextObject();
				m_Emit(m_TransGlobal(t_decl6)+String(L"=",1)+t_decl6->f_init->m_Trans()+String(L";",1));
			}
			m_Emit(String(L"return 0;",9));
			m_Emit(String(L"}",1));
			m_EndLocalScope();
		}
		m_Emit(String(L"}",1));
	}
	return m_JoinLines();
}
String bb_cstranslator_CsTranslator::m_TransArgs2(Array<bb_expr_Expr* > t_args){
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_arg=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_arg->m_Trans();
	}
	return m_Bra(t_t);
}
String bb_cstranslator_CsTranslator::m_TransFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args,bb_expr_Expr* t_lhs){
	if(t_decl->m_IsMethod()){
		if((t_lhs)!=0){
			return m_TransSubExpr(t_lhs,2)+String(L".",1)+t_decl->f_munged+m_TransArgs2(t_args);
		}
		return t_decl->f_munged+m_TransArgs2(t_args);
	}
	return m_TransStatic(t_decl)+m_TransArgs2(t_args);
}
String bb_cstranslator_CsTranslator::m_TransSuperFunc(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args){
	return String(L"base.",5)+t_decl->f_munged+m_TransArgs2(t_args);
}
String bb_cstranslator_CsTranslator::m_TransConstExpr(bb_expr_ConstExpr* t_expr){
	return m_TransValue(t_expr->f_exprType,t_expr->f_value);
}
String bb_cstranslator_CsTranslator::m_TransNewObjectExpr(bb_expr_NewObjectExpr* t_expr){
	String t_t=String(L"(new ",5)+t_expr->f_classDecl->f_munged+String(L"())",3);
	if((t_expr->f_ctor)!=0){
		t_t=t_t+(String(L".",1)+t_expr->f_ctor->f_munged+m_TransArgs2(t_expr->f_args));
	}
	return t_t;
}
String bb_cstranslator_CsTranslator::m_TransNewArrayExpr(bb_expr_NewArrayExpr* t_expr){
	String t_texpr=t_expr->f_expr->m_Trans();
	bb_type_Type* t_elemTy=dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType)->f_elemType;
	if((dynamic_cast<bb_type_StringType*>(t_elemTy))!=0){
		return String(L"bb_std_lang.stringArray",23)+m_Bra(t_texpr);
	}
	String t_t=String(L"[",1)+t_texpr+String(L"]",1);
	while((dynamic_cast<bb_type_ArrayType*>(t_elemTy))!=0){
		t_elemTy=dynamic_cast<bb_type_ArrayType*>(t_elemTy)->f_elemType;
		t_t=t_t+String(L"[]",2);
	}
	return String(L"new ",4)+m_TransType(t_elemTy)+t_t;
}
String bb_cstranslator_CsTranslator::m_TransSelfExpr(bb_expr_SelfExpr* t_expr){
	return String(L"this",4);
}
String bb_cstranslator_CsTranslator::m_TransCastExpr(bb_expr_CastExpr* t_expr){
	bb_type_Type* t_dst=t_expr->f_exprType;
	bb_type_Type* t_src=t_expr->f_expr->f_exprType;
	String t_uexpr=t_expr->f_expr->m_Trans();
	String t_texpr=m_Bra(t_uexpr);
	if((dynamic_cast<bb_type_BoolType*>(t_dst))!=0){
		if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
			return t_texpr;
		}
		if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=0",3));
		}
		if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=0.0f",6));
		}
		if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L".Length!=0",10));
		}
		if((dynamic_cast<bb_type_ArrayType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L".Length!=0",10));
		}
		if((dynamic_cast<bb_type_ObjectType*>(t_src))!=0){
			return m_Bra(t_texpr+String(L"!=null",6));
		}
	}else{
		if((dynamic_cast<bb_type_IntType*>(t_dst))!=0){
			if((dynamic_cast<bb_type_BoolType*>(t_src))!=0){
				return m_Bra(t_texpr+String(L"?1:0",4));
			}
			if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
				return t_texpr;
			}
			if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
				return String(L"(int)",5)+t_texpr;
			}
			if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
				return String(L"int.Parse",9)+t_texpr;
			}
		}else{
			if((dynamic_cast<bb_type_FloatType*>(t_dst))!=0){
				if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
					return String(L"(float)",7)+t_texpr;
				}
				if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
					return t_texpr;
				}
				if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
					if(bb_config_ENV_TARGET==String(L"xna",3)){
						return String(L"float.Parse",11)+m_Bra(t_uexpr+String(L",CultureInfo.InvariantCulture",29));
					}
					return String(L"float.Parse",11)+m_Bra(t_uexpr);
				}
			}else{
				if((dynamic_cast<bb_type_StringType*>(t_dst))!=0){
					if((dynamic_cast<bb_type_IntType*>(t_src))!=0){
						return t_texpr+String(L".ToString()",11);
					}
					if((dynamic_cast<bb_type_FloatType*>(t_src))!=0){
						if(bb_config_ENV_TARGET==String(L"xna",3)){
							return t_texpr+String(L".ToString(CultureInfo.InvariantCulture)",39);
						}
						return t_texpr+String(L".ToString()",11);
					}
					if((dynamic_cast<bb_type_StringType*>(t_src))!=0){
						return t_texpr;
					}
				}else{
					if(((dynamic_cast<bb_type_ObjectType*>(t_dst))!=0) && ((dynamic_cast<bb_type_ObjectType*>(t_src))!=0)){
						if((t_src->m_GetClass()->m_ExtendsClass(t_dst->m_GetClass()))!=0){
							return t_texpr;
						}else{
							bb_decl_LocalDecl* t_tmp=(new bb_decl_LocalDecl)->g_new(String(),0,t_src,0);
							m_MungDecl(t_tmp);
							m_Emit(m_TransType(t_src)+String(L" ",1)+t_tmp->f_munged+String(L"=",1)+t_uexpr+String(L";",1));
							return String(L"($t is $c ? ($c)$t : null)",26).Replace(String(L"$t",2),t_tmp->f_munged).Replace(String(L"$c",2),m_TransType(t_dst));
						}
					}
				}
			}
		}
	}
	bb_config_Err(String(L"CS translator can't convert ",28)+t_src->m_ToString()+String(L" to ",4)+t_dst->m_ToString());
	return String();
}
String bb_cstranslator_CsTranslator::m_TransUnaryExpr(bb_expr_UnaryExpr* t_expr){
	int t_pri=m_ExprPri(t_expr);
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,t_pri);
	return m_TransUnaryOp(t_expr->f_op)+t_t_expr;
}
String bb_cstranslator_CsTranslator::m_TransBinaryExpr(bb_expr_BinaryExpr* t_expr){
	if(((dynamic_cast<bb_expr_BinaryCompareExpr*>(t_expr))!=0) && ((dynamic_cast<bb_type_StringType*>(t_expr->f_lhs->f_exprType))!=0) && ((dynamic_cast<bb_type_StringType*>(t_expr->f_rhs->f_exprType))!=0)){
		return m_Bra(m_TransSubExpr(t_expr->f_lhs,2)+String(L".CompareTo(",11)+t_expr->f_rhs->m_Trans()+String(L")",1)+m_TransBinaryOp(t_expr->f_op,String())+String(L"0",1));
	}
	int t_pri=m_ExprPri(t_expr);
	String t_t_lhs=m_TransSubExpr(t_expr->f_lhs,t_pri);
	String t_t_rhs=m_TransSubExpr(t_expr->f_rhs,t_pri-1);
	return t_t_lhs+m_TransBinaryOp(t_expr->f_op,t_t_rhs)+t_t_rhs;
}
String bb_cstranslator_CsTranslator::m_TransIndexExpr(bb_expr_IndexExpr* t_expr){
	String t_t_expr=m_TransSubExpr(t_expr->f_expr,2);
	String t_t_index=t_expr->f_index->m_Trans();
	if((dynamic_cast<bb_type_StringType*>(t_expr->f_expr->f_exprType))!=0){
		return String(L"(int)",5)+t_t_expr+String(L"[",1)+t_t_index+String(L"]",1);
	}
	return t_t_expr+String(L"[",1)+t_t_index+String(L"]",1);
}
String bb_cstranslator_CsTranslator::m_TransSliceExpr(bb_expr_SliceExpr* t_expr){
	String t_t_expr=t_expr->f_expr->m_Trans();
	String t_t_args=String(L"0",1);
	if((t_expr->f_from)!=0){
		t_t_args=t_expr->f_from->m_Trans();
	}
	if((t_expr->f_term)!=0){
		t_t_args=t_t_args+(String(L",",1)+t_expr->f_term->m_Trans());
	}
	return String(L"((",2)+m_TransType(t_expr->f_exprType)+String(L")bb_std_lang.slice(",19)+t_t_expr+String(L",",1)+t_t_args+String(L"))",2);
}
String bb_cstranslator_CsTranslator::m_TransArrayExpr(bb_expr_ArrayExpr* t_expr){
	String t_t=String();
	Array<bb_expr_Expr* > t_=t_expr->f_exprs;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_elem=t_[t_2];
		t_2=t_2+1;
		if((t_t).Length()!=0){
			t_t=t_t+String(L",",1);
		}
		t_t=t_t+t_elem->m_Trans();
	}
	return String(L"new ",4)+m_TransType(t_expr->f_exprType)+String(L"{",1)+t_t+String(L"}",1);
}
String bb_cstranslator_CsTranslator::m_TransIntrinsicExpr(bb_decl_Decl* t_decl,bb_expr_Expr* t_expr,Array<bb_expr_Expr* > t_args){
	String t_texpr=String();
	String t_arg0=String();
	String t_arg1=String();
	String t_arg2=String();
	if((t_expr)!=0){
		t_texpr=m_TransSubExpr(t_expr,2);
	}
	if(t_args.Length()>0 && ((t_args[0])!=0)){
		t_arg0=t_args[0]->m_Trans();
	}
	if(t_args.Length()>1 && ((t_args[1])!=0)){
		t_arg1=t_args[1]->m_Trans();
	}
	if(t_args.Length()>2 && ((t_args[2])!=0)){
		t_arg2=t_args[2]->m_Trans();
	}
	String t_id=t_decl->f_munged.Slice(1);
	String t_id2=t_id.Slice(0,1).ToUpper()+t_id.Slice(1);
	String t_=t_id;
	if(t_==String(L"print",5)){
		return String(L"bb_std_lang.Print",17)+m_Bra(t_arg0);
	}else{
		if(t_==String(L"error",5)){
			return String(L"bb_std_lang.Error",17)+m_Bra(t_arg0);
		}else{
			if(t_==String(L"debuglog",8)){
				return String(L"bb_std_lang.DebugLog",20)+m_Bra(t_arg0);
			}else{
				if(t_==String(L"debugstop",9)){
					return String(L"bb_std_lang.DebugStop()",23);
				}else{
					if(t_==String(L"length",6)){
						if((dynamic_cast<bb_type_StringType*>(t_expr->f_exprType))!=0){
							return t_texpr+String(L".Length",7);
						}
						return String(L"bb_std_lang.length",18)+m_Bra(t_texpr);
					}else{
						if(t_==String(L"resize",6)){
							String t_fn=String(L"resizeArray",11);
							bb_type_Type* t_ty=dynamic_cast<bb_type_ArrayType*>(t_expr->f_exprType)->f_elemType;
							if((dynamic_cast<bb_type_StringType*>(t_ty))!=0){
								t_fn=String(L"resizeStringArray",17);
							}else{
								if((dynamic_cast<bb_type_ArrayType*>(t_ty))!=0){
									t_fn=String(L"resizeArrayArray",16);
								}
							}
							return String(L"(",1)+m_TransType(t_expr->f_exprType)+String(L")bb_std_lang.",13)+t_fn+m_Bra(t_texpr+String(L",",1)+t_arg0);
						}else{
							if(t_==String(L"compare",7)){
								return t_texpr+String(L".CompareTo",10)+m_Bra(t_arg0);
							}else{
								if(t_==String(L"find",4)){
									return t_texpr+String(L".IndexOf",8)+m_Bra(t_arg0+String(L",",1)+t_arg1);
								}else{
									if(t_==String(L"findlast",8)){
										return t_texpr+String(L".LastIndexOf",12)+m_Bra(t_arg0);
									}else{
										if(t_==String(L"findlast2",9)){
											return t_texpr+String(L".LastIndexOf",12)+m_Bra(t_arg0+String(L",",1)+t_arg1);
										}else{
											if(t_==String(L"trim",4)){
												return t_texpr+String(L".Trim()",7);
											}else{
												if(t_==String(L"join",4)){
													return String(L"String.Join",11)+m_Bra(t_texpr+String(L",",1)+t_arg0);
												}else{
													if(t_==String(L"split",5)){
														return String(L"bb_std_lang.split",17)+m_Bra(t_texpr+String(L",",1)+t_arg0);
													}else{
														if(t_==String(L"replace",7)){
															return t_texpr+String(L".Replace",8)+m_Bra(t_arg0+String(L",",1)+t_arg1);
														}else{
															if(t_==String(L"tolower",7)){
																return t_texpr+String(L".ToLower()",10);
															}else{
																if(t_==String(L"toupper",7)){
																	return t_texpr+String(L".ToUpper()",10);
																}else{
																	if(t_==String(L"contains",8)){
																		return m_Bra(t_texpr+String(L".IndexOf",8)+m_Bra(t_arg0)+String(L"!=-1",4));
																	}else{
																		if(t_==String(L"startswith",10)){
																			return t_texpr+String(L".StartsWith",11)+m_Bra(t_arg0);
																		}else{
																			if(t_==String(L"endswith",8)){
																				return t_texpr+String(L".EndsWith",9)+m_Bra(t_arg0);
																			}else{
																				if(t_==String(L"tochars",7)){
																					return String(L"bb_std_lang.toChars",19)+m_Bra(t_texpr);
																				}else{
																					if(t_==String(L"fromchar",8)){
																						return String(L"new String",10)+m_Bra(String(L"(char)",6)+m_Bra(t_arg0)+String(L",1",2));
																					}else{
																						if(t_==String(L"fromchars",9)){
																							return String(L"bb_std_lang.fromChars",21)+m_Bra(t_arg0);
																						}else{
																							if(t_==String(L"sin",3) || t_==String(L"cos",3) || t_==String(L"tan",3)){
																								return String(L"(float)Math.",12)+t_id2+m_Bra(m_Bra(t_arg0)+String(L"*bb_std_lang.D2R",16));
																							}else{
																								if(t_==String(L"asin",4) || t_==String(L"acos",4) || t_==String(L"atan",4)){
																									return String(L"(float)",7)+m_Bra(String(L"Math.",5)+t_id2+m_Bra(t_arg0)+String(L"*bb_std_lang.R2D",16));
																								}else{
																									if(t_==String(L"atan2",5)){
																										return String(L"(float)",7)+m_Bra(String(L"Math.",5)+t_id2+m_Bra(t_arg0+String(L",",1)+t_arg1)+String(L"*bb_std_lang.R2D",16));
																									}else{
																										if(t_==String(L"sinr",4) || t_==String(L"cosr",4) || t_==String(L"tanr",4)){
																											return String(L"(float)Math.",12)+t_id2.Slice(0,-1)+m_Bra(t_arg0);
																										}else{
																											if(t_==String(L"asinr",5) || t_==String(L"acosr",5) || t_==String(L"atanr",5)){
																												return String(L"(float)Math.",12)+t_id2.Slice(0,-1)+m_Bra(t_arg0);
																											}else{
																												if(t_==String(L"atan2r",6)){
																													return String(L"(float)Math.",12)+t_id2.Slice(0,-1)+m_Bra(t_arg0+String(L",",1)+t_arg1);
																												}else{
																													if(t_==String(L"sqrt",4) || t_==String(L"floor",5) || t_==String(L"log",3) || t_==String(L"exp",3)){
																														return String(L"(float)Math.",12)+t_id2+m_Bra(t_arg0);
																													}else{
																														if(t_==String(L"ceil",4)){
																															return String(L"(float)Math.Ceiling",19)+m_Bra(t_arg0);
																														}else{
																															if(t_==String(L"pow",3)){
																																return String(L"(float)Math.",12)+t_id2+m_Bra(t_arg0+String(L",",1)+t_arg1);
																															}
																														}
																													}
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	bb_config_InternalErr(String(L"Internal error",14));
	return String();
}
String bb_cstranslator_CsTranslator::m_TransTryStmt(bb_stmt_TryStmt* t_stmt){
	m_Emit(String(L"try{",4));
	int t_unr=m_EmitBlock(t_stmt->f_block,true);
	Array<bb_stmt_CatchStmt* > t_=t_stmt->f_catches;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_stmt_CatchStmt* t_c=t_[t_2];
		t_2=t_2+1;
		m_MungDecl(t_c->f_init);
		m_Emit(String(L"}catch(",7)+m_TransType(t_c->f_init->f_type)+String(L" ",1)+t_c->f_init->f_munged+String(L"){",2));
		int t_unr2=m_EmitBlock(t_c->f_block,true);
	}
	m_Emit(String(L"}",1));
	return String();
}
void bb_cstranslator_CsTranslator::mark(){
	bb_translator_CTranslator::mark();
}
bb_list_Enumerator5::bb_list_Enumerator5(){
	f__list=0;
	f__curr=0;
}
bb_list_Enumerator5* bb_list_Enumerator5::g_new(bb_list_List4* t_list){
	gc_assign(f__list,t_list);
	gc_assign(f__curr,t_list->f__head->f__succ);
	return this;
}
bb_list_Enumerator5* bb_list_Enumerator5::g_new2(){
	return this;
}
bool bb_list_Enumerator5::m_HasNext(){
	while(f__curr->f__succ->f__pred!=f__curr){
		gc_assign(f__curr,f__curr->f__succ);
	}
	return f__curr!=f__list->f__head;
}
bb_stmt_Stmt* bb_list_Enumerator5::m_NextObject(){
	bb_stmt_Stmt* t_data=f__curr->f__data;
	gc_assign(f__curr,f__curr->f__succ);
	return t_data;
}
void bb_list_Enumerator5::mark(){
	Object::mark();
	gc_mark_q(f__list);
	gc_mark_q(f__curr);
}
bb_list_List9::bb_list_List9(){
	f__head=((new bb_list_HeadNode9)->g_new());
}
bb_list_List9* bb_list_List9::g_new(){
	return this;
}
bb_list_Node9* bb_list_List9::m_AddLast9(bb_decl_ModuleDecl* t_data){
	return (new bb_list_Node9)->g_new(f__head,f__head->f__pred,t_data);
}
bb_list_List9* bb_list_List9::g_new2(Array<bb_decl_ModuleDecl* > t_data){
	Array<bb_decl_ModuleDecl* > t_=t_data;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_decl_ModuleDecl* t_t=t_[t_2];
		t_2=t_2+1;
		m_AddLast9(t_t);
	}
	return this;
}
bool bb_list_List9::m_IsEmpty(){
	return f__head->f__succ==f__head;
}
bb_decl_ModuleDecl* bb_list_List9::m_RemoveLast(){
	bb_decl_ModuleDecl* t_data=f__head->m_PrevNode()->f__data;
	f__head->f__pred->m_Remove();
	return t_data;
}
void bb_list_List9::mark(){
	Object::mark();
	gc_mark_q(f__head);
}
bb_list_Node9::bb_list_Node9(){
	f__succ=0;
	f__pred=0;
	f__data=0;
}
bb_list_Node9* bb_list_Node9::g_new(bb_list_Node9* t_succ,bb_list_Node9* t_pred,bb_decl_ModuleDecl* t_data){
	gc_assign(f__succ,t_succ);
	gc_assign(f__pred,t_pred);
	gc_assign(f__succ->f__pred,this);
	gc_assign(f__pred->f__succ,this);
	gc_assign(f__data,t_data);
	return this;
}
bb_list_Node9* bb_list_Node9::g_new2(){
	return this;
}
bb_list_Node9* bb_list_Node9::m_GetNode(){
	return this;
}
bb_list_Node9* bb_list_Node9::m_PrevNode(){
	return f__pred->m_GetNode();
}
int bb_list_Node9::m_Remove(){
	gc_assign(f__succ->f__pred,f__pred);
	gc_assign(f__pred->f__succ,f__succ);
	return 0;
}
void bb_list_Node9::mark(){
	Object::mark();
	gc_mark_q(f__succ);
	gc_mark_q(f__pred);
	gc_mark_q(f__data);
}
bb_list_HeadNode9::bb_list_HeadNode9(){
}
bb_list_HeadNode9* bb_list_HeadNode9::g_new(){
	bb_list_Node9::g_new2();
	gc_assign(f__succ,(this));
	gc_assign(f__pred,(this));
	return this;
}
bb_list_Node9* bb_list_HeadNode9::m_GetNode(){
	return 0;
}
void bb_list_HeadNode9::mark(){
	bb_list_Node9::mark();
}
bb_expr_MemberVarExpr::bb_expr_MemberVarExpr(){
	f_expr=0;
	f_decl=0;
}
bb_expr_MemberVarExpr* bb_expr_MemberVarExpr::g_new(bb_expr_Expr* t_expr,bb_decl_VarDecl* t_decl){
	bb_expr_Expr::g_new();
	gc_assign(this->f_expr,t_expr);
	gc_assign(this->f_decl,t_decl);
	return this;
}
bb_expr_MemberVarExpr* bb_expr_MemberVarExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_MemberVarExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	if(!((f_decl->m_IsSemanted())!=0)){
		bb_config_InternalErr(String(L"Internal error",14));
	}
	gc_assign(f_exprType,f_decl->f_type);
	return (this);
}
String bb_expr_MemberVarExpr::m_ToString(){
	return String(L"MemberVarExpr(",14)+f_expr->m_ToString()+String(L",",1)+f_decl->m_ToString()+String(L")",1);
}
bool bb_expr_MemberVarExpr::m_SideEffects(){
	return f_expr->m_SideEffects();
}
bb_expr_Expr* bb_expr_MemberVarExpr::m_SemantSet(String t_op,bb_expr_Expr* t_rhs){
	return m_Semant();
}
String bb_expr_MemberVarExpr::m_Trans(){
	return bb_translator__trans->m_TransMemberVarExpr(this);
}
String bb_expr_MemberVarExpr::m_TransVar(){
	return bb_translator__trans->m_TransMemberVarExpr(this);
}
void bb_expr_MemberVarExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_expr);
	gc_mark_q(f_decl);
}
bb_expr_InvokeExpr::bb_expr_InvokeExpr(){
	f_decl=0;
	f_args=Array<bb_expr_Expr* >();
}
bb_expr_InvokeExpr* bb_expr_InvokeExpr::g_new(bb_decl_FuncDecl* t_decl,Array<bb_expr_Expr* > t_args){
	bb_expr_Expr::g_new();
	gc_assign(this->f_decl,t_decl);
	gc_assign(this->f_args,t_args);
	return this;
}
bb_expr_InvokeExpr* bb_expr_InvokeExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_InvokeExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	gc_assign(f_exprType,f_decl->f_retType);
	gc_assign(f_args,m_CastArgs(f_args,f_decl));
	return (this);
}
String bb_expr_InvokeExpr::m_ToString(){
	String t_t=String(L"InvokeExpr(",11)+f_decl->m_ToString();
	Array<bb_expr_Expr* > t_=f_args;
	int t_2=0;
	while(t_2<t_.Length()){
		bb_expr_Expr* t_arg=t_[t_2];
		t_2=t_2+1;
		t_t=t_t+(String(L",",1)+t_arg->m_ToString());
	}
	return t_t+String(L")",1);
}
String bb_expr_InvokeExpr::m_Trans(){
	return bb_translator__trans->m_TransInvokeExpr(this);
}
String bb_expr_InvokeExpr::m_TransStmt(){
	return bb_translator__trans->m_TransInvokeExpr(this);
}
void bb_expr_InvokeExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_decl);
	gc_mark_q(f_args);
}
bb_expr_StmtExpr::bb_expr_StmtExpr(){
	f_stmt=0;
	f_expr=0;
}
bb_expr_StmtExpr* bb_expr_StmtExpr::g_new(bb_stmt_Stmt* t_stmt,bb_expr_Expr* t_expr){
	bb_expr_Expr::g_new();
	gc_assign(this->f_stmt,t_stmt);
	gc_assign(this->f_expr,t_expr);
	return this;
}
bb_expr_StmtExpr* bb_expr_StmtExpr::g_new2(){
	bb_expr_Expr::g_new();
	return this;
}
bb_expr_Expr* bb_expr_StmtExpr::m_Copy(){
	return ((new bb_expr_StmtExpr)->g_new(f_stmt,m_CopyExpr(f_expr)));
}
String bb_expr_StmtExpr::m_ToString(){
	return String(L"StmtExpr(,",10)+f_expr->m_ToString()+String(L")",1);
}
bb_expr_Expr* bb_expr_StmtExpr::m_Semant(){
	if((f_exprType)!=0){
		return (this);
	}
	f_stmt->m_Semant();
	gc_assign(f_expr,f_expr->m_Semant());
	gc_assign(f_exprType,f_expr->f_exprType);
	return (this);
}
String bb_expr_StmtExpr::m_Trans(){
	return bb_translator__trans->m_TransStmtExpr(this);
}
void bb_expr_StmtExpr::mark(){
	bb_expr_Expr::mark();
	gc_mark_q(f_stmt);
	gc_mark_q(f_expr);
}
int bb_decl__loopnest;
bb_map_Map5::bb_map_Map5(){
	f_root=0;
}
bb_map_Map5* bb_map_Map5::g_new(){
	return this;
}
bb_map_Node5* bb_map_Map5::m_FindNode(String t_key){
	bb_map_Node5* t_node=f_root;
	while((t_node)!=0){
		int t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bb_decl_FuncDeclList* bb_map_Map5::m_Get(String t_key){
	bb_map_Node5* t_node=m_FindNode(t_key);
	if((t_node)!=0){
		return t_node->f_value;
	}
	return 0;
}
int bb_map_Map5::m_RotateLeft5(bb_map_Node5* t_node){
	bb_map_Node5* t_child=t_node->f_right;
	gc_assign(t_node->f_right,t_child->f_left);
	if((t_child->f_left)!=0){
		gc_assign(t_child->f_left->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_left){
			gc_assign(t_node->f_parent->f_left,t_child);
		}else{
			gc_assign(t_node->f_parent->f_right,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_left,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map5::m_RotateRight5(bb_map_Node5* t_node){
	bb_map_Node5* t_child=t_node->f_left;
	gc_assign(t_node->f_left,t_child->f_right);
	if((t_child->f_right)!=0){
		gc_assign(t_child->f_right->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_right){
			gc_assign(t_node->f_parent->f_right,t_child);
		}else{
			gc_assign(t_node->f_parent->f_left,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_right,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map5::m_InsertFixup5(bb_map_Node5* t_node){
	while(((t_node->f_parent)!=0) && t_node->f_parent->f_color==-1 && ((t_node->f_parent->f_parent)!=0)){
		if(t_node->f_parent==t_node->f_parent->f_parent->f_left){
			bb_map_Node5* t_uncle=t_node->f_parent->f_parent->f_right;
			if(((t_uncle)!=0) && t_uncle->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle->f_color=1;
				t_uncle->f_parent->f_color=-1;
				t_node=t_uncle->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_right){
					t_node=t_node->f_parent;
					m_RotateLeft5(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateRight5(t_node->f_parent->f_parent);
			}
		}else{
			bb_map_Node5* t_uncle2=t_node->f_parent->f_parent->f_left;
			if(((t_uncle2)!=0) && t_uncle2->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle2->f_color=1;
				t_uncle2->f_parent->f_color=-1;
				t_node=t_uncle2->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_left){
					t_node=t_node->f_parent;
					m_RotateRight5(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateLeft5(t_node->f_parent->f_parent);
			}
		}
	}
	f_root->f_color=1;
	return 0;
}
bool bb_map_Map5::m_Set5(String t_key,bb_decl_FuncDeclList* t_value){
	bb_map_Node5* t_node=f_root;
	bb_map_Node5* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				gc_assign(t_node->f_value,t_value);
				return false;
			}
		}
	}
	t_node=(new bb_map_Node5)->g_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->f_right,t_node);
		}else{
			gc_assign(t_parent->f_left,t_node);
		}
		m_InsertFixup5(t_node);
	}else{
		gc_assign(f_root,t_node);
	}
	return true;
}
void bb_map_Map5::mark(){
	Object::mark();
	gc_mark_q(f_root);
}
bb_map_StringMap5::bb_map_StringMap5(){
}
bb_map_StringMap5* bb_map_StringMap5::g_new(){
	bb_map_Map5::g_new();
	return this;
}
int bb_map_StringMap5::m_Compare(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void bb_map_StringMap5::mark(){
	bb_map_Map5::mark();
}
bb_map_Node5::bb_map_Node5(){
	f_key=String();
	f_right=0;
	f_left=0;
	f_value=0;
	f_color=0;
	f_parent=0;
}
bb_map_Node5* bb_map_Node5::g_new(String t_key,bb_decl_FuncDeclList* t_value,int t_color,bb_map_Node5* t_parent){
	this->f_key=t_key;
	gc_assign(this->f_value,t_value);
	this->f_color=t_color;
	gc_assign(this->f_parent,t_parent);
	return this;
}
bb_map_Node5* bb_map_Node5::g_new2(){
	return this;
}
void bb_map_Node5::mark(){
	Object::mark();
	gc_mark_q(f_right);
	gc_mark_q(f_left);
	gc_mark_q(f_value);
	gc_mark_q(f_parent);
}
bb_map_Map6::bb_map_Map6(){
	f_root=0;
}
bb_map_Map6* bb_map_Map6::g_new(){
	return this;
}
bb_map_Node6* bb_map_Map6::m_FindNode(String t_key){
	bb_map_Node6* t_node=f_root;
	while((t_node)!=0){
		int t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				return t_node;
			}
		}
	}
	return t_node;
}
bb_set_StringSet* bb_map_Map6::m_Get(String t_key){
	bb_map_Node6* t_node=m_FindNode(t_key);
	if((t_node)!=0){
		return t_node->f_value;
	}
	return 0;
}
int bb_map_Map6::m_RotateLeft6(bb_map_Node6* t_node){
	bb_map_Node6* t_child=t_node->f_right;
	gc_assign(t_node->f_right,t_child->f_left);
	if((t_child->f_left)!=0){
		gc_assign(t_child->f_left->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_left){
			gc_assign(t_node->f_parent->f_left,t_child);
		}else{
			gc_assign(t_node->f_parent->f_right,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_left,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map6::m_RotateRight6(bb_map_Node6* t_node){
	bb_map_Node6* t_child=t_node->f_left;
	gc_assign(t_node->f_left,t_child->f_right);
	if((t_child->f_right)!=0){
		gc_assign(t_child->f_right->f_parent,t_node);
	}
	gc_assign(t_child->f_parent,t_node->f_parent);
	if((t_node->f_parent)!=0){
		if(t_node==t_node->f_parent->f_right){
			gc_assign(t_node->f_parent->f_right,t_child);
		}else{
			gc_assign(t_node->f_parent->f_left,t_child);
		}
	}else{
		gc_assign(f_root,t_child);
	}
	gc_assign(t_child->f_right,t_node);
	gc_assign(t_node->f_parent,t_child);
	return 0;
}
int bb_map_Map6::m_InsertFixup6(bb_map_Node6* t_node){
	while(((t_node->f_parent)!=0) && t_node->f_parent->f_color==-1 && ((t_node->f_parent->f_parent)!=0)){
		if(t_node->f_parent==t_node->f_parent->f_parent->f_left){
			bb_map_Node6* t_uncle=t_node->f_parent->f_parent->f_right;
			if(((t_uncle)!=0) && t_uncle->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle->f_color=1;
				t_uncle->f_parent->f_color=-1;
				t_node=t_uncle->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_right){
					t_node=t_node->f_parent;
					m_RotateLeft6(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateRight6(t_node->f_parent->f_parent);
			}
		}else{
			bb_map_Node6* t_uncle2=t_node->f_parent->f_parent->f_left;
			if(((t_uncle2)!=0) && t_uncle2->f_color==-1){
				t_node->f_parent->f_color=1;
				t_uncle2->f_color=1;
				t_uncle2->f_parent->f_color=-1;
				t_node=t_uncle2->f_parent;
			}else{
				if(t_node==t_node->f_parent->f_left){
					t_node=t_node->f_parent;
					m_RotateRight6(t_node);
				}
				t_node->f_parent->f_color=1;
				t_node->f_parent->f_parent->f_color=-1;
				m_RotateLeft6(t_node->f_parent->f_parent);
			}
		}
	}
	f_root->f_color=1;
	return 0;
}
bool bb_map_Map6::m_Set6(String t_key,bb_set_StringSet* t_value){
	bb_map_Node6* t_node=f_root;
	bb_map_Node6* t_parent=0;
	int t_cmp=0;
	while((t_node)!=0){
		t_parent=t_node;
		t_cmp=m_Compare(t_key,t_node->f_key);
		if(t_cmp>0){
			t_node=t_node->f_right;
		}else{
			if(t_cmp<0){
				t_node=t_node->f_left;
			}else{
				gc_assign(t_node->f_value,t_value);
				return false;
			}
		}
	}
	t_node=(new bb_map_Node6)->g_new(t_key,t_value,-1,t_parent);
	if((t_parent)!=0){
		if(t_cmp>0){
			gc_assign(t_parent->f_right,t_node);
		}else{
			gc_assign(t_parent->f_left,t_node);
		}
		m_InsertFixup6(t_node);
	}else{
		gc_assign(f_root,t_node);
	}
	return true;
}
void bb_map_Map6::mark(){
	Object::mark();
	gc_mark_q(f_root);
}
bb_map_StringMap6::bb_map_StringMap6(){
}
bb_map_StringMap6* bb_map_StringMap6::g_new(){
	bb_map_Map6::g_new();
	return this;
}
int bb_map_StringMap6::m_Compare(String t_lhs,String t_rhs){
	return t_lhs.Compare(t_rhs);
}
void bb_map_StringMap6::mark(){
	bb_map_Map6::mark();
}
bb_map_Node6::bb_map_Node6(){
	f_key=String();
	f_right=0;
	f_left=0;
	f_value=0;
	f_color=0;
	f_parent=0;
}
bb_map_Node6* bb_map_Node6::g_new(String t_key,bb_set_StringSet* t_value,int t_color,bb_map_Node6* t_parent){
	this->f_key=t_key;
	gc_assign(this->f_value,t_value);
	this->f_color=t_color;
	gc_assign(this->f_parent,t_parent);
	return this;
}
bb_map_Node6* bb_map_Node6::g_new2(){
	return this;
}
void bb_map_Node6::mark(){
	Object::mark();
	gc_mark_q(f_right);
	gc_mark_q(f_left);
	gc_mark_q(f_value);
	gc_mark_q(f_parent);
}
bb_list_Enumerator6::bb_list_Enumerator6(){
	f__list=0;
	f__curr=0;
}
bb_list_Enumerator6* bb_list_Enumerator6::g_new(bb_list_List6* t_list){
	gc_assign(f__list,t_list);
	gc_assign(f__curr,t_list->f__head->f__succ);
	return this;
}
bb_list_Enumerator6* bb_list_Enumerator6::g_new2(){
	return this;
}
bool bb_list_Enumerator6::m_HasNext(){
	while(f__curr->f__succ->f__pred!=f__curr){
		gc_assign(f__curr,f__curr->f__succ);
	}
	return f__curr!=f__list->f__head;
}
bb_decl_GlobalDecl* bb_list_Enumerator6::m_NextObject(){
	bb_decl_GlobalDecl* t_data=f__curr->f__data;
	gc_assign(f__curr,f__curr->f__succ);
	return t_data;
}
void bb_list_Enumerator6::mark(){
	Object::mark();
	gc_mark_q(f__list);
	gc_mark_q(f__curr);
}
bb_stack_Stack8::bb_stack_Stack8(){
	f_data=Array<bb_decl_LocalDecl* >();
	f_length=0;
}
bb_stack_Stack8* bb_stack_Stack8::g_new(){
	return this;
}
bb_stack_Stack8* bb_stack_Stack8::g_new2(Array<bb_decl_LocalDecl* > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack8::m_Clear(){
	f_length=0;
	return 0;
}
bb_stack_Enumerator* bb_stack_Stack8::m_ObjectEnumerator(){
	return (new bb_stack_Enumerator)->g_new(this);
}
int bb_stack_Stack8::m_Length(){
	return f_length;
}
bb_decl_LocalDecl* bb_stack_Stack8::m_Get2(int t_index){
	return f_data[t_index];
}
int bb_stack_Stack8::m_Push22(bb_decl_LocalDecl* t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	gc_assign(f_data[f_length],t_value);
	f_length+=1;
	return 0;
}
int bb_stack_Stack8::m_Push23(Array<bb_decl_LocalDecl* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push22(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack8::m_Push24(Array<bb_decl_LocalDecl* > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push22(t_values[t_i]);
	}
	return 0;
}
void bb_stack_Stack8::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
bb_stack_Enumerator::bb_stack_Enumerator(){
	f_stack=0;
	f_index=0;
}
bb_stack_Enumerator* bb_stack_Enumerator::g_new(bb_stack_Stack8* t_stack){
	gc_assign(this->f_stack,t_stack);
	return this;
}
bb_stack_Enumerator* bb_stack_Enumerator::g_new2(){
	return this;
}
bool bb_stack_Enumerator::m_HasNext(){
	return f_index<f_stack->m_Length();
}
bb_decl_LocalDecl* bb_stack_Enumerator::m_NextObject(){
	f_index+=1;
	return f_stack->m_Get2(f_index-1);
}
void bb_stack_Enumerator::mark(){
	Object::mark();
	gc_mark_q(f_stack);
}
bb_stack_Stack9::bb_stack_Stack9(){
	f_data=Array<bb_decl_FuncDecl* >();
	f_length=0;
}
bb_stack_Stack9* bb_stack_Stack9::g_new(){
	return this;
}
bb_stack_Stack9* bb_stack_Stack9::g_new2(Array<bb_decl_FuncDecl* > t_data){
	gc_assign(this->f_data,t_data.Slice(0));
	this->f_length=t_data.Length();
	return this;
}
int bb_stack_Stack9::m_Push25(bb_decl_FuncDecl* t_value){
	if(f_length==f_data.Length()){
		gc_assign(f_data,f_data.Resize(f_length*2+10));
	}
	gc_assign(f_data[f_length],t_value);
	f_length+=1;
	return 0;
}
int bb_stack_Stack9::m_Push26(Array<bb_decl_FuncDecl* > t_values,int t_offset,int t_count){
	for(int t_i=0;t_i<t_count;t_i=t_i+1){
		m_Push25(t_values[t_offset+t_i]);
	}
	return 0;
}
int bb_stack_Stack9::m_Push27(Array<bb_decl_FuncDecl* > t_values,int t_offset){
	for(int t_i=t_offset;t_i<t_values.Length();t_i=t_i+1){
		m_Push25(t_values[t_i]);
	}
	return 0;
}
bb_stack_Enumerator2* bb_stack_Stack9::m_ObjectEnumerator(){
	return (new bb_stack_Enumerator2)->g_new(this);
}
int bb_stack_Stack9::m_Length(){
	return f_length;
}
bb_decl_FuncDecl* bb_stack_Stack9::m_Get2(int t_index){
	return f_data[t_index];
}
void bb_stack_Stack9::mark(){
	Object::mark();
	gc_mark_q(f_data);
}
bb_stack_Enumerator2::bb_stack_Enumerator2(){
	f_stack=0;
	f_index=0;
}
bb_stack_Enumerator2* bb_stack_Enumerator2::g_new(bb_stack_Stack9* t_stack){
	gc_assign(this->f_stack,t_stack);
	return this;
}
bb_stack_Enumerator2* bb_stack_Enumerator2::g_new2(){
	return this;
}
bool bb_stack_Enumerator2::m_HasNext(){
	return f_index<f_stack->m_Length();
}
bb_decl_FuncDecl* bb_stack_Enumerator2::m_NextObject(){
	f_index+=1;
	return f_stack->m_Get2(f_index-1);
}
void bb_stack_Enumerator2::mark(){
	Object::mark();
	gc_mark_q(f_stack);
}
int bbInit(){
	bb_config__cfgVars=(new bb_map_StringMap)->g_new();
	bb_target_ANDROID_PATH=String();
	bb_target_JDK_PATH=String();
	bb_target_ANT_PATH=String();
	bb_target_FLEX_PATH=String();
	bb_target_MINGW_PATH=String();
	bb_target_PSM_PATH=String();
	bb_target_MSBUILD_PATH=String();
	bb_target_HTML_PLAYER=String();
	bb_target_FLASH_PLAYER=String();
	bb_config_ENV_HOST=String();
	bb_target_OPT_MODPATH=String();
	bb_config_ENV_SAFEMODE=0;
	bb_target_OPT_CLEAN=0;
	bb_target_OPT_ACTION=0;
	bb_target_OPT_OUTPUT=String();
	bb_target_CASED_CONFIG=String(L"Debug",5);
	bb_config_ENV_CONFIG=String();
	bb_config_ENV_LANG=String();
	bb_config_ENV_TARGET=String();
	bb_config__errInfo=String();
	bb_toker_Toker::g__keywords=0;
	bb_toker_Toker::g__symbols=0;
	bb_type_Type::g_boolType=(new bb_type_BoolType)->g_new();
	bb_type_Type::g_stringType=(new bb_type_StringType)->g_new();
	bb_decl__env=0;
	bb_decl__envStack=(new bb_list_List3)->g_new();
	bb_type_Type::g_voidType=(new bb_type_VoidType)->g_new();
	bb_type_Type::g_emptyArrayType=(new bb_type_ArrayType)->g_new(bb_type_Type::g_voidType);
	bb_type_Type::g_intType=(new bb_type_IntType)->g_new();
	bb_type_Type::g_floatType=(new bb_type_FloatType)->g_new();
	bb_type_Type::g_objectType=(new bb_type_IdentType)->g_new(String(L"monkey.object",13),Array<bb_type_Type* >());
	bb_type_Type::g_throwableType=(new bb_type_IdentType)->g_new(String(L"monkey.throwable",16),Array<bb_type_Type* >());
	bb_type_Type::g_nullObjectType=(new bb_type_IdentType)->g_new(String(),Array<bb_type_Type* >());
	bb_config__errStack=(new bb_list_StringList)->g_new();
	bb_parser_FILE_EXT=String(L"monkey",6);
	bb_translator__trans=0;
	bb_decl_ClassDecl::g_nullObjectClass=(new bb_decl_ClassDecl)->g_new(String(L"{NULL}",6),1280,Array<String >(),0,Array<bb_type_IdentType* >());
	bb_decl__loopnest=0;
	return 0;
}
void gc_mark(){
	gc_mark_q(bb_config__cfgVars);
	gc_mark_q(bb_toker_Toker::g__keywords);
	gc_mark_q(bb_toker_Toker::g__symbols);
	gc_mark_q(bb_type_Type::g_boolType);
	gc_mark_q(bb_type_Type::g_stringType);
	gc_mark_q(bb_decl__env);
	gc_mark_q(bb_decl__envStack);
	gc_mark_q(bb_type_Type::g_voidType);
	gc_mark_q(bb_type_Type::g_emptyArrayType);
	gc_mark_q(bb_type_Type::g_intType);
	gc_mark_q(bb_type_Type::g_floatType);
	gc_mark_q(bb_type_Type::g_objectType);
	gc_mark_q(bb_type_Type::g_throwableType);
	gc_mark_q(bb_type_Type::g_nullObjectType);
	gc_mark_q(bb_config__errStack);
	gc_mark_q(bb_translator__trans);
	gc_mark_q(bb_decl_ClassDecl::g_nullObjectClass);
}
//${TRANSCODE_END}

FILE *fopenFile( String path,String mode ){

	if( !path.StartsWith( "monkey://" ) ){
		path=path;
	}else if( path.StartsWith( "monkey://data/" ) ){
		path=String("./data/")+path.Slice(14);
	}else if( path.StartsWith( "monkey://internal/" ) ){
		path=String("./internal/")+path.Slice(18);
	}else if( path.StartsWith( "monkey://external/" ) ){
		path=String("./external/")+path.Slice(18);
	}else{
		return 0;
	}

#if _WIN32
	return _wfopen( path.ToCString<wchar_t>(),mode.ToCString<wchar_t>() );
#else
	return fopen( path.ToCString<char>(),mode.ToCString<char>() );
#endif
}

int main( int argc,const char **argv ){

	try{
	
		bb_std_main( argc,argv );
		
	}catch( ThrowableObject *ex ){
	
		Print( "Monkey Runtime Error : Uncaught Monkey Exception" );
	
	}catch( const char *err ){
	
	}
}
