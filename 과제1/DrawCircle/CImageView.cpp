// CImageView.cpp: 구현 파일
//

#include "pch.h"
#include "CImageView.h"
#include "define.h"


// CImageView

IMPLEMENT_DYNCREATE(CImageView, CScrollView)

CImageView::CImageView() : m_dZoomFactor(1.0), m_bEditMode(FALSE), 
m_dZoomFactorMin(0.1), m_bZoomFit(FALSE), m_bZoomOrg(FALSE), m_bMBtnDown(FALSE), m_ptMButton(CPoint(0,0)),
m_pGrayImg(nullptr), m_bLBtnDown(FALSE), m_ptStartImg(CPoint(0,0)),
m_ptEndImg(CPoint(0, 0)), m_rcSelection(CRect(0, 0, 0, 0)), m_nGrayImgSize(0), m_bNewImage(FALSE),
m_hCurSizeAll(NULL), m_hCurCross(NULL), m_pOldMemBitmap(nullptr), m_memSize(0, 0), m_zoomCache(), m_zoomCacheSize(0, 0)
{
    m_hCurSizeAll = LoadCursor(NULL, IDC_SIZEALL);
    m_hCurCross = LoadCursor(NULL, IDC_CROSS);
}

CImageView::~CImageView()
{
#if HAS_EASYIMAGE
    try{
		//Easy::Terminate();
    }
    catch (const EException& e) {
        e.what();
    }
#endif

    if (m_pGrayImg) {
        delete[] m_pGrayImg;
        m_pGrayImg = nullptr;
    }

#if HAS_EASYIMAGE
    // EasyImage 라이브러리 정리

#endif

#if HAS_EASYGAUGE
    if (m_pEWorldShape) {
        delete m_pEWorldShape;
        m_pEWorldShape = nullptr;
	}
#endif
}


BEGIN_MESSAGE_MAP(CImageView, CScrollView)
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
    ON_COMMAND(ID_ZOOM_FIT, &CImageView::OnContextMenuZoomFit)
    ON_COMMAND(ID_ZOOM_ORG, &CImageView::OnContextMenuZoomOrg)
    ON_COMMAND(ID_ZOOM_IN, &CImageView::OnContextMenuZoomIn)
    ON_COMMAND(ID_ZOOM_OUT, &CImageView::OnContextMenuZoomOut)
    ON_COMMAND(ID_EDIT_MODE, &CImageView::OnContextMenuEditMode)
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MBUTTONDOWN()
    ON_WM_MBUTTONUP()
    ON_WM_MOUSEWHEEL()
    ON_MESSAGE(WM_UPDATE_DISPLAY, &CImageView::OnUpdateDisplay)
END_MESSAGE_MAP()


// CImageView 그리기

void CImageView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();
    CSize sizeTotal;
    sizeTotal.cx = sizeTotal.cy = 100;
    SetScrollSizes(MM_TEXT, sizeTotal);
}

void CImageView::OnContextMenuZoomFit()
{
    ZoomFit();
}

void CImageView::OnContextMenuZoomIn()
{
    ZoomIn();
}

void CImageView::OnContextMenuZoomOrg()
{
    ZoomOrg();
}


void CImageView::OnContextMenuZoomOut()
{
    ZoomOut();
}

void CImageView::OnContextMenuEditMode()
{
    //
	TRACE(_T("Edit Mode : %d\n"), m_bEditMode);
	Invalidate(FALSE);
}

void CImageView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CScrollView::OnHScroll(nSBCode, nPos, pScrollBar);
    Invalidate(FALSE); // 스크롤 시 전체 영역 무효화
    CPoint ptScrollPos = GetScrollPosition();
//	TRACE(_T("HScroll Pos: (%d, %d)\n"), ptScrollPos.x, ptScrollPos.y);
}

void CImageView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CScrollView::OnVScroll(nSBCode, nPos, pScrollBar);
    Invalidate(FALSE); // 스크롤 시 전체 영역 무효화
    CPoint ptScrollPos = GetScrollPosition();
//    TRACE(_T("HScroll Pos: (%d, %d)\n"), ptScrollPos.x, ptScrollPos.y);
}
BOOL CImageView::OnEraseBkgnd(CDC* pDC)
{
    // 배경을 지우는 작업을 막아 깜빡임을 방지합니다.
    // 더블버퍼링에서 배경은 메모리 DC에서 먼저 그려지므로
    // 이 단계는 필요하지 않습니다.
    return TRUE;
}

void CImageView::OnDraw(CDC* pDC)
{
    CSingleLock lock(&m_csImage, TRUE);

    CRect rectClient;
    GetClientRect(&rectClient);

    // --- 더블 버퍼 DC 생성/재사용 ---
    if (nullptr == m_memDC.GetSafeHdc())
    {
        m_memDC.CreateCompatibleDC(pDC);
        m_pOldMemBitmap = nullptr;
    }

    if (0 == m_memBitmap.GetSafeHandle() || m_memSize != rectClient.Size()/* || m_bNewImage*/)
    {
        // 기존 버퍼 정리 (선택된 비트맵을 원복 후 삭제)
        if (0 != m_memBitmap.GetSafeHandle())
        {
            if (nullptr != m_pOldMemBitmap)
            {
                m_memDC.SelectObject(m_pOldMemBitmap);
                m_pOldMemBitmap = nullptr;
            }
            m_memBitmap.DeleteObject();
        }

        // 새 버퍼 생성 & 선택
        m_memBitmap.CreateCompatibleBitmap(pDC, rectClient.Width(), rectClient.Height());
        m_pOldMemBitmap = (CBitmap*)m_memDC.SelectObject(&m_memBitmap);
        m_memSize = rectClient.Size();

    }

    // --- 배경(체스판) 그리기 (m_memDC 기준, origin = 0,0) ---
    DrawCheckerBackground(m_memDC, rectClient);

    // --- 이미지가 있으면 캐시에서 필요한 영역만 복사 ---
    if (FALSE == m_Image.IsNull())
    {
        // 확대된 이미지 크기
        CSize zoomSize = GetZoomImageSize();

        // 캐시 필요시 생성/갱신
        if (0 == m_zoomCache.GetSafeHandle() || m_zoomCacheSize != zoomSize || m_bNewImage)
        {
            CreateZoomCache(pDC); // 내부에서 m_zoomCache, m_zoomCacheSize 갱신
        }

        // 현재 스크롤/원점 정보
        CPoint ptScrollPos = GetScrollPosition();   // zoomCache 상에서의 좌상단 좌표
        CPoint ptOrg = GetImageDispOrg();           // 클라이언트에서 이미지가 시작되는 좌표 (센터링 등)

        // Source(zoomCache) 시작좌표
        int srcLeft = ptScrollPos.x;
        int srcTop = ptScrollPos.y;

        // Dest(m_memDC) 시작좌표
        int destLeft = ptOrg.x;
        int destTop = ptOrg.y;

        // 복사 가능한 너비/높이(클립)
        int availableDestW = rectClient.Width() - destLeft;
        int availableDestH = rectClient.Height() - destTop;
        int availableSrcW = zoomSize.cx - srcLeft;
        int availableSrcH = zoomSize.cy - srcTop;

        int copyWidth = min(availableDestW, availableSrcW);
        int copyHeight = min(availableDestH, availableSrcH);

        if (copyWidth > 0 && copyHeight > 0)
        {
            // 캐시 비트맵을 담는 임시 DC
            CDC cacheDC;
            cacheDC.CreateCompatibleDC(pDC);
            CBitmap* pOldCacheBmp = cacheDC.SelectObject(&m_zoomCache);

            // *** 사용자가 제시한 방식: m_memDC의 뷰포트 원점은 0,0 으로 유지하고 BitBlt 수행 ***
            m_memDC.SetViewportOrg(0, 0);
            m_memDC.BitBlt(destLeft, destTop, copyWidth, copyHeight,
                &cacheDC, srcLeft, srcTop, SRCCOPY);

            // 복원
            cacheDC.SelectObject(pOldCacheBmp);
        }

        // --- 오버레이(도형/선택영역) 그리기 ---
        // DrawShape/DrawSelectRect이 "이미지 좌표계(0..zoomSize-1)"로 그리는 구현이라면,
        // m_memDC의 뷰포트 원점을 (destLeft - srcLeft, destTop - srcTop) 으로 옮겨서 매핑시킨다.
        CPoint mappingOffset(destLeft - srcLeft, destTop - srcTop); // 이미지좌표 -> 클라이언트 좌표 변환
        CPoint oldOrg;
        oldOrg = m_memDC.SetViewportOrg(mappingOffset.x, mappingOffset.y);

        // 이미지 좌표계로 구현된 오버레이 그리기 (m_memDC에 그려짐)
        DrawShape(m_memDC.GetSafeHdc(), m_dZoomFactor);
        DrawSelectRect(m_memDC.GetSafeHdc());

        // 원점 복원
        m_memDC.SetViewportOrg(oldOrg.x, oldOrg.y);
    }
    else
    {
        // No Image 메시지
        DrawNoImageMessage(m_memDC, rectClient);
    }

    // --- 최종 출력: m_memDC -> 화면 pDC (항상 뷰포트 원점 0,0 으로 맞춤) ---
    pDC->SetViewportOrg(0, 0);
    pDC->BitBlt(0, 0, rectClient.Width(), rectClient.Height(), &m_memDC, 0, 0, SRCCOPY);

    m_bNewImage = FALSE; // 플래그 초기화
}


// CImageView 진단

#ifdef _DEBUG
void CImageView::AssertValid() const
{
	CScrollView::AssertValid();
}

#ifndef _WIN32_WCE
void CImageView::Dump(CDumpContext& dc) const
{
	CScrollView::Dump(dc);
}
#endif
#endif //_DEBUG


// CImageView 메시지 처리기

void CImageView::OnRButtonUp(UINT nFlags, CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
    // 컨텍스트 메뉴 생성

#if HAS_EASYGAUGE
    if (nullptr != m_pHitShape && m_bEditMode) {
        //메시지출력
        /*EPoint LocalToSensor(
            const EPoint & LPoint
        )*/

		EPoint eSensorPoint = m_pHitShape->LocalToSensor(EPoint(0, 0));
        CString strPoint;
		strPoint.Format(_T("Sensor Pos: (%.2f, %.2f)"), eSensorPoint.GetX(), eSensorPoint.GetY());
		AfxMessageBox(_T("Hit Shape: ") + CString(m_pHitShape->GetName().c_str()) + strPoint);
        return;
    }
#endif

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, ID_ZOOM_FIT, _T("Zoom Fit"));
    menu.AppendMenu(MF_STRING, ID_ZOOM_ORG, _T("Zoom 1:1"));
    menu.AppendMenu(MF_SEPARATOR); // 구분선 추가
    menu.AppendMenu(MF_STRING, ID_ZOOM_IN, _T("Zoom In"));
    menu.AppendMenu(MF_STRING, ID_ZOOM_OUT, _T("Zoom Out"));
#if HAS_EASYGAUGE
    menu.AppendMenu(MF_STRING, ID_EDIT_MODE, _T("Edit Mode"));
#endif

    // 체크 표시
    
    menu.CheckMenuItem(ID_ZOOM_FIT, MF_BYCOMMAND | (m_bZoomFit ? MF_CHECKED : MF_UNCHECKED));
    menu.CheckMenuItem(ID_ZOOM_ORG, MF_BYCOMMAND | (m_bZoomOrg ? MF_CHECKED : MF_UNCHECKED));
    menu.CheckMenuItem(ID_EDIT_MODE, MF_BYCOMMAND | (m_bEditMode ? MF_CHECKED : MF_UNCHECKED));
    ClientToScreen(&point);

    // 메뉴 표시
    //m_nCheckedMenu = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
    int nCheck = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
    if (nCheck != 0)
    {
        PostMessage(WM_COMMAND, nCheck, 0); // 해당 명령 실행

        if (ID_ZOOM_FIT == nCheck) {
			m_bZoomFit = TRUE;
			m_bZoomOrg = FALSE;
        }else if (ID_ZOOM_ORG == nCheck) {
            m_bZoomFit = FALSE;
            m_bZoomOrg = TRUE;
        }else if (ID_EDIT_MODE == nCheck) {
			// 편집 모드 토글
			m_bEditMode = !m_bEditMode;
        }
    }

    CScrollView::OnRButtonDown(nFlags, point);
}

void CImageView::OnMouseMove(UINT nFlags, CPoint point)
{
    // ===== 이미지 유효성 검사 =====
    if (m_Image.IsNull())
        return;

    // ===== 좌표 변환 =====
    const POINT ptScrollPos = GetScrollPosition();
    const CPoint ptClientToImage = ClientToImage(point);
    const CPoint ptImageToDispArea = ImageToDispArea(ptClientToImage);

    bool bNeedsInvalidate = false; // 화면 갱신 플래그

    // ===== 중간 버튼(스크롤 드래그) 처리 =====
    if (TRUE == m_bMBtnDown)
    {
        // 이동 거리 계산
        CPoint delta = point - m_ptMButton;
        m_ptMButton = point;

        // 스크롤 위치 업데이트 (역방향)
        CPoint newScrollPos = ptScrollPos;
        newScrollPos.x = max(0, min(newScrollPos.x - delta.x, GetTotalSize().cx));
        newScrollPos.y = max(0, min(newScrollPos.y - delta.y, GetTotalSize().cy));

        ScrollToPosition(newScrollPos);
    }

    // 좌표를 부모 다이얼로그에 전송
    GetParent()->SendMessage(
        WM_MOUSEMOVE,
        nFlags,
        MAKELPARAM(ptClientToImage.x, ptClientToImage.y)
    );

    // ===== 화면 갱신 =====
    if (TRUE == bNeedsInvalidate)
        Invalidate(FALSE);

    CScrollView::OnMouseMove(nFlags, point);
}

void CImageView::SetImage(BYTE* pImg, int width, int height)
{
    CSingleLock lock(&m_csImage, TRUE);

    if (nullptr == pImg || width <= 0 || height <= 0)
        return;

    if (!m_Image.IsNull())
        m_Image.Destroy();

    // height를 음수로 해서 top-down 생성
    if (FAILED(m_Image.Create(width, -height, 24)))
    {
        TRACE(_T("m_Image.Create() 실패\n"));
        return;
    }

    BYTE* pDstBits = static_cast<BYTE*>(m_Image.GetBits());
    int dstPitch = m_Image.GetPitch();

    //변환시간 측정
	auto start = std::chrono::high_resolution_clock::now();

    // OpenMP 병렬 변환 (8bpp → 24bpp RGB)
#pragma omp parallel for
    for (int y = 0; y < height; ++y)
    {
        BYTE* pDstLine = pDstBits + y * dstPitch;
        BYTE* pSrcLine = pImg + y * width;

        for (int x = 0; x < width; ++x)
        {
            BYTE gray = pSrcLine[x];
            pDstLine[x * 3 + 0] = gray; // B
            pDstLine[x * 3 + 1] = gray; // G
            pDstLine[x * 3 + 2] = gray; // R
        }
    }

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> duration = end - start;
	TRACE(_T("SetImage(BYTE*) size(%dx%d)- Conversion Duration: %.2f ms\n"), width, height, duration.count());


    m_Image.Save(_T("D:\\Copied_GrayToRGB.jpg"));
    m_bNewImage = TRUE;

    PostMessage(WM_UPDATE_DISPLAY, 0, 0);   //부모에서 콜백등으로 동작하더라도 문제없게 함.
//    ZoomFit(); 
//    Invalidate(FALSE);
}

//정상동작
void CImageView::SetImage(CImage* pImage)
{
    CSingleLock lock(&m_csImage, TRUE);

    if (nullptr == pImage || pImage->IsNull())
        return;
        
	pImage->Save(_T("D:\\1_SourceImage.jpg"));

	auto T0 = std::chrono::high_resolution_clock::now();

    // 1. 기존 m_Image가 있다면 삭제합니다.
    if (!m_Image.IsNull())
    {
        m_Image.Destroy();
    }

    int srcWidth = pImage->GetWidth();
    int srcHeight = pImage->GetHeight();
    int srcBPP = pImage->GetBPP();

    // 2. 8비트 이하 이미지 (팔레트 기반)와 16비트 이상 이미지를 분리하여 처리합니다.
    if (srcBPP <= 8)
    {
        // 8비트 이하의 이미지는 팔레트를 포함해야 합니다.
        // Create() 대신 CreateEx()를 사용해 팔레트 기반 이미지로 생성합니다.
        // 높이를 양수로 넣어 Top-Down 방식으로 생성합니다.
        m_Image.CreateEx(srcWidth, srcHeight, srcBPP, BI_RGB);

        // 3. 원본 이미지의 팔레트 정보를 읽어와서 대상 이미지에 설정합니다.
        HDC hSrcDC = pImage->GetDC();
        HDC hDestDC = m_Image.GetDC();

        if (nullptr != hSrcDC && nullptr != hDestDC)
        {
            // 팔레트 엔트리 수 계산 (256개 또는 그 이하)
            int nColors = 1 << srcBPP;
            std::vector<RGBQUAD> palette(nColors);

            // 원본 이미지에서 팔레트 정보 읽어오기
            ::GetDIBColorTable(hSrcDC, 0, nColors, palette.data());

            // 대상 이미지에 팔레트 정보 설정
            ::SetDIBColorTable(hDestDC, 0, nColors, palette.data());
        }

        if (nullptr != hSrcDC) pImage->ReleaseDC();
        if (nullptr != hDestDC) m_Image.ReleaseDC();
    }
    else // 16비트 이상 이미지는 일반 Create() 사용
    {
        // 높이를 양수로 넣어 Top-Down 방식으로 생성합니다.
        m_Image.Create(srcWidth, srcHeight, srcBPP);
    }

    auto T1 = std::chrono::high_resolution_clock::now();

    // 4. BitBlt를 사용하여 이미지 데이터를 복사합니다.
    HDC hSrcDC = pImage->GetDC();
    HDC hDestDC = m_Image.GetDC();

  //  SafeTestAccess(pImage);

    if (nullptr != hSrcDC && nullptr != hDestDC)
    {
        ::BitBlt(hDestDC, 0, 0, srcWidth, srcHeight,
            hSrcDC, 0, 0, SRCCOPY);
    }

    if (nullptr != hSrcDC) pImage->ReleaseDC();
    if (nullptr != hDestDC) m_Image.ReleaseDC();

    auto T3 = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double, std::milli> durationCreate = T1 - T0;
    std::chrono::duration<double, std::milli> durationBitBlt = T3 - T1;
    TRACE(_T("SetImage(CImage*) size(%dx%d) Create Duration: %.2f ms, BitBlt Duration: %.2f ms\n"), srcWidth, srcHeight, durationCreate, durationBitBlt);
    // Create Duration: 0.06 ms, BitBlt Duration: 20.67 ms
    
	// 디버깅용 저장

	m_Image.Save(_T("D:\\2_DestImage.jpg"));

    m_bZoomFit = TRUE;
    m_bZoomOrg = FALSE;

    m_bNewImage = TRUE;;

    ZoomFit(); // 이미지 설정 후 Zoom Fit 적용
    //Invalidate(FALSE); // 무효화하여 다시 그리기

}

void CImageView::SafeTestAccess(CImage* pImage)
{
    ASSERT(pImage);
    if (pImage->IsNull()) {
        TRACE(_T("pImage is null!\n"));
        return;
    }

    int width = pImage->GetWidth();
    int height = pImage->GetHeight();
    int bpp = pImage->GetBPP();
    int srcPitch = pImage->GetPitch();
    int srcPitchAbs = abs(srcPitch);

    BYTE* pSrcBits = (BYTE*)pImage->GetBits();
    if (!pSrcBits) {
        TRACE(_T("GetBits() returned nullptr!\n"));
        return;
    }

    // 전체 메모리 범위 계산
    BYTE* pSrcEnd = pSrcBits + (size_t)srcPitchAbs * height;

    TRACE(_T("==== Debug Info ====\n"));
    TRACE(_T("width=%d height=%d bpp=%d pitch=%d abs=%d pSrcBits=%p end=%p\n"),
        width, height, bpp, srcPitch, srcPitchAbs, pSrcBits, pSrcEnd);

    for (int y = 0; y < height; ++y)
    {
        int srcY = (srcPitch < 0) ? (height - 1 - y) : y;
        BYTE* pSrcLine = pSrcBits + (size_t)srcY * srcPitchAbs;

        // 주소 범위 검사
        if (pSrcLine < pSrcBits || pSrcLine + width > pSrcEnd) {
            TRACE(_T("[ERROR] Line out of range! y=%d srcY=%d pSrcLine=%p\n"),
                y, srcY, pSrcLine);
            return;
        }

        for (int x = 0; x < width; ++x)
        {
            // 접근하기 전 범위 다시 체크
            if (&pSrcLine[x] >= pSrcEnd) {
                TRACE(_T("[ERROR] Pixel out of range! y=%d x=%d addr=%p\n"),
                    y, x, &pSrcLine[x]);
                return;
            }

            BYTE gray = pSrcLine[x];
        }
    }
}

void CImageView::UpdateScrollSizes()
{
    TRACE(_T("UpdateScrollSizes()\n"));

    if (m_Image.IsNull())
    {
        SetScrollSizes(MM_TEXT, CSize(0, 0));
        return;
    }

    // 클라이언트 영역
    CRect rectClient;
    GetClientRect(&rectClient);

    //스크롤바의 크기 확인
    int nVScrollWidth = GetSystemMetrics(SM_CXVSCROLL);
    int nHScrollHeight = GetSystemMetrics(SM_CYHSCROLL);

    // 클라이언트 영역에서 스크롤바 크기만큼 보정
    rectClient.DeflateRect(0, 0, nVScrollWidth, nHScrollHeight);

    // 배율 적용된 이미지 크기
    CSize sizeTotal;
    sizeTotal.cx = static_cast<int>(m_Image.GetWidth() * m_dZoomFactor);
    sizeTotal.cy = static_cast<int>(m_Image.GetHeight() * m_dZoomFactor);

    // 방어 코드 추가
    if (sizeTotal.cx <= 0 || sizeTotal.cy <= 0)
    {
        TRACE(_T("Invalid scroll size: (%d, %d)\n"), sizeTotal.cx, sizeTotal.cy);
        return;
    }

    // 스크롤 크기 설정
    SetScrollSizes(MM_TEXT, sizeTotal); //임시
}

void CImageView::ZoomFit()
{
    //ZoomFit동작시에는 스크롤바가 없어지기때문에, 기존에 스크롤바가 있었는지 확인해서 처리필요함.
    m_bZoomFit = TRUE;
    m_bZoomOrg = FALSE;

    if (m_Image.IsNull())   {        return;    }

    CRect rectClient;
    GetClientRect(&rectClient);
    
    int hBarSize = GetScrollbarSize(SB_HORZ);
    int vBarSize = GetScrollbarSize(SB_VERT);
    rectClient.right += vBarSize;
    rectClient.bottom += hBarSize;


    double dZoomX = static_cast<double>(rectClient.Width()) / m_Image.GetWidth();
    double dZoomY = static_cast<double>(rectClient.Height()) / m_Image.GetHeight();

    m_dZoomFactorMin = min(dZoomX, dZoomY); // 최소 배율로 전체 이미지 표시
    m_dZoomFactor = m_dZoomFactorMin; // 최소 배율 저장
   
	PostMessage(WM_UPDATE_DISPLAY, 0, 0);   //부모에서 콜백등으로 동작하더라도 문제없게 함.

   /* UpdateScrollSizes();
    Invalidate(FALSE);*/
}

void CImageView::ZoomOrg()
{
    m_bZoomFit = FALSE;
    m_bZoomOrg = TRUE;

    if ( m_Image.IsNull())    {        return;    }

    m_dZoomFactor = 1;
    UpdateScrollSizes();
    Invalidate(FALSE);
}

void CImageView::ZoomIn()
{
    if (m_Image.IsNull())    {        return;    }

	m_bZoomFit = FALSE;
	m_bZoomOrg = FALSE;

    m_dZoomFactor *= 1.2;

    PostMessage(WM_UPDATE_DISPLAY, 0, 0);   //부모에서 콜백등으로 동작하더라도 문제없게 함.
   /* UpdateScrollSizes();
    Invalidate(FALSE);*/
}

void CImageView::ZoomOut()
{
    if ( m_Image.IsNull())    {        return;    }

    m_bZoomFit = FALSE;
    m_bZoomOrg = FALSE;

    m_dZoomFactor /= 1.2;

    if (m_dZoomFactor < m_dZoomFactorMin) m_dZoomFactor = m_dZoomFactorMin; // 최소 배율 제한

    PostMessage(WM_UPDATE_DISPLAY, 0, 0);   //부모에서 콜백등으로 동작하더라도 문제없게 함.
   /* UpdateScrollSizes();
    Invalidate(FALSE);*/
}

void CImageView::DrawShape(HDC hdc, double dZoomFactor)
{
}

void CImageView::DrawSelectRect(HDC hdc)
{
    if (m_rcSelection.IsRectEmpty())
        return;

    CDC dc;
    dc.Attach(hdc);   // HDC를 CDC에 연결 (소유권 이전)
    
    CRect rcClientSel;
    rcClientSel.left = ImageToDispArea(CPoint(m_rcSelection.left, m_rcSelection.top)).x;
    rcClientSel.top = ImageToDispArea(CPoint(m_rcSelection.left, m_rcSelection.top)).y;
    rcClientSel.right = ImageToDispArea(CPoint(m_rcSelection.right, m_rcSelection.bottom)).x;
    rcClientSel.bottom = ImageToDispArea(CPoint(m_rcSelection.right, m_rcSelection.bottom)).y;

    CPen pen(PS_DOT, 1, RGB(0, 0, 0));
    CPen* pOldPen = dc.SelectObject(&pen);
    CBrush* pOldBrush = (CBrush*)dc.SelectStockObject(NULL_BRUSH);

    dc.Rectangle(rcClientSel);

    dc.SelectObject(pOldPen);
    dc.SelectObject(pOldBrush);

    dc.Detach();      // 다시 HDC 분리 (소유권 반환)
}

void CImageView::SaveRsultmage(CString strFileName)
{
    if (!m_Image.IsNull())
    {
        // CImage를 JPEG 파일로 저장
		CImage imageCopy;
		imageCopy.Create(m_Image.GetWidth(), m_Image.GetHeight(), 24); // 24비트 RGB 이미지 생성
		CDC* pSrcDC = CDC::FromHandle(m_Image.GetDC());
		CDC* pDestDC = CDC::FromHandle(imageCopy.GetDC());
		pDestDC->BitBlt(0, 0, m_Image.GetWidth(), m_Image.GetHeight(), pSrcDC, 0, 0, SRCCOPY);

        m_Image.ReleaseDC();
        imageCopy.ReleaseDC();

		imageCopy.Save(strFileName, Gdiplus::ImageFormatJPEG);
    }
    else
    {
        AfxMessageBox(_T("No image to save."));
	}

}

CPoint CImageView::GetImageDispOrg()
{
    CPoint ptRet(0, 0);

    if (m_Image.IsNull())
        return ptRet;

    CPoint ptClient;
    CRect rectClient;
    GetClientRect(&rectClient);
    CPoint ptScrollPos = GetScrollPosition();

    // 배율 적용된 이미지 크기 계산
    int nZoomImgWidth = static_cast<int>(m_Image.GetWidth() * m_dZoomFactor);
    int nZoomImgHeight = static_cast<int>(m_Image.GetHeight() * m_dZoomFactor);

    int nLeft = 0;
    int nTop = 0;
    //이미지의 크기가 클라이언트 영역보다 작을 때 중앙 정렬
    if (nZoomImgWidth < rectClient.Width() || nZoomImgHeight < rectClient.Height())
    {
        nLeft = (rectClient.Width() - nZoomImgWidth) / 2;
        nTop = (rectClient.Height() - nZoomImgHeight) / 2;

        //ptScrollPos 스크롤 아래로 내려면 이미지가 위로 올라가야 하기 때문에 -값을 더해줌
        if (nLeft < 0) nLeft = -ptScrollPos.x; // 이미지가 클라이언트 영역보다 클 때는 스크롤 위치 반영
        if (nTop < 0) nTop = -ptScrollPos.y; // 이미지가 클라이언트 영역보다 클 때는 스크롤 위치 반영
    }

    ptRet.x = nLeft;
    ptRet.y = nTop;

    return ptRet;
}

CSize CImageView::GetZoomImageSize()
{
    CSize szZoomImageSize(0, 0);

    if (m_Image.IsNull())
        return szZoomImageSize;

    szZoomImageSize.cx = static_cast<int>(m_Image.GetWidth() * m_dZoomFactor);
    szZoomImageSize.cy = static_cast<int>(m_Image.GetHeight() * m_dZoomFactor);

    return szZoomImageSize;
}

//ptImg를 화면 중간에 놓고 확대/ 축소가 된다.
void CImageView::ZoomAtImgPoint(CPoint ptImg, double dNewZoomFactor)
{
    CSingleLock lock(&m_csImage, TRUE); // 락 획득

    TRACE(_T("ZoomAtPoint()\n"));

    if (m_Image.IsNull())
        return;

    // 현재 클라이언트 크기
    CRect rcClient;
    GetClientRect(&rcClient);

    // 현재 스크롤 위치
    CPoint ptScrollPos = GetScrollPosition();

    // 현재 줌 기준 화면상의 좌표 (ptImg를 화면 좌표로 변환)
    CPoint ptView;
    ptView.x = LONG(ptImg.x * m_dZoomFactor) - ptScrollPos.x;
    ptView.y = LONG(ptImg.y * m_dZoomFactor) - ptScrollPos.y;

    // 중심을 기준으로 확대/축소하므로
    // 확대 후에 뷰 좌표 중앙이 ptImg 가 되도록 스크롤 위치 계산
    if (dNewZoomFactor < m_dZoomFactorMin) dNewZoomFactor = m_dZoomFactorMin; // 최소 배율 제한
    m_dZoomFactor = dNewZoomFactor; // 새 배율 적용

    // 새 스크롤 위치 계산
    CPoint ptNewScroll;
    ptNewScroll.x = LONG(ptImg.x * m_dZoomFactor) - rcClient.Width() / 2;
    ptNewScroll.y = LONG(ptImg.y * m_dZoomFactor) - rcClient.Height() / 2;

    // 음수 스크롤 방지
    if (ptNewScroll.x < 0) ptNewScroll.x = 0;
    if (ptNewScroll.y < 0) ptNewScroll.y = 0;

    SetRedraw(FALSE);

    // 스크롤 적용 (스크롤바가 없으면 자동으로 0으로 맞춰짐)
    ScrollToPosition(ptNewScroll);

    // 새 TotalSize를 설정 (CScrollView는 이게 중요!)
   /* CSize sizeTotal(
        LONG(m_Image.GetWidth() * m_dZoomFactor),
        LONG(m_Image.GetHeight() * m_dZoomFactor)
    );*/

    SetScrollSizes(MM_TEXT, GetZoomImageSize());

    SetRedraw(TRUE);

    Invalidate(FALSE); // 다시 그리기
}

void CImageView::ZoomAtMousePoint(CPoint ptMouse, double dNewZoomFactor)
{
    if ( m_Image.IsNull())
        return;

	TRACE(_T("ZoomAtMousePoint() : oldZoom %.2f ==> newZoom %.2f\n"), m_dZoomFactor, dNewZoomFactor);
    m_bZoomFit = FALSE;
    m_bZoomOrg = FALSE;

    // 현재 스크롤 위치
    CPoint ptScrollPos = GetScrollPosition();

    // 마우스 좌표를 원본 이미지 좌표로 변환
    CPoint ptImg;
    ptImg.x = LONG((ptMouse.x + ptScrollPos.x) / m_dZoomFactor);
    ptImg.y = LONG((ptMouse.y + ptScrollPos.y) / m_dZoomFactor);

    TRACE(_T("ZoomAtMousePoint() : ImgPoint %d, %d\n"), ptImg.x, ptImg.y);

    // 확대 비율 갱신
    m_dZoomFactor = dNewZoomFactor;

    // 새 전체 크기 설정 (스크롤뷰용)
   /* CSize sizeTotal(
        LONG(m_Image.GetWidth() * m_dZoomFactor),
        LONG(m_Image.GetHeight() * m_dZoomFactor)
    );*/

    // 중간 깜빡임 방지
    SetRedraw(FALSE);

    SetScrollSizes(MM_TEXT, GetZoomImageSize());

    // 확대 후에도 ptImg 가 마우스 위치에 그대로 보이도록 스크롤 위치 계산
    CPoint ptNewScroll;
    ptNewScroll.x = LONG(ptImg.x * m_dZoomFactor) - ptMouse.x;
    ptNewScroll.y = LONG(ptImg.y * m_dZoomFactor) - ptMouse.y;
    
	//Zoom된 이미지의 크기가 클라이언트 영역보다 작을때 보정
    // 클라이언트 영역
    CRect rectClient;
    GetClientRect(&rectClient);

    // 배율 적용된 이미지 크기
    CSize sizeTotal;
    sizeTotal.cx = static_cast<int>(m_Image.GetWidth() * m_dZoomFactor);
    sizeTotal.cy = static_cast<int>(m_Image.GetHeight() * m_dZoomFactor);

    if (sizeTotal.cx < rectClient.Width()) ptNewScroll.x = 0;
    if (sizeTotal.cy < rectClient.Height()) ptNewScroll.y = 0;

    // 음수 스크롤 방지
    if (ptNewScroll.x < 0) ptNewScroll.x = 0;
    if (ptNewScroll.y < 0) ptNewScroll.y = 0;

    ScrollToPosition(ptNewScroll);

    SetRedraw(TRUE);
    Invalidate(FALSE);
}


BOOL CImageView::CropAndSaveBitmap(const CBitmap& bmpSrc, const CRect& inputRect, const CString& outFilename)
{
    if (outFilename.IsEmpty())
        return false;

    // 원본 비트맵 정보 얻기 (Win32 API 사용)
    BITMAP bm = { 0 };
    if (::GetObject((HBITMAP)bmpSrc.m_hObject, sizeof(BITMAP), &bm) == 0)
        return false;

    int srcW = bm.bmWidth;
    int srcH = bm.bmHeight;

    // 크롭 영역을 원본 크기에 맞게 보정
    CRect srcRect(0, 0, srcW, srcH);
    CRect rect = inputRect;
    rect.IntersectRect(rect, srcRect);
    if (rect.IsRectEmpty())
        return false;

    int cropW = rect.Width();
    int cropH = rect.Height();

    // 화면 DC와 메모리 DC 준비
    HDC hdcScreen = ::GetDC(NULL);
    HDC hdcSrc = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hOldSrcBmp = (HBITMAP)::SelectObject(hdcSrc, (HBITMAP)bmpSrc.m_hObject);

    HDC hdcDst = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hBmpDst = ::CreateCompatibleBitmap(hdcScreen, cropW, cropH);
    HBITMAP hOldDstBmp = (HBITMAP)::SelectObject(hdcDst, hBmpDst);

    // 크롭 영역 복사
    BOOL bOK = ::BitBlt(
        hdcDst, 0, 0, cropW, cropH,
        hdcSrc, rect.left, rect.top,
        SRCCOPY
    );

    // DC 원복 & 해제
    ::SelectObject(hdcSrc, hOldSrcBmp);
    ::SelectObject(hdcDst, hOldDstBmp);

    ::DeleteDC(hdcSrc);
    ::DeleteDC(hdcDst);
    ::ReleaseDC(NULL, hdcScreen);

    if (!bOK) {
        ::DeleteObject(hBmpDst);
        return false;
    }

    // 잘라낸 HBITMAP → CImage 로 저장
    CImage img;
    img.Attach(hBmpDst); // 소유권 이전

    HRESULT hr = img.Save(outFilename); // 확장자(.jpg, .png, .bmp 등)에 맞게 저장
    img.Detach();

    if (FAILED(hr)) {
        ::DeleteObject(hBmpDst);
        return false;
    }

    return true;
}

// 마진까지 포함한 Client 좌표 → 이미지 좌표
CPoint CImageView::ClientToImage(CPoint ptClient)
{
    CPoint ptImg;
    CRect rectClient;
    GetClientRect(&rectClient);
    CPoint ptScrollPos = GetScrollPosition();

    int nImageWidth = GetZoomImageSize().cx;
    int nImageHeight = GetZoomImageSize().cy;
    int nLeft = 0;
    int nTop = 0;
    int nZoomedPointX = 0;
    int nZoomedPointY = 0;
    //이미지의 크기가 클라이언트 영역보다 작을 때 중앙 정렬
    if (nImageWidth < rectClient.Width() || nImageHeight < rectClient.Height())
    {
        nLeft = (rectClient.Width() - nImageWidth) / 2;
        nTop = (rectClient.Height() - nImageHeight) / 2;
        if (nLeft < 0) nLeft = -ptScrollPos.x; // 이미지가 클라이언트 영역보다 클 때는 스크롤 위치 반영
        if (nTop < 0) nTop = -ptScrollPos.y;
        ptImg.x = static_cast<int>((ptClient.x - nLeft) / m_dZoomFactor);
        ptImg.y = static_cast<int>((ptClient.y - nTop) / m_dZoomFactor);
    }
    else {
         ptImg.x= static_cast<int>((ptClient.x + ptScrollPos.x) / m_dZoomFactor);
         ptImg.y= static_cast<int>((ptClient.y + ptScrollPos.y) / m_dZoomFactor);
    }
    return ptImg;
}

// 이미지 좌표 → Client 좌표
CPoint CImageView::ImageToClient(CPoint ptImage)
{
    CPoint ptClient;
    CRect rectClient;
    GetClientRect(&rectClient);
    CPoint ptScrollPos = GetScrollPosition();

    CPoint ptOrg = GetImageDispOrg();
    ptClient = ImageToDispArea(ptImage);
    ptClient.x += ptOrg.x;
    ptClient.y += ptOrg.y;

    return ptClient;
}

CPoint CImageView::ImageToDispArea(CPoint ptImage)
{
    CPoint ptDispArea;
    CRect rectClient;
    GetClientRect(&rectClient);
    CPoint ptScrollPos = GetScrollPosition();

    /*ptDispArea.x = static_cast<int>(ptImage.x * m_dZoomFactor - ptScrollPos.x);
    ptDispArea.y = static_cast<int>(ptImage.y * m_dZoomFactor - ptScrollPos.y);*/
    ptDispArea.x = static_cast<int>(ptImage.x * m_dZoomFactor);
    ptDispArea.y = static_cast<int>(ptImage.y * m_dZoomFactor);
    return ptDispArea;
}

void CImageView::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_bLBtnDown = TRUE;
  
    if (!m_bEditMode) {

        const CPoint ptClientToImage = ClientToImage(point);
        const CPoint ptImageToDispArea = ImageToDispArea(ptClientToImage);

        // 좌표를 부모 다이얼로그에 전송
        GetParent()->SendMessage(
            WM_LBUTTONDOWN,
            nFlags,
            MAKELPARAM(ptClientToImage.x, ptClientToImage.y)
        );

        CScrollView::OnLButtonDown(nFlags, point);
        return;
    }

    SetCapture();            // 마우스 캡처 (밖으로 나가도 이벤트 받음)
    m_ptStartImg = ClientToImage(point);
    m_ptEndImg = m_ptStartImg;
    m_rcSelection.SetRectEmpty();

    CScrollView::OnLButtonDown(nFlags, point);
}

void CImageView::OnLButtonUp(UINT nFlags, CPoint point)
{
    m_bLBtnDown = FALSE;
    ReleaseCapture();

    if (!m_bEditMode) {
        const CPoint ptClientToImage = ClientToImage(point);
        const CPoint ptImageToDispArea = ImageToDispArea(ptClientToImage);

        // 좌표를 부모 다이얼로그에 전송
        GetParent()->SendMessage(
            WM_LBUTTONUP,
            nFlags,
            MAKELPARAM(ptClientToImage.x, ptClientToImage.y)
        );

        CScrollView::OnLButtonUp(nFlags, point);
        return;
    }

    m_ptEndImg = ClientToImage(point);
    m_rcSelection.SetRect(m_ptStartImg, m_ptEndImg);
    m_rcSelection.NormalizeRect();

//	Invalidate(TRUE);

    CScrollView::OnLButtonUp(nFlags, point);
}

void CImageView::OnMButtonDown(UINT nFlags, CPoint point)
{
    if (m_dZoomFactorMin >= m_dZoomFactor) return;// ZoomFit 이하 축소상태에서 동작 안함.

    if (!m_bMBtnDown)
    {
        m_bMBtnDown = TRUE;
        m_ptMButton = point;
        SetCapture();
        SetCursor(LoadCursor(NULL, IDC_HAND));
    }
    CScrollView::OnMButtonDown(nFlags, point);
}

void CImageView::OnMButtonUp(UINT nFlags, CPoint point)
{
    m_bMBtnDown = FALSE;
    ReleaseCapture();
    CScrollView::OnMButtonUp(nFlags, point);
}

// 마우스 휠 이벤트에서
BOOL CImageView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (m_Image.IsNull())
		return FALSE;

    if (!(nFlags & MK_CONTROL))
		return FALSE;

    if (zDelta == 0)
		return FALSE;

	if (m_bMBtnDown)
		return FALSE;

    if (m_bLBtnDown)
		return FALSE;
 
    // 스크린 좌표 → 클라이언트 좌표 → 이미지 좌표 변환
    ScreenToClient(&pt);

    CPoint ptScrollPos = GetScrollPosition();
    CPoint ptImg;
    ptImg.x = (pt.x + ptScrollPos.x) / m_dZoomFactor;
    ptImg.y = (pt.y + ptScrollPos.y) / m_dZoomFactor;

    double dNewZoom = (zDelta > 0) ? m_dZoomFactor * 1.2 : m_dZoomFactor / 1.2;

	if (dNewZoom < m_dZoomFactorMin) dNewZoom = m_dZoomFactorMin; // 최소 배율 제한

    ZoomAtMousePoint(pt, dNewZoom);

    return CScrollView::OnMouseWheel(nFlags, zDelta, pt);
}

LRESULT CImageView::OnUpdateDisplay(WPARAM wParam, LPARAM lParam)
{
    UpdateScrollSizes();
    Invalidate(FALSE);

    return LRESULT();
}

int CImageView::GetScrollbarSize(int nBar)
{
    // 윈도우 스타일 확인
    LONG style = GetWindowLong(m_hWnd, GWL_STYLE);

    if (nBar == SB_HORZ) // 가로 스크롤바
    {
        if (style & WS_HSCROLL)
            return GetSystemMetrics(SM_CYHSCROLL); // 가로 스크롤바의 높이
    }
    else if (nBar == SB_VERT) // 세로 스크롤바
    {
        if (style & WS_VSCROLL)
            return GetSystemMetrics(SM_CXVSCROLL); // 세로 스크롤바의 폭
    }

    return 0; // 스크롤바 없음
}


// -------------------- 보조 함수 --------------------

// 체스판(바둑판) 배경
void CImageView::DrawCheckerBackground(CDC& dc, const CRect& rectClient)
{
    const int cellSize = 32;
    COLORREF color1 = RGB(180, 180, 180);
    COLORREF color2 = RGB(220, 220, 220);

    for (int y = 0; y < rectClient.Height(); y += cellSize) {
        for (int x = 0; x < rectClient.Width(); x += cellSize) {
            bool isEven = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            dc.FillSolidRect(x, y, cellSize, cellSize, isEven ? color1 : color2);
        }
    }
}

// 확대/축소 캐시 생성
void CImageView::CreateZoomCache(CDC* pDC)
{
    CSize zoomSize = GetZoomImageSize();

    if (m_zoomCache.GetSafeHandle())
        m_zoomCache.DeleteObject();

    m_zoomCache.CreateCompatibleBitmap(pDC, zoomSize.cx, zoomSize.cy);

    CDC zoomDC;
    zoomDC.CreateCompatibleDC(pDC);
    CBitmap* pOldBmp = zoomDC.SelectObject(&m_zoomCache);

    CRect destRect(0, 0, zoomSize.cx, zoomSize.cy);
    CRect srcRect(0, 0, m_Image.GetWidth(), m_Image.GetHeight());

    SetStretchBltMode(zoomDC.GetSafeHdc(), HALFTONE); // 고품질 확대
    m_Image.Draw(zoomDC.GetSafeHdc(), destRect, srcRect);

    zoomDC.SelectObject(pOldBmp);

    m_zoomCacheSize = zoomSize;
}

// No Image 메시지
void CImageView::DrawNoImageMessage(CDC& dc, const CRect& rectClient)
{
    CString text = _T("No Image");
    CSize textSize = dc.GetTextExtent(text);
    int x = (rectClient.Width() - textSize.cx) / 2;
    int y = (rectClient.Height() - textSize.cy) / 2;
    dc.SetBkMode(TRANSPARENT);
    dc.TextOut(x, y, text);
}