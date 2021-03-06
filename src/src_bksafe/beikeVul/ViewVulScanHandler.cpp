#include "StdAfx.h"
#include "ViewVulScanHandler.h"
#include "DlgMain.h"
#include "BeikeVulfixUtils.h"
#include "BeikeVulfixEngine.h"

bool NeedRepair( LPTUpdateItem pItem )
{
	return !pItem->isIgnored && pItem->nWarnLevel>=0;
}

bool NeedRepair( LPTVulSoft pItem )
{
	return pItem->state.comState!=COM_ALL_DISABLED;
}

static int CountValidVuls(const CSimpleArray<LPTUpdateItem>& arr)
{
	int count = 0;
	for(int i=0; i<arr.GetSize(); ++i)
	{
		LPTUpdateItem pItem = arr[i];
		if( NeedRepair(pItem) )
			++ count;
	}
	return count;
}

static int CountValidVuls(const CSimpleArray<LPTVulSoft>& arr)
{
	int count = 0;
	for(int i=0; i<arr.GetSize(); ++i)
	{
		LPTVulSoft pItem = arr[i];
		if( NeedRepair(pItem) )
			++ count;
	}
	return count;
};

CViewVulScanHandler::CViewVulScanHandler( CDlgMain &mainDlg ) : CBaseViewHandler<CDlgMain>(mainDlg)
{
	m_nPos = 0;
	m_nDashPage = 200;
	m_nItemOfRightInfo = -1;
}

BOOL CViewVulScanHandler::Init( )
{
	m_wndListCtrl.Create( 
		GetViewHWND(), NULL, NULL, 
		//WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_OWNERDRAWFIXED | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_NOCOLUMNHEADER, 
		WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL , 
		0, 9001, NULL);
	
	m_wndListCtrl.InsertColumn(0, _T("补丁名称"), LVCFMT_LEFT, 100);
	m_wndListCtrl.InsertColumn(1, _T("补丁描述"), LVCFMT_LEFT, 350);
	m_wndListCtrl.InsertColumn(2, _T("修复进度"), LVCFMT_CENTER, 120);
	m_wndListCtrl.InsertColumn(3, _T("修复状态"), LVCFMT_CENTER, 100);
	
	// 
	m_ctlRichEdit.FirstInitialize( GetViewHWND(), 24150 );
	m_ctlRichEdit.SetBackgroundColor( RGB(240, 244, 250) );
	
	return TRUE;
}

BOOL CViewVulScanHandler::OnViewActived( INT nTabId, BOOL bActive )
{
	return TRUE;
}

void CViewVulScanHandler::StartScan()
{
	if( theEngine->ScanVul( (HWND)-1 ) )
	{
		m_RefWin.SetNoDisturb(TRUE, FALSE, 20080);
		ShowDashPage( 200 );
		_SetScanProgress(0);
	}
}

int CViewVulScanHandler::_AppendVulItem( T_VulListItemData * pVulItem )
{
	CString strTitle;
	CString strFileSize;
	strTitle.Format(_T("KB%d"), pVulItem->nID);
	FormatSizeString(pVulItem->nFileSize, strFileSize);

	int nItem = m_wndListCtrl.Append(strTitle);
	m_wndListCtrl.AppendSubItem(nItem, pVulItem->strDesc);
	m_wndListCtrl.AppendSubItem(nItem, strFileSize);
	m_wndListCtrl.AppendSubItem(nItem, _T("未修复"));
	m_wndListCtrl.SetSubItemColor(nItem, 3, red);
	m_wndListCtrl.SetItemData(nItem, (DWORD_PTR)pVulItem);	
	return nItem;
}

LRESULT CViewVulScanHandler::OnVulFixEventHandle( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	int evt = uMsg - WMH_VULFIX_BASE;
	if(evt==EVulfix_ScanBegin||evt==EVulfix_ScanProgress)
	{
		if(evt==EVulfix_ScanBegin)
		{
			if(wParam==VTYPE_WINDOWS)
				m_nScanState = 0;
			else 
				++m_nScanState;
			m_nTotalItem = lParam;
			m_nCurrentItem = 0;
		}
		else //EVulfix_ScanProgress:
		{
			if(lParam>(m_nCurrentItem+50))
			{
				m_nCurrentItem = lParam;
				int nPos = m_nScanState*100 + (m_nCurrentItem*100)/m_nTotalItem;
				_SetScanProgress(nPos);
			}
		}
	}
	else
	{
		int nKbId = wParam;
		int nIndex = FindListItem( nKbId );
		if(nIndex==-1)
			return 0; 

		int nDownloadPercent = 0;
		int nSubitem = -1,nSubitem2 = -1;
		CString strTitle, strTitle2;
		COLORREF clr = black, clr2=black;
		switch (evt)
		{
		case EVulfix_DownloadBegin:
		case EVulfix_DownloadProcess:
		case EVulfix_DownloadEnd:
		case EVulfix_DownloadError:
			nSubitem = 2;
			if( EVulfix_DownloadProcess==evt )
			{
				T_VulListItemData *pItemData = (T_VulListItemData*)m_wndListCtrl.GetItemData( nIndex );
				ATLASSERT(pItemData);
				if(pItemData)
				{
					CString strFileSize, strDownloadedSize;
					FormatSizeString(lParam, strDownloadedSize);
					FormatSizeString(pItemData->nFileSize, strFileSize);
					strTitle.Format(_T("%s/%s"), strDownloadedSize, strFileSize);

					nSubitem2 = 3;
					nDownloadPercent = (100*lParam)/pItemData->nFileSize;
					strTitle2.Format(_T("已下载%d%%"), nDownloadPercent );					
				}
			}
			else if(EVulfix_DownloadEnd==evt)
			{
				++ m_nRepairDownloaded;
				strTitle = _T("已下载");
				
				nSubitem2 = 3;
				strTitle2 = strTitle;
			}
			else if(EVulfix_DownloadError==evt)
			{
				clr = red;
				strTitle = _T("下载失败 ");
				++ m_nRepairProcessed;

				nSubitem2 = 3;
				clr2 = red;
				strTitle2 = strTitle;
			}
			else
				strTitle = _T("正连接 ");
			break;
		
		case EVulfix_InstallBegin:
		case EVulfix_InstallEnd:
		case EVulfix_InstallError:
			nSubitem = 3;
			if(EVulfix_InstallBegin==evt)
			{
				clr = blue;
				strTitle = _T("正安装 ");
			}
			else if(EVulfix_InstallEnd==evt)
			{
				clr = green;
				strTitle = _T("已安装");
				++m_nRepairInstalled;
			}
			else if(EVulfix_InstallError==evt)
			{
				clr = red;
				strTitle = _T("安装失败");
			}
			
			if(EVulfix_InstallError==evt || EVulfix_InstallEnd==evt)
				++ m_nRepairProcessed;
			break;

		case EVulfix_Task_Complete:
		case EVulfix_Task_Error:
			break;
		}
		if(nSubitem>=0)
		{
			m_wndListCtrl.SetSubItem(nIndex, nSubitem, strTitle, SUBITEM_TEXT, FALSE);
			m_wndListCtrl.SetSubItemColor(nIndex, nSubitem, clr);
		}
		if(nSubitem2>=0)
		{
			m_wndListCtrl.SetSubItem(nIndex, nSubitem2, strTitle2, SUBITEM_TEXT, FALSE);
			m_wndListCtrl.SetSubItemColor(nIndex, nSubitem2, clr2);
		}
		if(evt==EVulfix_DownloadBegin || evt==EVulfix_DownloadError || evt==EVulfix_InstallEnd || evt==EVulfix_InstallError )
			// 更新Title 
			// TODO : 下载进度考虑进去??
		{
			_UpdateRepairTitle();
			_SetRepairProgress( m_nRepairTotal==0 ? 100 : (100*m_nRepairProcessed)/m_nRepairTotal );
		}
	}
	return 0;
}

void CViewVulScanHandler::_UpdateRepairTitle()
{
	CString strTitle;
	strTitle.Format(_T("正在修复系统漏洞(%d, %d/%d)..."), m_nRepairInstalled, m_nRepairDownloaded, m_nRepairTotal );
	SetItemText(2004, strTitle);
}

int CViewVulScanHandler::FindListItem( int nID )
{
	static int nItem = -1;

	if(nItem>=0 && nItem<m_wndListCtrl.GetItemCount())
	{
		T_VulListItemData *pItemData = (T_VulListItemData *)m_wndListCtrl.GetItemData(nItem);
		if(pItemData && pItemData->nID==nID)
			return nItem;
	}

	for(int i=0; i<m_wndListCtrl.GetItemCount(); ++i)
	{
		T_VulListItemData *pItemData = (T_VulListItemData *)m_wndListCtrl.GetItemData(i);
		if(pItemData && pItemData->nID==nID)
			return nItem = i;
	}
	return nItem = -1;
}

void CViewVulScanHandler::ShowDashPage( int nID, BOOL showIntro/*=TRUE*/ )
{
	if(nID==m_nDashPage)
		return; 

	if(m_nDashPage)
		SetItemVisible(m_nDashPage, FALSE);
	m_nDashPage = nID;
	SetItemVisible(m_nDashPage, TRUE);

	if(showIntro)
	{
		SetItemVisible(210, TRUE);
		SetItemVisible(211, FALSE);
	}
	else
	{
		SetItemVisible(210, FALSE);
		SetItemVisible(211, TRUE);
	}
}

void CViewVulScanHandler::OnBtnStartScan()
{
	StartScan();
	return;
}

void CViewVulScanHandler::OnBtnCancelScan()
{
	theEngine->CancelScanVul();
	m_RefWin.SetNoDisturb(FALSE, FALSE);
	BOOL bHandled = FALSE;
	OnScanDone(WMH_SCAN_DONE, 1, 0, bHandled);
}

void CViewVulScanHandler::OnBtnViewVulsDetail()
{
	SetTabCurSel(IDBK_TAB_MAIN, 1);
	SetTabCurSel(3003, 0);	
}

void CViewVulScanHandler::OnBtnViewSoftVulsDetail()
{
	SetTabCurSel(IDBK_TAB_MAIN, 2);
}

void CViewVulScanHandler::OnBtnRepair()
{
	ATLASSERT(theEngine->m_pVulScan && theEngine->m_pSoftVulScan);
	
	RepairCOMVul( theEngine->m_pSoftVulScan->GetResults() );

	// INIT Listbox
	ShowDashPage(204, FALSE);
	ResetListCtrl(m_wndListCtrl);
	
	m_nRepairTotal = 0;
	
	CSimpleArray<int> arrVul, arrSoftVul;
	
	const CSimpleArray<LPTUpdateItem>& arr = theEngine->m_pVulScan->GetResults();
	for(int i=0; i<arr.GetSize(); ++i)
	{
		LPTUpdateItem pItem = arr[i];
		if(NeedRepair( pItem ))
		{
			_AppendVulItem( CreateListItem(pItem) );
			arrVul.Add( pItem->nID );
			++m_nRepairTotal;
		}
	}
	
	const CSimpleArray<LPTVulSoft>& arr2= theEngine->m_pSoftVulScan->GetResults();
	for(int i=0; i<arr2.GetSize(); ++i)
	{
		LPTVulSoft pItem = arr2[i];
		int state = GetSoftItemState(pItem->state, pItem->nDisableCom);
		if( state==VUL_UPDATE )
		{
			_AppendVulItem( CreateListItem(pItem) );
			arrSoftVul.Add( pItem->nID );
			++m_nRepairTotal;
		}
	}
	
	m_nRepairInstalled = 0;
	m_nRepairDownloaded = 0;
	m_nRepairProcessed = 0;
	_UpdateRepairTitle();
	_SetRepairProgress( 0 );
	
	m_RefWin.SetNoDisturb(TRUE, TRUE, 20480);
	theEngine->RepairAll( (HWND)-1, arrVul, arrSoftVul);
}

void CViewVulScanHandler::OnBtnCancelRepair()
{
	theEngine->CancelRepair();
	m_RefWin.SetNoDisturb(FALSE, FALSE);
	_DisplayRelateVulInfo(-1);
	OnBtnStartScan();
}

LRESULT CViewVulScanHandler::OnScanStart( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	StartScan();
	return 0;
}

LRESULT CViewVulScanHandler::OnScanDone( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	theEngine->WaitScanDone();
	m_RefWin.SetNoDisturb(FALSE, FALSE);
	
	BOOL isCanceld = wParam!=0;
	
	// 
	if(isCanceld)
	{
		ShowDashPage( 201 );
	}
	else
	{
		ATLASSERT(theEngine->m_pVulScan && theEngine->m_pSoftVulScan);
		int nVul = CountValidVuls(theEngine->m_pVulScan->GetResults());
		int nSoftVul = CountValidVuls(theEngine->m_pSoftVulScan->GetResults());
		
		if( nVul==0 && nSoftVul==0 )
		{
			ShowDashPage( 202 );
		}
		else
		{
			int nPos = 0;
			
			if(nVul>0)
			{
				SetItemVisible(2030, true);
				SetItemVisible(2031, false);
			}
			else
			{
				SetItemVisible(2030, false);
				SetItemVisible(2031, true);
			}
			
			if( nVul>0 )
			{
				CString strVul;
				strVul.Format(_T("发现 %d 个系统漏洞"), nVul );
				SetItemText(2002, strVul);

				nPos += 25;
				SetItemVisible( 221, true); 
			}
			else
				SetItemVisible( 221, false);
			
			if( nSoftVul>0 )
			{
				CString strVul;
				strVul.Format(_T("发现 %d 个软件漏洞"), nSoftVul );
				SetItemText(2003, strVul);
				
				CStringA strPos;
				strPos.Format("0,%d,-0,-0", nPos);
				SetItemAttribute(222, "pos", strPos);
				SetItemVisible( 222, true); 
			}
			else
				SetItemVisible( 222, false); 

			ShowDashPage( 203 );
		}
	}
	//m_btn_repair.EnableWindow( FALSE ) ;
	//m_btn_ignore.EnableWindow( FALSE ) ;
	return 0;
}

LRESULT CViewVulScanHandler::OnRepaireDone( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	m_RefWin.SetNoDisturb(FALSE, FALSE);

	BOOL isCanceld = wParam!=0;
	
	if(isCanceld)
	{
		// 重新扫描 
		OnBtnStartScan();
	}
	else
	{
		// 安装正常结束 
		int nToRepair = CountValidVuls(theEngine->m_pVulScan->GetResults()) + theEngine->m_pSoftVulScan->Count();

		if(  nToRepair==m_nRepairInstalled )
		{
			ShowDashPage( 205, FALSE );
		}
		else if(m_nRepairInstalled>0)
		{
			ShowDashPage( 206, FALSE );
		}
		else
		{
			ShowDashPage( 207, FALSE );
		}

		if (GetItemCheck(1008))
		{
			m_RefWin.ShutDownComputer(TRUE);
		}
	}
	return 0;
}

void CViewVulScanHandler::OnBtnRunBackground()
{
	ShowWindow(SW_SHOWMINIMIZED);
}

void CViewVulScanHandler::OnBtnRebootNow()
{
	m_RefWin.ShutDownComputer(TRUE);
}

void CViewVulScanHandler::OnBtnRebootLater()
{
	OnBtnStartScan();
}

LRESULT CViewVulScanHandler::OnListBoxNotify( int idCtrl, LPNMHDR pnmh, BOOL& bHandled )
{
	bHandled = FALSE;
	if(pnmh->code==NM_CLICK)
	{
		DEBUG_TRACE(_T("ItemClicked %d : %d\n"), idCtrl, pnmh->code);
	}
	else if(pnmh->code==LVN_ITEMCHANGED)
	{
		LPNMLISTVIEW pnmv = (LPNMLISTVIEW) pnmh; 
		DEBUG_TRACE(_T("ItemChanged (%d %d,%d,%d)\n"), pnmv->iItem, pnmv->uOldState, pnmv->uNewState, pnmv->uChanged);

		if(pnmv->uNewState & LVIS_SELECTED)
		{
			_DisplayRelateVulInfo( pnmv->iItem );
		}
	}
	return 0;
}

LRESULT CViewVulScanHandler::OnRichEditLink( int idCtrl, LPNMHDR pnmh, BOOL& bHandled )
{
	ENLINK *pLink = (ENLINK*)pnmh;
	if(pLink->msg==WM_LBUTTONUP)
	{
		// 点击了了解更多
		T_VulListItemData *pItemData = NULL;
		if(m_nItemOfRightInfo>=0)
			pItemData = (T_VulListItemData*)m_wndListCtrl.GetItemData( m_nItemOfRightInfo );
		if(pItemData)
		{
			ATLASSERT(!pItemData->strWebPage.IsEmpty());
			ShellExecute(NULL, _T("open"), pItemData->strWebPage, NULL, NULL, SW_SHOW);
		}
	}
	return 0;
}



void CViewVulScanHandler::_DisplayRelateVulInfo( int nItem )
{
	if(m_nItemOfRightInfo==nItem)
		return; 

	m_nItemOfRightInfo = nItem;

	// Item Selected 
	T_VulListItemData *pItemData = NULL;
	if(nItem>=0)
		pItemData = (T_VulListItemData*)m_wndListCtrl.GetItemData( nItem );
	if(pItemData)
	{
		SetItemVisible(240, FALSE, FALSE);
		SetItemVisible(241, TRUE);
		
		SetRelateInfo(m_ctlRichEdit, pItemData, FALSE);
	}
	else
	{
		SetItemVisible(240, TRUE, FALSE);
		SetItemVisible(241, FALSE);
	}
}

void CViewVulScanHandler::_SetScanProgress( int nPos )
{
	SetItemDWordAttribute(112, "value", nPos, TRUE);
}

void CViewVulScanHandler::_SetRepairProgress( int nPos )
{
	SetItemDWordAttribute(20400, "value", nPos, TRUE);
}
