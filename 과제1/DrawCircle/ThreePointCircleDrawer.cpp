// ================================
// ThreePointCircleDrawer.cpp
// ================================
#include "pch.h"
#include "ThreePointCircleDrawer.h"
#include "define.h"


CThreePointCircleDrawer::CThreePointCircleDrawer()
{
    m_pImage = nullptr;
    m_pBaseImage = nullptr;
    m_nWidth = m_nHeight = m_nStride = 0;
    m_nDragIndex = -1;
    m_nPointRadius = 4;
    m_nHitRadius = m_nPointRadius + 3;
    m_nCircleThickness = 1;
}

CThreePointCircleDrawer::~CThreePointCircleDrawer()
{
    delete[] m_pBaseImage;
}

void CThreePointCircleDrawer::SetImage(BYTE* pImage, int nWidth, int nHeight, int nStride)
{
    m_pImage = pImage;
    m_nWidth = nWidth;
    m_nHeight = nHeight;
    m_nStride = (nStride == 0) ? nWidth : nStride;

    BackupBaseImage();
}

void CThreePointCircleDrawer::SetPointRadius(int nRadius)
{
    m_nPointRadius = (nRadius < 1) ? 1 : nRadius;
    m_nHitRadius = m_nPointRadius + 3;
}

void CThreePointCircleDrawer::SetCircleThickness(int nThickness)
{
    m_nCircleThickness = (nThickness < 1) ? 1 : nThickness;
}

void CThreePointCircleDrawer::BackupBaseImage()
{
    if (!m_pImage) return;

    delete[] m_pBaseImage;
    m_pBaseImage = new BYTE[m_nStride * m_nHeight];
    memcpy(m_pBaseImage, m_pImage, m_nStride * m_nHeight);
}

void CThreePointCircleDrawer::ClearOverlay()
{
    if (!m_pImage || !m_pBaseImage) return;
    memcpy(m_pImage, m_pBaseImage, m_nStride * m_nHeight);
}

void CThreePointCircleDrawer::OnLButtonDown(CPoint pt)
{
    for (size_t i = 0; i < m_points.size(); ++i)
    {
        double dx = m_points[i].x - pt.x;
        double dy = m_points[i].y - pt.y;
        if (dx * dx + dy * dy <= m_nHitRadius * m_nHitRadius)
        {
            m_nDragIndex = (int)i;
            return;
        }
    }

    if (m_points.size() < 3)
        m_points.push_back({ (double)pt.x, (double)pt.y });
}

void CThreePointCircleDrawer::OnMouseMove(CPoint pt, BOOL bLButtonDown)
{
    if (bLButtonDown && m_nDragIndex >= 0)
    {
        m_points[m_nDragIndex].x = pt.x;
        m_points[m_nDragIndex].y = pt.y;
    }
}

void CThreePointCircleDrawer::OnLButtonUp(CPoint /*pt*/)
{
    m_nDragIndex = -1;
}

void CThreePointCircleDrawer::Draw()
{
    if (!m_pImage) return;

    ClearOverlay();

    for (const auto& p : m_points)
        DrawFilledPoint(p);

    


    if (m_points.size() == 3)
    {
        SPoint center;
        double radius;
        if (ComputeCircle(m_points[0], m_points[1], m_points[2], center, radius))
            DrawCircleStroke(center, radius);
    }
}

bool CThreePointCircleDrawer::ComputeCircle(const SPoint& p1, const SPoint& p2, const SPoint& p3,
    SPoint& center, double& radius)
{
    double a = p2.x - p1.x;
    double b = p2.y - p1.y;
    double c = p3.x - p1.x;
    double d = p3.y - p1.y;

    double e = a * (p1.x + p2.x) + b * (p1.y + p2.y);
    double f = c * (p1.x + p3.x) + d * (p1.y + p3.y);
    double g = 2.0 * (a * (p3.y - p2.y) - b * (p3.x - p2.x));

    if (fabs(g) < 1e-6)
        return false;

    center.x = (d * e - b * f) / g;
    center.y = (a * f - c * e) / g;

    radius = sqrt((center.x - p1.x) * (center.x - p1.x) +
        (center.y - p1.y) * (center.y - p1.y));
    return true;
}

void CThreePointCircleDrawer::DrawFilledPoint(const SPoint& pt)
{
    for (int dy = -m_nPointRadius; dy <= m_nPointRadius; ++dy)
    {
        for (int dx = -m_nPointRadius; dx <= m_nPointRadius; ++dx)
        {
            if (dx * dx + dy * dy <= m_nPointRadius * m_nPointRadius)
                SetPixelSafe((int)(pt.x + dx), (int)(pt.y + dy), 0x00);
        }
    }
}

void CThreePointCircleDrawer::DrawCircleStroke(const SPoint& center, double radius)
{
    if (radius < 1.0) return;

    int half = m_nCircleThickness / 2;
    int steps = (int)(radius * 2.0 * M_PI / 2.0);
    if (steps < 180) steps = 180;
    if (steps > 1440) steps = 1440;

    for (int t = -half; t <= half; ++t)
    {
        double r = radius + t;
        if (r <= 0.5) continue;

        for (int i = 0; i < steps; ++i)
        {
            double rad = 2.0 * M_PI * i / steps;
            int x = (int)(center.x + r * cos(rad));
            int y = (int)(center.y + r * sin(rad));
            SetPixelSafe(x, y, 0x00);
        }
    }
}

void CThreePointCircleDrawer::SetPixelSafe(int x, int y, BYTE gray)
{
    if (x < 0 || y < 0 || x >= m_nWidth || y >= m_nHeight)
        return;
    m_pImage[y * m_nStride + x] = gray;
}
