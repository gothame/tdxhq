// Quotation.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "HQTEST.h"
#include "Quotation.h"
#include "Config.h"
#include "..\TDXHQ\TDXHQ.h"

#include <iostream>
#include <boost/date_time.hpp>
#include "tests/functions.h"

extern CConfig cfg;


// CQuotation

IMPLEMENT_DYNCREATE(CQuotation, CWinThread)

CQuotation::CQuotation()
{}

CQuotation::CQuotation(DWORD dwID, STOCKCODE * pStockCode, int iStockNum  )
{
	m_dwID = dwID;
	m_iStockNum = iStockNum;
	ZeroMemory( & m_stockcode[0] , sizeof( STOCKCODE ) * 400 );
	memcpy( & m_stockcode[0] ,pStockCode , sizeof( STOCKCODE) * iStockNum );

	redis =new redis::client("192.168.99.100",32770,0,"");
}

CQuotation::~CQuotation()
{
}

BOOL CQuotation::InitInstance()
{
	g_bGetQuotation = FALSE;
	conID = -1;
	// TODO: �ڴ�ִ���������̳߳�ʼ��
	return TRUE;
}

int CQuotation::ExitInstance()
{
	// TODO: �ڴ�ִ���������߳�����
	return CWinThread::ExitInstance();
}

BEGIN_MESSAGE_MAP(CQuotation, CWinThread)
END_MESSAGE_MAP()


// CQuotation ��Ϣ�������

int CQuotation::Run()
{
	// TODO: �ڴ����ר�ô����/����û���
	_AFX_THREAD_STATE* pState = AfxGetThreadState();

	while( 1 )
	{
		//======================��Ϣѭ��
		if( ::PeekMessage(&(pState->m_msgCur), NULL, NULL, NULL, PM_NOREMOVE))
		{
			if (!PumpMessage())
			{
				return ExitInstance();
			}
		}
		//======================��Ϣѭ��

		if( g_bGetQuotation )
		{
			CTime t_start = CTime::GetCurrentTime();
			while( conID < 0 )
			{	
				conID = TDXHQ_Connect() ;
				CString csMsg;
				csMsg.Format("%.2d  conID:%d\r\n",m_dwID,conID);
				TRACE( csMsg);
			};

			for( int i = 0 ; i < m_iStockNum ; i++ )
			{
				int market = 0;
				CString csDbPath = cfg.m_csDataPath.GetBuffer();
				if( m_stockcode[i].market[0] == '0' )
				{
					//SZ
					csDbPath += "\\SZ\\";
					csDbPath += m_stockcode[i].code;
					csDbPath += ".db";
					market = 0;
				}
				else if ( m_stockcode[i].market[0] == '1' )
				{
					//SH
					csDbPath += "\\SH\\";
					csDbPath += m_stockcode[i].code;
					csDbPath += ".db";
					market = 1;
				}

				//TRACE( csDbPath );
				
				sqlite3 * db =NULL;
				int result = sqlite3_open_v2( csDbPath ,& db, SQLITE_OPEN_READWRITE  | SQLITE_OPEN_NOMUTEX, NULL);

				//ȡʵʱ5��
				//TODO
				//ʵʱ5��
				////==========================================================////
				redis->select( 0 );
				redis->publish("data","message");
				redis->select( 1 );
				redis->publish("kline5","message9");
				////=========================================================////

				//30����
				WORD count = 2;
				char szResult[256 * 800];
				ZeroMemory( szResult , sizeof( szResult ) );
				TDXHQ_GetSecurityBars( conID, 2,market,m_stockcode[i].code , 0 ,count ,szResult );
				CString csRes = szResult;
				CString csLine;
				int iRow = 1 ;
				do 
				{
					AfxExtractSubString(csLine, szResult, iRow , '\n');
					iRow ++;
					if( csLine.GetLength() == 0 )
						break;
					else
					{
						CString csSQL = "REPLACE INTO Min30(DateTime,Open,Close,Hight,Low,Vol,amount) Values(";
						csSQL += csLine;
						csSQL += ")";
						char * szErrMsg = NULL;
						result = sqlite3_exec( db,csSQL , NULL, NULL, & szErrMsg  );
						if( result )
							TRACE("Result:%d %s\n",result,szErrMsg );
					}
				} while ( 1 );

				//5����
				count = 3;
				ZeroMemory( szResult , sizeof( szResult ) );
				TDXHQ_GetSecurityBars( conID,0,market,m_stockcode[i].code , 0 ,count ,szResult );
				csRes = szResult;
				iRow = 1 ;
				do 
				{
					AfxExtractSubString(csLine, szResult, iRow , '\n');
					iRow ++;
					if( csLine.GetLength() == 0 )
						break;
					else
					{
						CString csSQL = "REPLACE INTO Min5(DateTime,Open,Close,Hight,Low,Vol,TureOver) Values(";
						csSQL += csLine;
						csSQL += ")";
						char * szErrMsg = NULL;
						result = sqlite3_exec( db,csSQL , NULL, NULL, & szErrMsg  );
						if( result )
							TRACE("Result:%d %s\n",result,szErrMsg );
					}
				} while ( 1 );



				//Day1
				/*
				count = 2;
				ZeroMemory( szResult , sizeof( szResult ) );
				TDXHQ_GetSecurityBars( conID,4,market,m_stockcode[i].code , 0 ,count ,szResult );
				csRes = szResult;
				iRow = 1 ;
				do 
				{
					AfxExtractSubString(csLine, szResult, iRow , '\n');
					iRow ++;
					if( csLine.GetLength() == 0 )
						break;
					else
					{
						CString csSQL = "REPLACE INTO Day1(DateTime,Open,Close,Hight,Low,Vol,TureOver) Values(";
						csSQL += csLine;
						csSQL += ")";
						char * szErrMsg = NULL;
						result = sqlite3_exec( db,csSQL , NULL, NULL, & szErrMsg  );
						if( result )
							TRACE("Result:%d %s\n",result,szErrMsg );
					}
				} while ( 1 );
				*/

			
				sqlite3_close( db );
			}
			//TDXHQ_DisConnect( conID );
			CTime t_end = CTime::GetCurrentTime();
			CTimeSpan t_cost = t_end - t_start;
			CString cstmp;
			cstmp.Format("%.2d  ",m_dwID );
			cstmp += t_cost.Format("%M:%S\n") ;
			TRACE( cstmp );
			//conID = -1;
			g_bGetQuotation = FALSE;
		}
		else
		{
			Sleep(  900 );
		}



	}

	return CWinThread::Run();
}
