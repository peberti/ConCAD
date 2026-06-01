#include "stdafx.h"
#include "SvgTitleBlock.h"
#include "Context.h"
#include "Details.h"
#include "DPoint.h"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg/nanosvg.h"

#include "rapidxml-1.13/rapidxml.hpp"

#include <vector>
#include <string.h>
#include <stdlib.h>

CSvgTitleBlock::CSvgTitleBlock()
	: m_image(nullptr)
{
}

CSvgTitleBlock::~CSvgTitleBlock()
{
	Clear();
}

void CSvgTitleBlock::Clear()
{
	if (m_image)
	{
		nsvgDelete(m_image);
		m_image = nullptr;
	}
	m_svgUtf8.Empty();
}

bool CSvgTitleBlock::Load(const CString& svgText)
{
	Clear();
	if (svgText.IsEmpty()) return false;

	// Convert from CString (UTF-16 under MFC/Unicode) to UTF-8 for NanoSVG.
	CT2A utf8(svgText, CP_UTF8);
	m_svgUtf8 = (LPCSTR)utf8;
	if (m_svgUtf8.IsEmpty()) return false;

	// nsvgParse mutates the input buffer; hand it its own copy.
	const int len = m_svgUtf8.GetLength();
	std::vector<char> buf(len + 1);
	memcpy(buf.data(), (LPCSTR)m_svgUtf8, len);
	buf[len] = '\0';

	m_image = nsvgParse(buf.data(), "px", 96.0f);
	if (!m_image || m_image->width <= 0.0f || m_image->height <= 0.0f)
	{
		Clear();
		return false;
	}
	return true;
}

//=========================================================================
namespace {

struct ViewBoxXform
{
	double scale;
	double offsetX;
	double offsetY;

	CDPoint Map(double vx, double vy) const
	{
		return CDPoint(vx * scale + offsetX, vy * scale + offsetY);
	}
	double MapDist(double v) const { return v * scale; }
};

ViewBoxXform ComputeXform(double svgW, double svgH, CDPoint tl, CDPoint br)
{
	const double targetW = br.x - tl.x;
	const double targetH = br.y - tl.y;
	const double sx = (svgW > 0.0) ? targetW / svgW : 1.0;
	const double sy = (svgH > 0.0) ? targetH / svgH : 1.0;
	const double s = (sx < sy) ? sx : sy;
	const double w = svgW * s;
	const double h = svgH * s;
	ViewBoxXform x;
	x.scale = s;
	x.offsetX = tl.x + (targetW - w) * 0.5;
	x.offsetY = tl.y + (targetH - h) * 0.5;
	return x;
}

inline COLORREF NsvgColorToCOLORREF(unsigned int rgba)
{
	// NanoSVG packs as: byte0=R, byte1=G, byte2=B, byte3=A.
	const BYTE r = (BYTE)((rgba) & 0xff);
	const BYTE g = (BYTE)((rgba >> 8) & 0xff);
	const BYTE b = (BYTE)((rgba >> 16) & 0xff);
	return RGB(r, g, b);
}

inline POINT MapToDevice(double svgX, double svgY,
                         const ViewBoxXform& xf, const Transform& trans)
{
	CPoint q = trans.Scale(xf.Map(svgX, svgY));
	POINT pt = { q.x, q.y };
	return pt;
}

void EmitShapePath(CDC& dc, const NSVGshape* shape,
                   const ViewBoxXform& xf, const Transform& trans)
{
	for (const NSVGpath* p = shape->paths; p; p = p->next)
	{
		if (p->npts < 4) continue;

		const POINT start = MapToDevice(p->pts[0], p->pts[1], xf, trans);
		dc.MoveTo(start.x, start.y);

		const int nBez = (p->npts - 1) / 3;
		if (nBez <= 0) continue;

		std::vector<POINT> ctrl;
		ctrl.reserve(3 * nBez);
		for (int i = 0; i < nBez; ++i)
		{
			const float* q = p->pts + 6 * i;
			ctrl.push_back(MapToDevice(q[2], q[3], xf, trans));
			ctrl.push_back(MapToDevice(q[4], q[5], xf, trans));
			ctrl.push_back(MapToDevice(q[6], q[7], xf, trans));
		}
		dc.PolyBezierTo(ctrl.data(), (int)ctrl.size());

		if (p->closed) dc.CloseFigure();
	}
}

void RenderShape(CContext& cdc, const NSVGshape* shape,
                 const ViewBoxXform& xf)
{
	if (!shape) return;
	if ((shape->flags & NSVG_FLAGS_VISIBLE) == 0) return;

	const bool hasFill   = (shape->fill.type   == NSVG_PAINT_COLOR);
	const bool hasStroke = (shape->stroke.type == NSVG_PAINT_COLOR);
	if (!hasFill && !hasStroke) return;
	// Gradients/patterns: skipped silently for this v1.

	if (hasStroke)
	{
		int sw = (int)(xf.MapDist((double)shape->strokeWidth) + 0.5);
		if (sw < 1) sw = 1;
		cdc.SelectPen(PS_SOLID, sw, (LONG)NsvgColorToCOLORREF(shape->stroke.color));
	}
	if (hasFill)
	{
		cdc.SelectBrush(NsvgColorToCOLORREF(shape->fill.color));
	}

	CDC* pDC = cdc.GetDC();
	if (!pDC) return;

	const Transform& trans = cdc.GetTransform();

	pDC->BeginPath();
	EmitShapePath(*pDC, shape, xf, trans);
	pDC->EndPath();

	const int prevMode = pDC->SetPolyFillMode(
		(shape->fillRule == NSVG_FILLRULE_EVENODD) ? ALTERNATE : WINDING);

	if (hasFill && hasStroke)      pDC->StrokeAndFillPath();
	else if (hasFill)              pDC->FillPath();
	else                            pDC->StrokePath();

	pDC->SetPolyFillMode(prevMode);
}

//=========================================================================
// <text> rendering via rapidxml.  We ignore transform inheritance — author
// title-block <text> with absolute x/y in the viewBox.

COLORREF ParseHexColor(const char* v, COLORREF fallback)
{
	if (!v || *v != '#') return fallback;
	const size_t n = strlen(v);
	if (n == 4)
	{
		auto hex = [](char c) -> int {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return 10 + c - 'a';
			if (c >= 'A' && c <= 'F') return 10 + c - 'A';
			return -1;
		};
		int r = hex(v[1]), g = hex(v[2]), b = hex(v[3]);
		if (r < 0 || g < 0 || b < 0) return fallback;
		return RGB(r * 17, g * 17, b * 17);
	}
	if (n == 7)
	{
		char* end = nullptr;
		unsigned long rgb = strtoul(v + 1, &end, 16);
		if (end == v + 1) return fallback;
		return RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
	}
	return fallback;
}

void RenderOneText(rapidxml::xml_node<>* node, CContext& dc,
                   const ViewBoxXform& xf, const CDetails& details)
{
	if (!node) return;

	double x = 0.0, y = 0.0, fontSize = 12.0;
	COLORREF fill = RGB(0, 0, 0);
	UINT align = TA_LEFT | TA_BASELINE | TA_NOUPDATECP;
	CStringA fontFamily;

	for (rapidxml::xml_attribute<>* a = node->first_attribute(); a; a = a->next_attribute())
	{
		const char* name = a->name();
		const char* val  = a->value();
		if (!name || !val) continue;
		if      (strcmp(name, "x") == 0)            x = atof(val);
		else if (strcmp(name, "y") == 0)            y = atof(val);
		else if (strcmp(name, "font-size") == 0)    fontSize = atof(val);
		else if (strcmp(name, "font-family") == 0)  fontFamily = val;
		else if (strcmp(name, "fill") == 0)         fill = ParseHexColor(val, fill);
		else if (strcmp(name, "text-anchor") == 0)
		{
			if      (strcmp(val, "middle") == 0) align = TA_CENTER | TA_BASELINE | TA_NOUPDATECP;
			else if (strcmp(val, "end")    == 0) align = TA_RIGHT  | TA_BASELINE | TA_NOUPDATECP;
		}
	}

	// Collect text content: direct text nodes + nested <tspan> text.
	CStringA bodyA;
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling())
	{
		if (c->type() == rapidxml::node_data)
		{
			bodyA += c->value();
		}
		else if (c->type() == rapidxml::node_element)
		{
			const char* cn = c->name();
			if (cn && strcmp(cn, "tspan") == 0)
			{
				for (rapidxml::xml_node<>* d = c->first_node(); d; d = d->next_sibling())
					if (d->type() == rapidxml::node_data && d->value())
						bodyA += d->value();
			}
		}
	}
	if (bodyA.IsEmpty())
	{
		const char* v = node->value();
		if (v && *v) bodyA = v;
	}
	if (bodyA.IsEmpty()) return;

	CA2T body(bodyA, CP_UTF8);
	CString text = details.Resolve(CString((LPCTSTR)body));

	LOGFONT lf;
	memset(&lf, 0, sizeof(lf));
	lf.lfHeight = -(LONG)(fontSize + 0.5);   // negative = "em-height"; CContext applies its own scaling
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = DEFAULT_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	if (!fontFamily.IsEmpty())
	{
		CA2T fname(fontFamily, CP_UTF8);
		_tcsncpy_s(lf.lfFaceName, (LPCTSTR)fname, LF_FACESIZE - 1);
	}
	// "3" = no rotation, matching the existing built-in title block.
	dc.SelectFont(lf, 3);
	dc.SetTextColor((LONG)fill);
	dc.SetTextAlign(align);
	dc.SetBkMode(TRANSPARENT);

	CDPoint pos = xf.Map(x, y);
	dc.TextOut(pos.x, pos.y, text);
}

void RenderTextsRecursive(rapidxml::xml_node<>* node, CContext& dc,
                          const ViewBoxXform& xf, const CDetails& details)
{
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling())
	{
		if (c->type() != rapidxml::node_element) continue;
		const char* n = c->name();
		if (!n) continue;
		if (strcmp(n, "text") == 0)
			RenderOneText(c, dc, xf, details);
		else
			RenderTextsRecursive(c, dc, xf, details);
	}
}

} // namespace

//=========================================================================
void CSvgTitleBlock::Paint(CContext& dc, CDPoint tl, CDPoint br,
                           const CDetails& details) const
{
	if (!m_image) return;

	const ViewBoxXform xf = ComputeXform(
		(double)m_image->width, (double)m_image->height, tl, br);

	for (const NSVGshape* shape = m_image->shapes; shape; shape = shape->next)
		RenderShape(dc, shape, xf);

	if (m_svgUtf8.IsEmpty()) return;

	std::vector<char> buf(m_svgUtf8.GetLength() + 1);
	memcpy(buf.data(), (LPCSTR)m_svgUtf8, m_svgUtf8.GetLength());
	buf[m_svgUtf8.GetLength()] = '\0';

	try
	{
		rapidxml::xml_document<> doc;
		doc.parse<rapidxml::parse_default>(buf.data());
		rapidxml::xml_node<>* root = doc.first_node("svg");
		if (root) RenderTextsRecursive(root, dc, xf, details);
	}
	catch (const rapidxml::parse_error&)
	{
		// Shape pass already happened; ignore text-pass parse error.
	}
}
