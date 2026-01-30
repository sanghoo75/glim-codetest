// ================================
// ThreePointCircleDrawer.h
// ================================
#pragma once
#include <vector>
#include <cmath>

class CThreePointCircleDrawer
{
public:
    CThreePointCircleDrawer();
    ~CThreePointCircleDrawer();

    // 이미지 설정 (grayscale BYTE*)
    void SetImage(BYTE* pImage, int nWidth, int nHeight, int nStride = 0);

    // 점(마커) 반지름 (filled circle)
    void SetPointRadius(int nRadius);

    // 세 점 원 두께 (stroke width)
    void SetCircleThickness(int nThickness);

    // Mouse events (다이얼로그에서 전달)
    void OnLButtonDown(CPoint pt);
    void OnMouseMove(CPoint pt, BOOL bLButtonDown);
    void OnLButtonUp(CPoint pt);

    // overlay redraw
    void Draw();

    void MovePoint(int index, CPoint pt)
    {
        if (index < 0 || index >= (int)m_points.size()) return;
        m_points[index].x = pt.x;
        m_points[index].y = pt.y;
	}

private:
    struct SPoint
    {
        double x;
        double y;
    };

    // 세 점을 지나는 원 계산
    bool ComputeCircle(const SPoint& p1, const SPoint& p2, const SPoint& p3,
        SPoint& center, double& radius);

    // base image 관리
    void BackupBaseImage();
    void ClearOverlay();

    // drawing primitives
    void DrawFilledPoint(const SPoint& pt);
    void DrawCircleStroke(const SPoint& center, double radius);
    void SetPixelSafe(int x, int y, BYTE gray);

private:
    BYTE* m_pImage;      // 출력 이미지 (외부 소유)
    BYTE* m_pBaseImage;  // 최초 이미지 백업 (내부 소유)

    int m_nWidth;
    int m_nHeight;
    int m_nStride;

    std::vector<SPoint> m_points; // 최대 3점
    int m_nDragIndex;             // 드래그 중인 점 index

    int m_nPointRadius;           // 점 반지름
    int m_nHitRadius;             // hit-test 반경
    int m_nCircleThickness;       // 원 두께
};
