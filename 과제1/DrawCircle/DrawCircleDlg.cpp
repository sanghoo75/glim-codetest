// DrawCircleDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "DrawCircle.h"
#include "DrawCircleDlg.h"
#include "afxdialogex.h"

#include "define.h"
#include <thread>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CDrawCircleDlg 대화 상자
CDrawCircleDlg::CDrawCircleDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DRAWCIRCLE_DIALOG, pParent), 
	m_bLbuttonDown(FALSE), m_bInit(FALSE), 
	m_nCircleThickness(1),
	m_nRunInterval(2000),
	m_nRunMaxNum(10),
	m_bUiUpdating(FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDrawCircleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDrawCircleDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CDrawCircleDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CDrawCircleDlg::OnBnClickedCancel)
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_BN_CLICKED(IDC_BTN_MOVE_RAND, &CDrawCircleDlg::OnBnClickedBtnMoveRand)
	ON_EN_CHANGE(IDC_EDIT_CIRCLE_THICKNESS, &CDrawCircleDlg::OnEnChangeEditCircleThickness)
END_MESSAGE_MAP()


// CDrawCircleDlg 메시지 처리기

void CDrawCircleDlg::UiToVal()
{
	m_bUiUpdating = TRUE;
	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_CIRCLE_THICKNESS);
	if (pEdit != nullptr)
	{
		CString s;
		pEdit->GetWindowTextW(s);
		m_nCircleThickness = _ttoi(s);
		if (m_nCircleThickness < 1) m_nCircleThickness = 1;
		if (m_nCircleThickness > 10) m_nCircleThickness = 10;

		s.Format(_T("%d"), m_nCircleThickness);
		pEdit->SetWindowTextW(s);
	}

	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_RUN_INTERVAL);
	if (pEdit != nullptr)
	{
		CString s;
		pEdit->GetWindowTextW(s);
		m_nRunInterval = _ttoi(s);
		if (m_nRunInterval < 100) m_nRunInterval = 100;
		if (m_nRunInterval > 10000) m_nRunInterval = 10000;

		s.Format(_T("%d"), m_nRunInterval);
		pEdit->SetWindowTextW(s);
	}
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_RUN_MAX_NUM);
	if (pEdit != nullptr)
	{
		CString s;
		pEdit->GetWindowTextW(s);
		m_nRunMaxNum = _ttoi(s);
		if (m_nRunMaxNum < 1) m_nRunMaxNum = 1;
		if (m_nRunMaxNum > 30) m_nRunMaxNum = 30;

		s.Format(_T("%d"), m_nRunMaxNum);
		pEdit->SetWindowTextW(s);
	}
	m_bUiUpdating = FALSE;
}

void CDrawCircleDlg::ValToUi()
{

	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_CIRCLE_THICKNESS);
	if (pEdit != nullptr)
	{
		CString s;
		s.Format(_T("%d"), m_nCircleThickness);
		pEdit->SetWindowTextW(s);
	}
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_RUN_INTERVAL);
	if (pEdit != nullptr)
	{
		CString s;
		s.Format(_T("%d"), m_nRunInterval);
		pEdit->SetWindowTextW(s);
	}
	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_RUN_MAX_NUM);
	if (pEdit != nullptr)
	{
		CString s;
		s.Format(_T("%d"), m_nRunMaxNum);
		pEdit->SetWindowTextW(s);
	}

}

BOOL CDrawCircleDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	CRect rectScreen(0,0, m_nScreenWidth, m_nScreenHeight);
	CStatic* pStatic = (CStatic*)GetDlgItem(IDC_SCREEN);
	if (pStatic != nullptr)
	{
		pStatic->MoveWindow(rectScreen);
	}

	CRect rect;
	pStatic->GetClientRect(&rect);
	pStatic->MapWindowPoints(this, &rect);

	m_pImageView = new CImageView();

	BOOL bOK = m_pImageView->Create(
		NULL,
		_T(""),
		WS_CHILD | WS_VISIBLE | WS_BORDER,
		rect,
		this,
		ID_IMAGEVIEW
	);

	if (!bOK) {
		DWORD err = GetLastError();
		CString s; s.Format(_T("Create 실패, err = %u"), err);
		AfxMessageBox(s);
	}

	m_pImageView->OnInitialUpdate();


	m_pImageBuffer = new BYTE[m_nScreenWidth * m_nScreenHeight];
	memset(m_pImageBuffer, 0xFF, m_nScreenWidth * m_nScreenHeight);
	//ZeroMemory(m_pImageBuffer, m_nScreenWidth * m_nScreenHeight);

	m_pImageView->SetImage(m_pImageBuffer, m_nScreenWidth, m_nScreenHeight);
	m_pImageView->ZoomFit();

	m_circleDrawer.SetImage(m_pImageBuffer, m_nScreenWidth, m_nScreenHeight);

	ValToUi();

	m_bInit = TRUE;
	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CDrawCircleDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CDrawCircleDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CDrawCircleDlg::OnBnClickedOk()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	
	//CDialogEx::OnOK();
}

void CDrawCircleDlg::OnBnClickedCancel()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.

	const int result = MessageBox(_T("종료 하시겠습니까?"), _T("종료 확인"), MB_ICONQUESTION | MB_OKCANCEL);
	if (result == IDOK)
	{
		if (m_pImageView != nullptr)
		{
			delete m_pImageView;
			m_pImageView = nullptr;
		}

		CDialogEx::OnCancel();
	}
	// 취소를 누르면 아무 동작도 하지 않음 (다이얼로그 계속 표시)
}

void CDrawCircleDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	// TODO: 여기에 메시지 처리기 코드를 추가합니다.
	

}

LRESULT CDrawCircleDlg::OnUpdateTitleCoords(WPARAM wParam, LPARAM lParam)
{
	// wParam과 lParam으로부터 좌표를 얻어옵니다.
	int x = static_cast<int>(wParam);
	int y = static_cast<int>(lParam);

	// 타이틀 바에 표시할 문자열 생성
	CString strTitle;
	strTitle.Format(_T("Image - 좌표: (%d, %d)"), x, y);

	// 윈도우 타이틀 업데이트
	SetWindowText(strTitle);
	return LRESULT();
}

void CDrawCircleDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
	TRACE(_T("CDrawCircleDlg::OnLButtonDown: (%d, %d)\n"), point.x, point.y);

	m_circleDrawer.OnLButtonDown(point);
	m_bLbuttonDown = TRUE;

	m_circleDrawer.Draw();
	m_pImageView->SetImage(m_pImageBuffer, m_nScreenWidth, m_nScreenHeight);

//	Invalidate(FALSE);

	CDialogEx::OnLButtonDown(nFlags, point);
}

void CDrawCircleDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.

	// 타이틀 바에 표시할 문자열 생성
	CString strTitle;
	strTitle.Format(_T("Image - 좌표: (%d, %d)"), point.x, point.y);

	// 윈도우 타이틀 업데이트
	SetWindowText(strTitle);

	m_circleDrawer.OnMouseMove(point, m_bLbuttonDown);

	if (m_bLbuttonDown) {
		m_circleDrawer.Draw();
		m_pImageView->SetImage(m_pImageBuffer, m_nScreenWidth, m_nScreenHeight);
	}
	

//	Invalidate(FALSE);

	CDialogEx::OnMouseMove(nFlags, point);
}

void CDrawCircleDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
	TRACE(_T("CDrawCircleDlg::OnLButtonUp: (%d, %d)\n"), point.x, point.y);

	m_bLbuttonDown = FALSE;
	m_circleDrawer.OnLButtonUp(point);

	m_circleDrawer.Draw();
	m_pImageView->SetImage(m_pImageBuffer, m_nScreenWidth, m_nScreenHeight);

//	Invalidate(FALSE);

	CDialogEx::OnLButtonUp(nFlags, point);
}

//스래드 함수
//UINT AFX_CDECL ThreadFuncMoveCircleRand(LPVOID pParam)
UINT ThreadFuncMoveCircleRand(LPVOID pParam)
{
	CDrawCircleDlg* pDlg = (CDrawCircleDlg*)pParam;
	if (pDlg == nullptr) return 0;
	pDlg->UiToVal();
	for (int i = 0; i < pDlg->m_nRunMaxNum; i++)
	{
		int x = rand() % pDlg->m_nScreenWidth;
		int y = rand() % pDlg->m_nScreenHeight;
		CPoint center(x, y);
		int nIdx = rand() % 3;
		pDlg->m_circleDrawer.MovePoint(nIdx, center);
		pDlg->m_circleDrawer.Draw();
		pDlg->m_pImageView->SetImage(pDlg->m_pImageBuffer, pDlg->m_nScreenWidth, pDlg->m_nScreenHeight);
		Sleep(pDlg->m_nRunInterval);
	}
	return 0;
}

void CDrawCircleDlg::OnBnClickedBtnMoveRand()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	// 스레드 생성
	UiToVal();
	AfxBeginThread(ThreadFuncMoveCircleRand, this);
}

void CDrawCircleDlg::OnEnChangeEditCircleThickness()
{
	// TODO:  RICHEDIT 컨트롤인 경우, 이 컨트롤은
	// CDialogEx::OnInitDialog() 함수를 재지정 
	//하고 마스크에 OR 연산하여 설정된 ENM_CHANGE 플래그를 지정하여 CRichEditCtrl().SetEventMask()를 호출하지 않으면
	// ENM_CHANGE가 있으면 마스크에 ORed를 플래그합니다.

	// TODO:  여기에 컨트롤 알림 처리기 코드를 추가합니다.
	if (!m_bInit) return;
	if (m_bUiUpdating) return;
	if (m_pImageView == nullptr) return;

	UiToVal();

	m_circleDrawer.SetCircleThickness(m_nCircleThickness);

}