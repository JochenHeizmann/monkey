
//Note: OS_CHAR and OS_STR are declared in os file os.cpp

int info_width;
int info_height;

int get_info_png( String path ){

	if( FILE *f=fopen( OS_STR(path),OS_STR("rb") ) ){
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

	if( FILE *f=fopen( OS_STR(path),OS_STR("rb") ) ){
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

	if( FILE *f=fopen( OS_STR(path),OS_STR("rb") ) ){
		unsigned char data[32];
		
		if( fread( data,1,11,f )==11 && 
			data[0]==0xff && data[1]==0xd8 && data[2]==0xff && data[3]==0xe0 &&
			data[6]=='J' && data[7]=='F' && data[8]=='I' && data[9]=='F' && data[10]==0 ){
			
			int i=2,block_length=(data[4]<<8)|data[5];
			
			for(;;){
				i+=block_length+2;
				
				if( fseek( f,i,SEEK_SET )<0 ) break;
				
				if( fread( data,1,4,f )!=4 || data[0]!=0xff ) break;
				
				if( data[1]==0xc0 ){
					if( fread( data,1,5,f )!=5 ) break;
					info_width=(data[1]<<8)|data[2];
					info_height=(data[3]<<8)|data[4];
					fclose( f );
					return 0;
				}
				block_length=(data[2]<<8)|data[3];
			}
		}
		fclose( f );
	}
	return -1;
}
