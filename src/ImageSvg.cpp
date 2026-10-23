#include "stdafx.h"
#include "ImageSvg.h"
#include "Context.h"
#include "Details.h"
#include "XMLWriter.h"

CImageSvg::CImageSvg()
{
}

CImageSvg::~CImageSvg()
{
}

void CImageSvg::SaveXML(CXMLWriter &xml)
{
	xml.addChildDataUUencode(m_data.data(), m_data.size());
}

bool CImageSvg::SetCompressedData(unsigned char *data, size_t size)
{
	m_data.assign(data, data + size);
	delete[] data;

	CStringA utf8((const char *) m_data.data(), (int) m_data.size());
	return m_svg.Load(CString(CA2T(utf8, CP_UTF8)));
}

void CImageSvg::Paint(CDC &dc, CRect rect, int)
{
	PaintInvalid(dc, rect);
}

void CImageSvg::PaintVector(CContext &dc, CDPoint a, CDPoint b, const CDetails &details) const
{
	// The renderer wants top-left/bottom-right; the content itself is not rotated
	CDPoint tl(min(a.x, b.x), min(a.y, b.y));
	CDPoint br(max(a.x, b.x), max(a.y, b.y));
	m_svg.Paint(dc, tl, br, details);
}

CString CImageSvg::GetType()
{
	return _T("SVG");
}

CSize CImageSvg::GetSize(CDC &)
{
	double w, h;
	if (!m_svg.GetNaturalSizePixels(w, h))
	{
		return CSize(100, 100);
	}
	return CSize((int) (w + 0.5), (int) (h + 0.5));
}
