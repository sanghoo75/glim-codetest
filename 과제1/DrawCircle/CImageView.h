#pragma once
// CImageView 보기
#include "define.h"

#define _USE_MATH_DEFINES // MSVC에서 M_PI와 같은 상수를 노출하기 위해 필요
#include <string>
#include <afxmt.h> // CCriticalSection 사용
#include <vector>
#include <memory>
#include <cmath>    //M_PI 사용
#include <chrono> //시간측정용
#include <thread> //병렬처리용

#include <atlimage.h>   // CImage
#include <afx.h>        // CBitmap, CString, CRect

using namespace std;

class CImageView : public CScrollView
{
	DECLARE_DYNCREATE(CImageView)

protected:

public:
	CImageView();           // 동적 만들기에 사용되는 protected 생성자입니다.
	virtual ~CImageView();

//YSH
protected:
	CCriticalSection m_csImage; // 이미지 및 게이지 보호용
	
	CImage m_Image;
	BOOL m_bNewImage; // 새 이미지 플래그

public:

	//모든 이미지 타입들은 화면출력해야 하니까 CImage로 변환하여 사용
	void SetImage(BYTE* pImg, int width, int height);
	void SetImage(CImage* pImage); // CImage 설정 함수
	void SafeTestAccess(CImage* pImage);

    HCURSOR m_hCurSizeAll;
    HCURSOR m_hCurCross;

	
	CImage GetImage() {
		return m_Image;
	}
    bool IsEqual(double a, double b, double epsilon = 1e-9)
    {
        return std::fabs(a - b) < epsilon;
    }
    int GetScrollbarSize(int nBar);

protected:


public:
	template<typename T, typename... Args>
	void AddShape(Args&&... args)
	{
		CSingleLock lock(&m_csImage, TRUE);
		m_dsOverlay.push_back(std::make_shared<T>(std::forward<Args>(args)...));
	}

	// Overlay 도형 추가 함수
	void AddLine(CPoint pt1, CPoint pt2, COLORREF color = RGB(255, 0, 0), int thickness = 2);
	void AddPassLine(CPoint pt1, CPoint pt2, COLORREF color = RGB(255, 0, 0), int thickness = 2);
	void AddSingleArrow(CPoint pt1, CPoint pt2, COLORREF color = RGB(255, 0, 0), int thickness = 2);
	void AddDoubleArrow(CPoint pt1, CPoint pt2, COLORREF color = RGB(255, 0, 0), int thickness = 2);
	void AddCircle(CPoint center, int radius, COLORREF color = RGB(0, 255, 0), int thickness = 2);
	void AddRect(CRect rect, COLORREF color = RGB(0, 0, 255), int thickness = 2);
	void AddText(CString text, CPoint pos, COLORREF color = RGB(0, 0, 0), int fontSize = 12);

	double m_dZoomFactor; // 현재 배율 (기본 1.0)

	BYTE* m_pGrayImg;
	int m_nGrayImgSize;

    BOOL CropAndSaveBitmap(const CBitmap& bmpSrc, const CRect& inputRect, const CString& outFilename);

	void UpdateScrollSizes(); // 스크롤 크기 업데이트
	void ZoomFit(); // Zoom Fit 기능
	void ZoomOrg(); // Zoom Out 기능
	void ZoomIn(); // Zoom In 기능
	void ZoomOut(); // Zoom Out 기능

	BOOL m_bLBtnDown;
	double m_dZoomFactorMin;

	BOOL m_bEditMode; // 편집 모드 플래그
	BOOL m_bZoomFit;
	BOOL m_bZoomOrg;

	BOOL m_bMBtnDown;	//드래그 위해 L버튼다운
	CPoint m_ptMButton;

	CPoint      m_ptStartImg;    // 드래그 시작점 (이미지 좌표)
	CPoint      m_ptEndImg;      // 드래그 끝점 (이미지 좌표)
	CRect       m_rcSelection;   // 최종 선택 영역 (이미지 좌표)

	// ====== 헬퍼 함수 ======
	void DrawCheckerBackground(CDC& dc, const CRect& rectClient); // 체스판 배경
	void DrawNoImageMessage(CDC& dc, const CRect& rectClient);    // No Image 표시
	void CreateZoomCache(CDC* pDC);                               // 확대/축소 캐시 생성

	// ====== 더블 버퍼링 ======
	CDC      m_memDC;           // 더블 버퍼용 DC
	CBitmap  m_memBitmap;       // 더블 버퍼용 비트맵
	CSize    m_memSize;         // 버퍼 크기
	CBitmap* m_pOldMemBitmap;   // 원래 선택된 비트맵 핸들 (복원용)

	// ====== 확대/축소 캐시 ======
	CBitmap  m_zoomCache;       // 확대된 이미지 캐시
	CSize    m_zoomCacheSize;   // 캐시된 이미지 크기 (배율 변경 시 갱신)

	//좌표계정리
	// client => ZoomFit()했을경우 마진까지 포함한 화면자표
	// Image => 원본이미지 좌표
	// DispArea => ZoomFit()했을때 마진제외하고 실제 이미지가 그려지는 좌표
	// 좌표 변환 함수
	CPoint ClientToImage(CPoint ptClient);
	CPoint ImageToClient(CPoint ptImage);
	CPoint ImageToDispArea(CPoint ptImage);
	CPoint GetImageDispOrg();	//ZoomFit했을때의 이미지가 클라이인트 영역의 중간에 그려짐. 이때 그려저야하는 원점 계산
	CSize GetZoomImageSize();

	void DrawShape(HDC hdc, double dZoomFactor);
	void DrawSelectRect(HDC hdc);
	void SaveRsultmage(CString strFileName);

	//void ZoomAtPoint(CPoint ptMouse, int zDelta);
    void ZoomAtImgPoint(CPoint ptImg, double dNewZoomFactor);
    void ZoomAtMousePoint(CPoint ptMouse, double dNewZoomFactor);
   
protected:

private:
	

public:
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif
public:
	virtual void OnInitialUpdate();     // 생성된 후 처음입니다.

protected:
	virtual void OnDraw(CDC* pDC);      // 이 뷰를 그리기 위해 재정의되었습니다.
	virtual void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	virtual void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnContextMenuZoomFit();
	afx_msg void OnContextMenuZoomOrg();
	afx_msg void OnContextMenuZoomIn();
	afx_msg void OnContextMenuZoomOut();
	afx_msg void OnContextMenuEditMode();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg LRESULT OnUpdateDisplay(WPARAM wParam, LPARAM lParam);
};
