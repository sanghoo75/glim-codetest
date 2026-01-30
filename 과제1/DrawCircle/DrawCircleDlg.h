
// DrawCircleDlg.h: 헤더 파일
//

#pragma once

#include "CImageView.h"
#include "ThreePointCircleDrawer.h"

// CDrawCircleDlg 대화 상자
class CDrawCircleDlg : public CDialogEx
{
// 생성입니다.
public:
	CDrawCircleDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DRAWCIRCLE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

// YSH
public:
	const int m_nScreenWidth = 640;
	const int m_nScreenHeight = 480;
	BYTE* m_pImageBuffer;

	CImageView* m_pImageView;

	CThreePointCircleDrawer m_circleDrawer;
	BOOL	m_bLbuttonDown;

	int m_nCircleThickness;
	int m_nRunInterval;
	int m_nRunMaxNum;

	void UiToVal();
	void ValToUi();
	BOOL m_bInit;
	BOOL m_bUiUpdating;

// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnUpdateTitleCoords(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnBnClickedBtnMoveRand();
	afx_msg void OnEnChangeEditCircleThickness();
};
