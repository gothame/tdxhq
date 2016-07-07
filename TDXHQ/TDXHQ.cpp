// TDXHQ.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "TDXHQ.h"
#include "ByteBuffer.h"
#include "zlib.h"
#include "math.h"
#include "afxmt.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define CONNECTNUM 1000

//�������IP���� TODO
#pragma data_seg("shared")  
DWORD nIP;
DWORD nCurentIP;

DWORD nConnectID;
SOCKET skHQ[CONNECTNUM];

CMutex * pmtx;

DWORD dwAutoConnect;
sockaddr_in IPS[100];//100��IP��λ��

#pragma comment(linker,"/SECTION:shared,RWS")  

TDXHQ_API int TDXHQ_Init(DWORD dwChoiceFast,DWORD dwAutoCon)
{
	WORD ver;
	WSADATA data;
	ver =MAKEWORD( 2, 0 );
	WSAStartup( ver, &data );

	dwAutoConnect = dwAutoCon;
	HMODULE module = GetModuleHandle( "TDXHQ.dll"); 
	CHAR szBuff[MAX_PATH]; 
	GetModuleFileName(module, szBuff, sizeof(szBuff)); 
	CString csFileName = szBuff;
	int nPosSplit = csFileName.ReverseFind( ('\\') );

	CString csPath = csFileName.Left(nPosSplit);
	
	CString csCfgFileName = csPath + "\\TDXHQ.ini";

	CStdioFile cfCfgFile;
	if( !cfCfgFile.Open( csCfgFileName, CFile::modeRead | CFile::shareDenyNone ) )
	{
		CString csMsg;
		csMsg.Format("���ܴ������ļ�:");
		csMsg += csCfgFileName;
		AfxMessageBox( csMsg );

		return TDXHQ_ERROR_COFG;
	}
	else
	{
		CString csIPline;
		int i = 0;
		while( cfCfgFile.ReadString( csIPline ) )
		{	
			CString csIP,csPort;
			AfxExtractSubString(csIP, csIPline, 0, ':');
			AfxExtractSubString(csPort, csIPline, 1, ':');
			WORD port = atoi( csPort );

			IPS[i].sin_family = AF_INET;
			IPS[i].sin_port = htons( port );
			IPS[i].sin_addr.S_un.S_addr = inet_addr( csIP );
			i++ ;
		}
		nCurentIP = 0;
		nConnectID = 0;
		nIP = i ;
		pmtx = new CMutex(FALSE, "MutexQuotation");

		for( int c = 0 ; c<CONNECTNUM ; c ++ )
			skHQ[c] = INVALID_SOCKET ;
	}

	if( dwChoiceFast )
	{
		return TDXHQ_ERROR_OK;
	}
	else
	{//���浽DLL�ڴ���
		return TDXHQ_ERROR_OK;
	}
}

TDXHQ_API int TDXHQ_Connect( )
{
	pmtx->Lock();
	int ret = TDXHQ_ERROR_CONT;

	for( int i =0 ; i< CONNECTNUM ; i++)
	{
		if( skHQ[i] == INVALID_SOCKET )
		{
			nConnectID = i;
			break;
		}
	}
	

	if( skHQ[nConnectID] == INVALID_SOCKET )
	{
		skHQ[nConnectID]  = socket( AF_INET, SOCK_STREAM , 0 );
	}

	if( nIP > 0 )
	{
		nCurentIP = nCurentIP % nIP;
		nCurentIP ++;
	}
	else
		AfxMessageBox("�����ļ���IP");

	if( INVALID_SOCKET == connect( skHQ[nConnectID],(sockaddr *) (& IPS[nCurentIP]) , sizeof( sockaddr_in ) ) )
	{
		//TRACE( "����ʧ��\r\n");
		ret = TDXHQ_ERROR_CONT;
	}
	else 
	{
		//TRACE("���ӳɹ�\r\n");
		ret = nConnectID;
		//nConnectID ++;
	}
	pmtx->Unlock();
	return ret;
}

TDXHQ_API int TDXHQ_DisConnect(int conID )
{
	pmtx->Lock();
	if( skHQ[conID] != INVALID_SOCKET )
	{
		shutdown( skHQ[conID] , SD_BOTH );
		closesocket( skHQ[conID] );
		skHQ[conID] = INVALID_SOCKET;
	}
	pmtx->Unlock();
	return TDXHQ_ERROR_OK;
}

int getPrice( char * pData , int & pos )
{
	BYTE bdata;
	int data = 0;
	int posByte = 6;
	bdata = * ((BYTE *)( &pData[pos]) );
	data = bdata & 0x3f;
	bool bSign;
	if( bdata & 0x40)
		bSign = 1;
	else
		bSign = 0;
	if( bdata & 0x80 )
	{
		do
		{
			pos ++;
			bdata = * ((BYTE *)( &pData[pos]) );
			data += (bdata & 0x7f) << posByte ;
			posByte += 7;				
		}
		while( bdata & 0x80 );
	}
	pos ++;

	if( bSign )
		data = -data;

	return data;
}


float GetVolume(DWORD ivol)
{
	//ivol = 0x00000000;
	char logpoint = ivol >> (8*3);
	unsigned char hheax = ivol >> (8*3);				//[3]
	unsigned char hleax = ( ivol >> (8*2) )&0xff;	//[2]
	unsigned char lheax = (ivol >> 8)&0xff;			//[1]
	unsigned char lleax = ivol & 0xff;				//[0]

	double dbl_1 = 1.0;
	double dbl_2 = 2.0;
	double dbl_128 = 128.0;

	int dwEcx = logpoint * 2  - 0x7f;
	int dwEdx = logpoint * 2  - 0x86;
	int dwEsi = logpoint * 2  - 0x8e;
	int dwEax = logpoint * 2  - 0x96;

	int tmpEax;
	if( dwEcx < 0 )
		tmpEax = - dwEcx;
	else
		tmpEax = dwEcx;

	double dbl_xmm6 = 0;
	dbl_xmm6 = pow( 2.0, tmpEax );
	if( dwEcx < 0 )
		dbl_xmm6 = 1.0/dbl_xmm6;

	double dbl_xmm4 = 0;
	if( hleax > 0x80 )
	{
		double tmpdbl_xmm3 = 0;
		double tmpdbl_xmm1 = 0;
		int dwtmpeax = dwEdx + 1;
		tmpdbl_xmm3 = pow( 2.0, dwtmpeax );

		double dbl_xmm0 ;
		dbl_xmm0 = pow( 2.0, dwEdx ) * 128.0;

		dbl_xmm0 += (hleax & 0x7f)* tmpdbl_xmm3 ;

 		dbl_xmm4 = dbl_xmm0;
	}
	else
	{
		double dbl_xmm0 = 0;
		if( dwEdx >= 0)
			 dbl_xmm0 = pow( 2.0 ,dwEdx ) * hleax;
		else
			dbl_xmm0 = (1/pow( 2.0 ,dwEdx ) )* hleax;

		dbl_xmm4 = dbl_xmm0;
	}

	double dbl_xmm3 = pow( 2.0, dwEsi) * lheax;
	double dbl_xmm1 = pow(2.0,dwEax) * lleax;
	if( hleax & 0x80 )
	{
		dbl_xmm3 *= 2.0;
		dbl_xmm1 *= 2.0;
	}
	double dbl_ret = dbl_xmm6 + dbl_xmm4 + dbl_xmm3 + dbl_xmm1;
	return dbl_ret;
}

/// ��ȡ֤ȯָ����Χ�ĵ�K������
///Category:K������, 0->5����K��    1->15����K��    2->30����K��  3->1СʱK��    4->��K��  5->��K��  6->��K��  7->1����  8->1����K��  9->��K��  10->��K��  11->��K��
///Market:�г�����,   0->����     1->�Ϻ�
///Zqdm:֤ȯ����
///Start:��Χ�Ŀ�ʼλ��,���һ��K��λ����0, ǰһ����1, ��������
///Count:��Χ�Ĵ�С��APIִ��ǰ,��ʾ�û�Ҫ�����K����Ŀ, APIִ�к�,������ʵ�ʷ��ص�K����Ŀ, ���ֵ800
///Result:��APIִ�з��غ�Result�ڱ����˷��صĲ�ѯ����, ��ʽΪ������ݣ�������֮��ͨ��\n�ַ��ָ������֮��ͨ��\t�ָ���һ��Ҫ����1024*1024�ֽڵĿռ䡣����ʱΪ���ַ�����
///ErrInfo:��APIִ�з��غ�������������˴�����Ϣ˵����һ��Ҫ����256�ֽڵĿռ䡣û����ʱΪ���ַ�����
/// returns:�ɹ�����true, ʧ�ܷ���false
TDXHQ_API int TDXHQ_GetSecurityBars(int connectID , WORD Category, WORD Market, char* Zqdm, WORD Start, WORD & Count, char szResult[] )
{
	//CString csMsgBox;
	//csMsgBox.Format("1:%d,2:%d,3:%s,4:%d,5:%d,6:0x%x",Category,Market,Zqdm,Start,Count , (DWORD)szResult);
	//AfxMessageBox( csMsgBox );

	CByteBuffer bfSend;
	bfSend.append( UINT16( 0x10c ) );
	bfSend.append( UINT32( 0x01016408) );
	bfSend.append( UINT16( 0x12 ) );
	bfSend.append( UINT16( 0x12 ) );
	bfSend.append( UINT16( 0x529 ) );
	bfSend.append( UINT16( Market ) );
	bfSend.append( Zqdm, strlen( Zqdm ) );
	bfSend.append( UINT16( Category ) );
	bfSend.append( UINT32( 1 ) );
	bfSend.append( UINT16( Count ) );

	if( skHQ[connectID] != INVALID_SOCKET)
	{
		int nSend = bfSend.size();
		char * datatosend = new char[ nSend ];
		bfSend.read( (UINT8 *) datatosend , nSend );
		int nSended = send( skHQ[connectID] , datatosend , nSend , 0 );
		delete [] datatosend;
		if( nSended != nSend )
		{//����������
			TRACE("������������");
			return TDXHQ_ERROR_SEND;
		}
		else
		{
			char recvHead[0x10];
			int nRecved = recv( skHQ[connectID] , recvHead , sizeof(recvHead) , 0 );
			if( nRecved == 0x10 )
			{
				CByteBuffer bfRecv;
				bfRecv.append( recvHead, sizeof( recvHead ) );
				UINT32 ignor ;
				bfRecv >> ignor;
				bfRecv >> ignor;
				bfRecv >> ignor;

				UINT16 wZipedSize;
				bfRecv >> wZipedSize;
				UINT16 wUzipedSize;
				bfRecv >> wUzipedSize;

				int iRecvedBody;
				CByteBuffer bfRecvedBody;
				char * ZipedData = new char[ wZipedSize ];
				do 
				{
					iRecvedBody = recv( skHQ[connectID] , ZipedData , wZipedSize , 0 ) ;
					bfRecvedBody.append( ZipedData, iRecvedBody );
					//TRACE("����:%d,��������С:%d,����С:%d\r\n",iRecvedBody,bfRecvedBody.size() ,wZipedSize );
				
				} 
				while ( (iRecvedBody != 0) && (bfRecvedBody.size() != wZipedSize)  );


				if( 0 == iRecvedBody)
				{
					TRACE("����������ʧ�ܷ������Ͽ�����" );
					return TDXHQ_ERROR_RECV;
				}

				bfRecvedBody.rpos( 0 );
				bfRecvedBody.read( (UINT8 *) ZipedData , wZipedSize );
								
				char * UzipedData = new char[ wUzipedSize  ];
				ZeroMemory( UzipedData , wUzipedSize );
				if( wZipedSize == wUzipedSize )
				{
					//TRACE("����Ҫ��ѹ");
					memcpy( UzipedData , ZipedData , wUzipedSize );
					//bfRecvedBody.read( (UINT8 *)UzipedData , wUzipedSize );
					//����Ҫ��ѹ
				}
				else
				{
					//��Ҫ��ѹ
					//TRACE("��Ҫ��ѹ");
					DWORD dwUzipedLen = wUzipedSize;
					uncompress( (Bytef *)( UzipedData ) , & dwUzipedLen ,( Bytef *)( ZipedData) , wZipedSize );
					if( wUzipedSize != dwUzipedLen)
						AfxMessageBox( "��ѹ����");
				}
				delete [] ZipedData;

				WORD resCount = *( (WORD *)UzipedData );
				Count = resCount;
				CString csResult = "datetime,open,close,hight,low,volume,amount\n";
				//TRACE("K������ %d",Count );
				int pos = 2;
				int iPreClose = 0;
				for( int i = 0 ; i< Count; i++ )
				{
					int year=0,month=0,day=0,hour=15,minute=0;
					if( ( Category < 4) || Category==7 || Category==8 )
					{
						//��������
						WORD zipday =* ((WORD *)( &UzipedData[pos]) );
						year = (zipday >> 11) + 2004;
						month = (zipday % 2048) / 100;
						day = (zipday % 2048) % 100;
						pos += 2;

						//����ʱ��
						WORD tminutes =* ((WORD *)( &UzipedData[pos]) );
						hour = tminutes / 60;
						minute = tminutes % 60;
						pos +=2;
					}
					else
					{
						DWORD zipday =* ((DWORD *)( &UzipedData[pos]) );
						pos +=4;

						year = zipday / 10000;
						month = (zipday % 10000) /100;
						day = zipday % 100;
					}

					//��������
					int iPriceOpen =iPreClose + getPrice( UzipedData , pos );
					int iPriceClose =iPriceOpen +  getPrice( UzipedData , pos );
					
					int iPriceHight =iPriceOpen + getPrice( UzipedData , pos );
					int iPriceLow = iPriceOpen + getPrice( UzipedData , pos );
					iPreClose = iPriceClose;
					int Volume = getPrice( UzipedData , pos );
					if( Volume == 0 )
						Volume = 1;

					int iTurnOver = * ((DWORD *)( &UzipedData[pos]) );
					double dbvol = GetVolume( iTurnOver );
					if( dbvol < 0.0000001 )
						dbvol = iPriceClose/1000.0;
					pos += 4;

					CString csLine ;
					csLine.Format("\"%4d-%.2d-%.2d %.2d:%.2d:%.2d\",%.3f,%.3f,%.3f,%.3f,%d,%.3f\n",year,month,day,hour,minute,0,iPriceOpen/1000.0,iPriceClose/1000.0,iPriceHight/1000.0,iPriceLow/1000.0,Volume,dbvol);
					//TRACE( csLine );
					csResult += csLine;
				}

				memcpy( szResult , csResult.GetBuffer() , csResult.GetLength() );

				return TDXHQ_ERROR_OK;
			}
			else
			{
				TRACE("����ͷʧ��");
				return TDXHQ_ERROR_RECV;
			}
		}	
	}
	else
	{
		TRACE("��Ч��socket");
		return TDXHQ_ERROR_CONT;
	}
}

/*

B1 CB 74 00 0C 01 20 63 00 00 3E 05 (55 00) (55 00)   
B1 CB 01 00 00 30 30 30 30 30 31 
0E 0B //2830
BD 0F 
07   
06 
07 
42 
93 FD A6 0E //ʱ��
FD 0F 
9E D3 1F 
A8 33 
97 C3 7C 4D 
B4 EB 14 
AA E7 0A 
00 
9C 0A 
41 
00 
8F 22 
A1   
1B 
42 
01 
91 
97 01 
9E 24 
43 
02 
A5 9A 01 
A4 26 
44   
03 
9F 1F 
B6 19 
45 
04 
A0 1C 
82 23 

02 //����
01 //���� 
00 //���� 
0A //���� 
11 //����  
03 //���� 
00 00 //����
0E 0B //2830                                  
                               
								 
�г�	����	��Ծ��	�ּ�		����		����		���		���		ʱ��	����	����	����	�ܽ��	����	����	����	����	��һ��	��һ��	��һ��	��һ��	�����	������	�����	������	������	������	������	������	���ļ�	���ļ�	������	������	�����	�����	������	������	����	����	����	����	����	����	��Ծ��
0		000001	2830	10.210000	10.280000	10.270000	10.280000	10.190000	14999379	-1021	259294	3304	265042288.000000	170740	88554	0	668	10.200000	10.210000	2191	1761	10.190000	10.220000	9681	2334	10.180000	10.230000	9893	2468	10.170000	10.240000	2015	1654	10.160000	10.250000	1824	2242	258	0	10	17	3	0.000000	2830
*/
TDXHQ_API int TdxHq_GetSecurityQuotes(int connectID,WORD Market, char* Zqdm, WORD& Count, char szResult[])//��ȡ�̿��嵵����
{
	/*
	0C 01 
	20 63 00 02 
	13 00 
	13 00 
	3E 05 05 00 
	00 00 00 00 
	00 00 
	01 00 
	00 �г�����0���ڣ�1�Ϻ���
	30 30 30 30 30 31            000001
	*/
	CByteBuffer bfSend;
	bfSend.append( UINT16( 0x10c ) );
	bfSend.append( UINT32( 0x02006320) );
	bfSend.append( UINT16( 0x13 ) );
	bfSend.append( UINT16( 0x13 ) );
	bfSend.append( UINT16( 0x5053e ) );
	bfSend.append( UINT32( 0 ));
	bfSend.append( UINT16( 0 ) );
	bfSend.append( UINT16( 1 ) );
	bfSend.append( UINT8( Market ) );
	bfSend.append( Zqdm, strlen( Zqdm ) );

	if( skHQ[connectID] != INVALID_SOCKET)
	{
		int nSend = bfSend.size();
		char * datatosend = new char[ nSend ];
		bfSend.read( (UINT8 *) datatosend , nSend );
		int nSended = send( skHQ[connectID] , datatosend , nSend , 0 );
		delete [] datatosend;
		if( nSended != nSend )
		{//����������
			TRACE("������������");
			return TDXHQ_ERROR_SEND;
		}
		else
		{
			char recvHead[0x10];
			int nRecved = recv( skHQ[connectID] , recvHead , sizeof(recvHead) , 0 );
			if( nRecved == 0x10 )
			{
				CByteBuffer bfRecv;
				bfRecv.append( recvHead, sizeof( recvHead ) );
				UINT32 ignor ;
				bfRecv >> ignor;
				bfRecv >> ignor;
				bfRecv >> ignor;

				UINT16 wZipedSize;
				bfRecv >> wZipedSize;
				UINT16 wUzipedSize;
				bfRecv >> wUzipedSize;

				int iRecvedBody;
				CByteBuffer bfRecvedBody;
				char * ZipedData = new char[ wZipedSize ];
				do 
				{
					iRecvedBody = recv( skHQ[connectID] , ZipedData , wZipedSize , 0 ) ;
					bfRecvedBody.append( ZipedData, iRecvedBody );
					//TRACE("����:%d,��������С:%d,����С:%d\r\n",iRecvedBody,bfRecvedBody.size() ,wZipedSize );

				} 
				while ( (iRecvedBody != 0) && (bfRecvedBody.size() != wZipedSize)  );


				if( 0 == iRecvedBody)
				{
					TRACE("����������ʧ�ܷ������Ͽ�����" );
					return TDXHQ_ERROR_RECV;
				}

				bfRecvedBody.rpos( 0 );
				bfRecvedBody.read( (UINT8 *) ZipedData , wZipedSize );

				char * UzipedData = new char[ wUzipedSize  ];
				ZeroMemory( UzipedData , wUzipedSize );
				if( wZipedSize == wUzipedSize )
				{
					//TRACE("����Ҫ��ѹ");
					memcpy( UzipedData , ZipedData , wUzipedSize );
					//bfRecvedBody.read( (UINT8 *)UzipedData , wUzipedSize );
					//����Ҫ��ѹ
				}
				else
				{
					//��Ҫ��ѹ
					//TRACE("��Ҫ��ѹ");
					DWORD dwUzipedLen = wUzipedSize;
					uncompress( (Bytef *)( UzipedData ) , & dwUzipedLen ,( Bytef *)( ZipedData) , wZipedSize );
					if( wUzipedSize != dwUzipedLen)
						AfxMessageBox( "��ѹ����");
				}
				delete [] ZipedData;

				WORD resCount = *( (WORD *)UzipedData );
				Count = resCount;
				CString csResult = "datetime,open,close,hight,low,volume,amount\n";
				//TRACE("K������ %d",Count );
				int pos = 2;
				int iPreClose = 0;
				for( int i = 0 ; i< Count; i++ )
				{
					int year=0,month=0,day=0,hour=15,minute=0;
					int Category = 4; //���������ʱ���� @markqiu
					if( ( Category < 4) || Category==7 || Category==8 )
					{
						//��������
						WORD zipday =* ((WORD *)( &UzipedData[pos]) );
						year = (zipday >> 11) + 2004;
						month = (zipday % 2048) / 100;
						day = (zipday % 2048) % 100;
						pos += 2;

						//����ʱ��
						WORD tminutes =* ((WORD *)( &UzipedData[pos]) );
						hour = tminutes / 60;
						minute = tminutes % 60;
						pos +=2;
					}
					else
					{
						DWORD zipday =* ((DWORD *)( &UzipedData[pos]) );
						pos +=4;

						year = zipday / 10000;
						month = (zipday % 10000) /100;
						day = zipday % 100;
					}

					//��������
					int iPriceOpen =iPreClose + getPrice( UzipedData , pos );
					int iPriceClose =iPriceOpen +  getPrice( UzipedData , pos );

					int iPriceHight =iPriceOpen + getPrice( UzipedData , pos );
					int iPriceLow = iPriceOpen + getPrice( UzipedData , pos );
					iPreClose = iPriceClose;
					int Volume = getPrice( UzipedData , pos );
					if( Volume == 0 )
						Volume = 1;

					int iTurnOver = * ((DWORD *)( &UzipedData[pos]) );
					double dbvol = GetVolume( iTurnOver );
					if( dbvol < 0.0000001 )
						dbvol = iPriceClose/1000.0;
					pos += 4;

					CString csLine ;
					csLine.Format("\"%4d-%.2d-%.2d %.2d:%.2d:%.2d\",%.3f,%.3f,%.3f,%.3f,%d,%.3f\n",year,month,day,hour,minute,0,iPriceOpen/1000.0,iPriceClose/1000.0,iPriceHight/1000.0,iPriceLow/1000.0,Volume,dbvol);
					//TRACE( csLine );
					csResult += csLine;
				}

				memcpy( szResult , csResult.GetBuffer() , csResult.GetLength() );

				return TDXHQ_ERROR_OK;
			}
			else
			{
				TRACE("����ͷʧ��");
				return TDXHQ_ERROR_RECV;
			}
		}	
	}
	else
	{
		TRACE("��Ч��socket");
		return TDXHQ_ERROR_CONT;
	}
}

