#include "stdafx.h"
#include "SvgTitleBlock.h"
#include "Context.h"
#include "Details.h"
#include "DPoint.h"
#include "ConCad.h"
#include <shlobj.h>
#include <algorithm>
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")

#define NANOSVG_IMPLEMENTATION
#include "nanosvg/nanosvg.h"

#include "rapidxml-1.13/rapidxml.hpp"

#include <vector>
#include <map>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

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

bool CSvgTitleBlock::GetNaturalSizePixels(double& width_px, double& height_px) const
{
	if (!m_image) { width_px = height_px = 0.0; return false; }
	width_px  = (double)m_image->width;
	height_px = (double)m_image->height;
	return (width_px > 0.0 && height_px > 0.0);
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
// <text> rendering via rapidxml.

// 2D affine transform.  Column-vector convention: [a c e; b d f; 0 0 1].
struct Affine
{
	double a, b, c, d, e, f;
	Affine() : a(1), b(0), c(0), d(1), e(0), f(0) {}
	void Apply(double& x, double& y) const
	{
		const double X = a * x + c * y + e;
		const double Y = b * x + d * y + f;
		x = X; y = Y;
	}
	// Approximate uniform scale for sizing things like fonts/strokes.
	double UniformScale() const
	{
		const double det = a * d - b * c;
		return sqrt(det >= 0.0 ? det : -det);
	}
};

Affine ConcatAffine(const Affine& A, const Affine& B)
{
	Affine r;
	r.a = A.a * B.a + A.c * B.b;
	r.b = A.b * B.a + A.d * B.b;
	r.c = A.a * B.c + A.c * B.d;
	r.d = A.b * B.c + A.d * B.d;
	r.e = A.a * B.e + A.c * B.f + A.e;
	r.f = A.b * B.e + A.d * B.f + A.f;
	return r;
}

// Parse an SVG transform="..." attribute value.  Supports translate / scale
// / matrix / rotate, possibly chained.
Affine ParseTransformAttr(const char* s)
{
	Affine result;
	if (!s) return result;
	const char* p = s;
	while (*p)
	{
		while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
		if (!*p) break;

		const char* nameStart = p;
		while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') ++p;
		if (p == nameStart || *p != '(') break;
		const size_t nameLen = (size_t)(p - nameStart);
		++p; // skip '('

		double nums[6] = { 0, 0, 0, 0, 0, 0 };
		int n = 0;
		while (*p && *p != ')' && n < 6)
		{
			while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
			if (!*p || *p == ')') break;
			char* eend = nullptr;
			const double v = strtod(p, &eend);
			if (eend == p) break;
			nums[n++] = v;
			p = eend;
		}
		while (*p && *p != ')') ++p;
		if (*p == ')') ++p;

		char name[16] = { 0 };
		if (nameLen < sizeof(name)) memcpy(name, nameStart, nameLen);

		Affine t;
		if (strcmp(name, "translate") == 0)
		{
			t.e = nums[0];
			t.f = (n >= 2) ? nums[1] : 0.0;
		}
		else if (strcmp(name, "scale") == 0)
		{
			t.a = nums[0];
			t.d = (n >= 2) ? nums[1] : nums[0];
		}
		else if (strcmp(name, "matrix") == 0 && n >= 6)
		{
			t.a = nums[0]; t.b = nums[1]; t.c = nums[2];
			t.d = nums[3]; t.e = nums[4]; t.f = nums[5];
		}
		else if (strcmp(name, "rotate") == 0)
		{
			const double pi = 3.14159265358979323846;
			const double rad = nums[0] * pi / 180.0;
			const double cs = cos(rad), sn = sin(rad);
			t.a = cs; t.b = sn; t.c = -sn; t.d = cs;
			if (n >= 3)
			{
				const double cx = nums[1], cy = nums[2];
				Affine tr1; tr1.e = -cx; tr1.f = -cy;
				Affine tr2; tr2.e =  cx; tr2.f =  cy;
				t = ConcatAffine(tr2, ConcatAffine(t, tr1));
			}
		}
		// unknown transform name: identity, harmless.

		result = ConcatAffine(result, t);
	}
	return result;
}

COLORREF ParseHexColor(const char* v, COLORREF fallback);

// Pull font-size, font-family, fill, text-anchor out of an SVG/CSS-style
// "name:value;name:value" string.  Inkscape stores presentation properties
// here rather than as discrete XML attributes, so without this the parser
// silently misses font-size (and the rendered text comes out at the default
// 12-unit fallback, which is "way too big" for an Inkscape title block).
struct CssTextProps
{
	double fontSize;     bool hasFontSize;
	CStringA fontFamily; bool hasFontFamily;
	COLORREF fill;       bool hasFill;
	UINT align;          bool hasAlign;
	int weight;          bool hasWeight;    // LOGFONT weight (FW_NORMAL, FW_BOLD, ...)
	bool italic;         bool hasItalic;
	CssTextProps()
		: fontSize(0.0), hasFontSize(false), fill(0), hasFill(false),
		  align(TA_LEFT | TA_BASELINE | TA_NOUPDATECP), hasAlign(false),
		  weight(FW_NORMAL), hasWeight(false), italic(false), hasItalic(false) {}
};

// CSS font-weight -> LOGFONT weight: "bold"/"bolder" = 700, "normal"/
// "lighter" = 400, or a number 100..900.
int ParseFontWeight(const char* val)
{
	while (*val == ' ' || *val == '\t') ++val;
	if (_strnicmp(val, "bold", 4) == 0) return FW_BOLD;   // also "bolder"
	if (_strnicmp(val, "normal", 6) == 0 || _strnicmp(val, "lighter", 7) == 0) return FW_NORMAL;
	int w = atoi(val);
	return (w >= 100 && w <= 900) ? w : FW_NORMAL;
}

// CSS font-style: "italic" and "oblique" render italic.
bool ParseFontItalic(const char* val)
{
	while (*val == ' ' || *val == '\t') ++val;
	return _strnicmp(val, "italic", 6) == 0 || _strnicmp(val, "oblique", 7) == 0;
}

void ParseCssTextStyle(const char* style, CssTextProps& out)
{
	if (!style) return;
	const char* p = style;
	while (*p)
	{
		while (*p == ' ' || *p == ';' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
		if (!*p) break;

		const char* nameStart = p;
		while (*p && *p != ':' && *p != ';') ++p;
		if (*p != ':') { while (*p && *p != ';') ++p; continue; }
		const size_t nameLen = (size_t)(p - nameStart);
		++p;  // skip ':'

		while (*p == ' ' || *p == '\t') ++p;
		const char* valStart = p;
		while (*p && *p != ';') ++p;
		size_t valLen = (size_t)(p - valStart);
		while (valLen > 0 && (valStart[valLen - 1] == ' ' || valStart[valLen - 1] == '\t')) --valLen;

		const auto matches = [&](const char* key) -> bool {
			const size_t kl = strlen(key);
			return kl == nameLen && strncmp(nameStart, key, kl) == 0;
		};

		if (matches("font-size"))
		{
			char buf[64] = { 0 };
			memcpy(buf, valStart, valLen < sizeof(buf) - 1 ? valLen : sizeof(buf) - 1);
			out.fontSize = atof(buf);  // atof stops at non-numeric (handles "2.82px")
			out.hasFontSize = true;
		}
		else if (matches("font-family"))
		{
			size_t end = valLen;
			for (size_t i = 0; i < valLen; ++i) if (valStart[i] == ',') { end = i; break; }
			while (end > 0 && (valStart[end - 1] == ' ' || valStart[end - 1] == '\t')) --end;
			CStringA fam(valStart, (int)end);
			if (!fam.IsEmpty() && (fam[0] == '\'' || fam[0] == '"')) fam.Delete(0, 1);
			if (!fam.IsEmpty()) {
				const int last = fam.GetLength() - 1;
				if (fam[last] == '\'' || fam[last] == '"') fam.Delete(last, 1);
			}
			if (!fam.IsEmpty()) { out.fontFamily = fam; out.hasFontFamily = true; }
		}
		else if (matches("fill"))
		{
			char buf[64] = { 0 };
			memcpy(buf, valStart, valLen < sizeof(buf) - 1 ? valLen : sizeof(buf) - 1);
			if (buf[0] == '#') { out.fill = ParseHexColor(buf, out.fill); out.hasFill = true; }
		}
		else if (matches("font-weight") || matches("font-style"))
		{
			char buf[32] = { 0 };
			memcpy(buf, valStart, valLen < sizeof(buf) - 1 ? valLen : sizeof(buf) - 1);
			if (matches("font-weight")) { out.weight = ParseFontWeight(buf); out.hasWeight = true; }
			else                        { out.italic = ParseFontItalic(buf); out.hasItalic = true; }
		}
		else if (matches("text-anchor"))
		{
			char buf[16] = { 0 };
			memcpy(buf, valStart, valLen < sizeof(buf) - 1 ? valLen : sizeof(buf) - 1);
			if      (strcmp(buf, "middle") == 0) out.align = TA_CENTER | TA_BASELINE | TA_NOUPDATECP;
			else if (strcmp(buf, "end")    == 0) out.align = TA_RIGHT  | TA_BASELINE | TA_NOUPDATECP;
			else                                  out.align = TA_LEFT   | TA_BASELINE | TA_NOUPDATECP;
			out.hasAlign = true;
		}

		if (*p == ';') ++p;
	}
}

// Merge "set" properties from src onto dst (src wins where it has a value).
void MergeTextProps(CssTextProps& dst, const CssTextProps& src)
{
	if (src.hasFontSize)   { dst.fontSize = src.fontSize;     dst.hasFontSize = true; }
	if (src.hasFontFamily) { dst.fontFamily = src.fontFamily; dst.hasFontFamily = true; }
	if (src.hasFill)       { dst.fill = src.fill;             dst.hasFill = true; }
	if (src.hasAlign)      { dst.align = src.align;           dst.hasAlign = true; }
	if (src.hasWeight)     { dst.weight = src.weight;         dst.hasWeight = true; }
	if (src.hasItalic)     { dst.italic = src.italic;         dst.hasItalic = true; }
}

// class-name -> resolved text properties, parsed from <style> rules.
typedef std::map<std::string, CssTextProps> CssRuleMap;

// Parse a CSS stylesheet body (the text of a <style> element).  Handles
// ".name { decls }" and comma-separated selectors; comments are skipped.
// Inkscape/Illustrator put title-block font sizes here as class rules, so
// without this every class-styled <text> falls back to the default size.
void ParseStyleSheet(const char* css, CssRuleMap& rules)
{
	if (!css) return;
	const char* p = css;
	while (*p)
	{
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
		if (p[0] == '/' && p[1] == '*')           // skip /* comment */
		{
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/')) ++p;
			if (*p) p += 2;
			continue;
		}
		if (!*p) break;

		const char* selStart = p;
		while (*p && *p != '{') ++p;
		if (*p != '{') break;
		const char* selEnd = p;
		++p;                                       // skip '{'
		const char* blockStart = p;
		while (*p && *p != '}') ++p;
		const char* blockEnd = p;
		if (*p == '}') ++p;

		std::string block(blockStart, (size_t)(blockEnd - blockStart));
		CssTextProps props;
		ParseCssTextStyle(block.c_str(), props);

		// Split the selector list on ',' and store under each ".class" token.
		std::string sel(selStart, (size_t)(selEnd - selStart));
		size_t i = 0;
		while (i <= sel.size())
		{
			size_t comma = sel.find(',', i);
			std::string one = sel.substr(i, comma == std::string::npos ? std::string::npos : comma - i);
			size_t dot = one.find('.');
			if (dot != std::string::npos)
			{
				size_t s = dot + 1, e = s;
				while (e < one.size() && (isalnum((unsigned char)one[e]) || one[e] == '-' || one[e] == '_')) ++e;
				if (e > s) MergeTextProps(rules[one.substr(s, e - s)], props);
			}
			if (comma == std::string::npos) break;
			i = comma + 1;
		}
	}
}

// Apply the rules referenced by node's "class" attribute onto out.
void ApplyClassRules(rapidxml::xml_node<>* node, const CssRuleMap& rules, CssTextProps& out)
{
	if (rules.empty() || !node) return;
	rapidxml::xml_attribute<>* cls = node->first_attribute("class");
	if (!cls || !cls->value()) return;
	const char* p = cls->value();
	while (*p)
	{
		while (*p == ' ' || *p == '\t') ++p;
		const char* s = p;
		while (*p && *p != ' ' && *p != '\t') ++p;
		if (p > s)
		{
			CssRuleMap::const_iterator it = rules.find(std::string(s, (size_t)(p - s)));
			if (it != rules.end()) MergeTextProps(out, it->second);
		}
	}
}

// Collect and parse every <style> element in the tree (they may sit under
// <defs>), accumulating their class rules.
void CollectStyleSheet(rapidxml::xml_node<>* node, CssRuleMap& rules)
{
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling())
	{
		if (c->type() != rapidxml::node_element) continue;
		const char* n = c->name();
		if (n && strcmp(n, "style") == 0)
		{
			CStringA css;
			for (rapidxml::xml_node<>* t = c->first_node(); t; t = t->next_sibling())
				if ((t->type() == rapidxml::node_data || t->type() == rapidxml::node_cdata) && t->value())
					css += t->value();
			if (css.IsEmpty() && c->value()) css = c->value();
			ParseStyleSheet(css, rules);
		}
		else
		{
			CollectStyleSheet(c, rules);
		}
	}
}

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

// Resolve a node's font-size/family/fill/anchor in CSS precedence order:
// presentation attributes (lowest) < class rules < inline style (highest).
// Also reads x/y when xOut/yOut are provided (the <text> node only).
void ResolveNodeTextProps(rapidxml::xml_node<>* node, const CssRuleMap& rules,
                          CssTextProps& eff, double* xOut, double* yOut)
{
	for (rapidxml::xml_attribute<>* a = node->first_attribute(); a; a = a->next_attribute())
	{
		const char* name = a->name();
		const char* val  = a->value();
		if (!name || !val) continue;
		if      (xOut && strcmp(name, "x") == 0)    *xOut = atof(val);
		else if (yOut && strcmp(name, "y") == 0)    *yOut = atof(val);
		else if (strcmp(name, "font-size") == 0)  { eff.fontSize = atof(val); eff.hasFontSize = true; }
		else if (strcmp(name, "font-family") == 0){ eff.fontFamily = val; eff.hasFontFamily = true; }
		else if (strcmp(name, "fill") == 0)       { eff.fill = ParseHexColor(val, eff.fill); eff.hasFill = true; }
		else if (strcmp(name, "font-weight") == 0){ eff.weight = ParseFontWeight(val); eff.hasWeight = true; }
		else if (strcmp(name, "font-style") == 0) { eff.italic = ParseFontItalic(val); eff.hasItalic = true; }
		else if (strcmp(name, "text-anchor") == 0)
		{
			if      (strcmp(val, "middle") == 0) eff.align = TA_CENTER | TA_BASELINE | TA_NOUPDATECP;
			else if (strcmp(val, "end")    == 0) eff.align = TA_RIGHT  | TA_BASELINE | TA_NOUPDATECP;
			else                                  eff.align = TA_LEFT   | TA_BASELINE | TA_NOUPDATECP;
			eff.hasAlign = true;
		}
	}

	// class="..." rules from the <style> sheet override presentation attrs.
	ApplyClassRules(node, rules, eff);

	// Inline style="..." overrides everything above.
	if (rapidxml::xml_attribute<>* st = node->first_attribute("style"))
	{
		CssTextProps css;
		ParseCssTextStyle(st->value(), css);
		MergeTextProps(eff, css);
	}
}

// Split text into display lines.  '\n' always breaks; when maxWidth > 0,
// lines are also word-wrapped so none is wider than maxWidth (same units as
// fontSize).  A single word wider than maxWidth gets a line of its own.
// Widths are measured with GDI on a reference-size font, so the result does
// not depend on the zoom level.
std::vector<CString> WrapText(const CString& textIn, double maxWidth, double fontSize, const CString& family,
                              int weight = FW_NORMAL, bool italic = false)
{
	CString text = textIn;
	text.Replace(_T("\r\n"), _T("\n"));

	std::vector<CString> lines;
	if (maxWidth <= 0.0 || fontSize <= 0.0)
	{
		int start = 0;
		for (;;)
		{
			int nl = text.Find(_T('\n'), start);
			lines.push_back(nl < 0 ? text.Mid(start) : text.Mid(start, nl - start));
			if (nl < 0) break;
			start = nl + 1;
		}
		return lines;
	}

	// First family of a CSS list, without quotes; Arial when unset.
	CString face = family;
	int comma = face.Find(_T(','));
	if (comma >= 0) face = face.Left(comma);
	face.Trim(_T(" \t'\""));
	if (face.IsEmpty()) face = _T("Arial");

	const int kRef = 1000;   // reference em size in pixels
	HDC hdc = CreateCompatibleDC(NULL);
	LOGFONT lf;
	memset(&lf, 0, sizeof(lf));
	lf.lfHeight = -kRef;
	lf.lfWeight = weight;
	lf.lfItalic = italic ? TRUE : FALSE;
	lf.lfCharSet = DEFAULT_CHARSET;
	_tcsncpy_s(lf.lfFaceName, face, LF_FACESIZE - 1);
	HFONT hFont = CreateFontIndirect(&lf);
	HGDIOBJ hOld = SelectObject(hdc, hFont);
	const double scale = fontSize / kRef;
	auto width = [&](const CString& s) -> double
	{
		SIZE sz = { 0, 0 };
		GetTextExtentPoint32(hdc, s, s.GetLength(), &sz);
		return sz.cx * scale;
	};

	int start = 0;
	for (;;)
	{
		int nl = text.Find(_T('\n'), start);
		CString para = nl < 0 ? text.Mid(start) : text.Mid(start, nl - start);
		CString line;
		int pos = 0;
		bool any = false;
		for (;;)
		{
			CString word = para.Tokenize(_T(" "), pos);
			if (pos < 0) break;
			any = true;
			CString candidate = line.IsEmpty() ? word : line + _T(" ") + word;
			if (!line.IsEmpty() && width(candidate) > maxWidth)
			{
				lines.push_back(line);
				line = word;
			}
			else
			{
				line = candidate;
			}
		}
		lines.push_back(any ? line : CString());
		if (nl < 0) break;
		start = nl + 1;
	}

	SelectObject(hdc, hOld);
	DeleteObject(hFont);
	DeleteDC(hdc);
	return lines;
}

void RenderOneText(rapidxml::xml_node<>* node, CContext& dc,
                   const ViewBoxXform& xf, const CDetails& details,
                   const Affine& parentTransform, const CssRuleMap& rules)
{
	if (!node) return;

	double x = 0.0, y = 0.0;

	// Resolve on the <text> node, then refine with the first <tspan> child
	// (more specific): Inkscape/Illustrator often carry the size on the tspan
	// or on a class referenced by it.  Each level only overrides where set.
	CssTextProps eff;
	ResolveNodeTextProps(node, rules, eff, &x, &y);
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling())
	{
		if (c->type() != rapidxml::node_element) continue;
		const char* cn = c->name();
		if (!cn || strcmp(cn, "tspan") != 0) continue;
		ResolveNodeTextProps(c, rules, eff, nullptr, nullptr);
		break;
	}

	// Without an explicit size anywhere, a 12-unit default scaled through
	// viewBoxToPixel renders absurdly large, so keep the conservative 12.
	double fontSize = eff.hasFontSize ? eff.fontSize : 12.0;
	COLORREF fill = eff.hasFill ? eff.fill : RGB(0, 0, 0);
	UINT align = eff.align;
	CStringA fontFamily = eff.fontFamily;

	// Collect text content recursively: Inkscape nests text inside
	// <text><tspan><tspan>Title</tspan></tspan></text>, so a single-level
	// walk would miss the actual string.
	struct Collect {
		static void Walk(rapidxml::xml_node<>* n, CStringA& out)
		{
			if (!n) return;
			for (rapidxml::xml_node<>* c = n->first_node(); c; c = c->next_sibling())
			{
				if (c->type() == rapidxml::node_data)
				{
					if (c->value()) out += c->value();
				}
				else if (c->type() == rapidxml::node_element)
				{
					const char* cn = c->name();
					if (cn && strcmp(cn, "tspan") == 0) Walk(c, out);
				}
			}
		}
	};
	CStringA bodyA;
	Collect::Walk(node, bodyA);
	if (bodyA.IsEmpty())
	{
		const char* v = node->value();
		if (v && *v) bodyA = v;
	}
	if (bodyA.IsEmpty()) return;

	CA2T body(bodyA, CP_UTF8);
	CString text = details.Resolve(CString((LPCTSTR)body));

	// Accumulate this element's own transform, then apply to (x, y) and font.
	Affine textTransform = parentTransform;
	if (rapidxml::xml_attribute<>* tr = node->first_attribute("transform"))
	{
		textTransform = ConcatAffine(parentTransform, ParseTransformAttr(tr->value()));
	}
	const double localX = x, localY = y;   // pre-transform, for extra lines
	textTransform.Apply(x, y);
	// Font sizing: scale by the viewBox->pixel factor only (no xf.scale on
	// top).  Multiplying by xf.scale as well would render an Inkscape "2.82
	// px" font at its full physical 2.82 mm — larger than typical for title-
	// block label text and bigger than the procedural box's font.  This
	// matches how Inkscape's design-time font size visually compares to
	// schematic-tool fonts.
	const double textScale = textTransform.UniformScale();

	LOGFONT lf;
	memset(&lf, 0, sizeof(lf));
	lf.lfHeight = -(LONG)(fontSize * (textScale > 0.0 ? textScale : 1.0) + 0.5);   // negative = "em-height"; CContext applies its own scaling
	lf.lfWeight = eff.weight;
	lf.lfItalic = eff.italic ? TRUE : FALSE;
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

	// A resolved token may span several lines (e.g. a multi-line revision
	// change description), and data-wrap-width="<w>" word-wraps the text to
	// that width (local units): draw each line 1.2 em below the previous one.
	double wrapWidth = 0.0;
	if (rapidxml::xml_attribute<>* ww = node->first_attribute("data-wrap-width"))
	{
		wrapWidth = atof(ww->value());
	}
	CA2T family(fontFamily, CP_UTF8);
	const std::vector<CString> lines = WrapText(text, wrapWidth, fontSize, CString((LPCTSTR)family), eff.weight, eff.italic);
	for (size_t line = 0; line < lines.size(); ++line)
	{
		double lx = localX, ly = localY + line * fontSize * 1.2;
		textTransform.Apply(lx, ly);
		CDPoint pos = xf.Map(lx, ly);
		dc.TextOut(pos.x, pos.y, lines[line]);
	}
}

void RenderTextsRecursive(rapidxml::xml_node<>* node, CContext& dc,
                          const ViewBoxXform& xf, const CDetails& details,
                          const Affine& parentTransform, const CssRuleMap& rules)
{
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling())
	{
		if (c->type() != rapidxml::node_element) continue;
		const char* n = c->name();
		if (!n) continue;

		// Compose the child's effective transform (parent * own).
		Affine childTransform = parentTransform;
		if (rapidxml::xml_attribute<>* tr = c->first_attribute("transform"))
		{
			childTransform = ConcatAffine(parentTransform, ParseTransformAttr(tr->value()));
		}

		if (strcmp(n, "text") == 0)
			RenderOneText(c, dc, xf, details, parentTransform, rules);
		else
			RenderTextsRecursive(c, dc, xf, details, childTransform, rules);
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
		if (root)
		{
			// NanoSVG bakes the viewBox->pixel transform into shape points,
			// but <text> x/y attributes are in raw viewBox user units.  Build
			// the same viewBox->pixel transform here and seed the text walk's
			// parent transform with it so text positions match shape positions.
			double vbX = 0.0, vbY = 0.0;
			double vbW = (double)m_image->width;
			double vbH = (double)m_image->height;
			if (rapidxml::xml_attribute<>* a = root->first_attribute("viewBox"))
			{
				double values[4] = { 0, 0, 0, 0 };
				const char* p = a->value();
				int n = 0;
				while (p && *p && n < 4)
				{
					while (*p == ' ' || *p == ',' || *p == '\t') ++p;
					char* eend = nullptr;
					double v = strtod(p, &eend);
					if (eend == p) break;
					values[n++] = v;
					p = eend;
				}
				if (n == 4 && values[2] > 0.0 && values[3] > 0.0)
				{
					vbX = values[0]; vbY = values[1];
					vbW = values[2]; vbH = values[3];
				}
			}
			Affine viewBoxToPixel;
			viewBoxToPixel.a = (vbW > 0.0) ? (double)m_image->width  / vbW : 1.0;
			viewBoxToPixel.d = (vbH > 0.0) ? (double)m_image->height / vbH : 1.0;
			viewBoxToPixel.e = -vbX * viewBoxToPixel.a;
			viewBoxToPixel.f = -vbY * viewBoxToPixel.d;
			// Parse <style> class rules once, then walk the text nodes.
			CssRuleMap rules;
			CollectStyleSheet(root, rules);
			RenderTextsRecursive(root, dc, xf, details, viewBoxToPixel, rules);
		}
	}
	catch (const rapidxml::parse_error&)
	{
		// Shape pass already happened; ignore text-pass parse error.
	}
}

//=========================================================================
// CTitleBlockTemplateStore

namespace {

void AddTemplatesFromFolder(const CString& folder, bool isUserFolder,
                            std::vector<STitleBlockTemplate>& out)
{
	if (folder.IsEmpty()) return;

	CFileFind finder;
	const CString pattern = folder + _T("\\*.svg");
	BOOL working = finder.FindFile(pattern);
	while (working)
	{
		working = finder.FindNextFile();
		if (finder.IsDots() || finder.IsDirectory()) continue;

		STitleBlockTemplate t;
		t.fullPath = finder.GetFilePath();

		CString fname = finder.GetFileName();
		const int dotIdx = fname.ReverseFind(_T('.'));
		t.name = (dotIdx > 0) ? fname.Left(dotIdx) : fname;
		t.displayName = t.name;
		if (isUserFolder) t.displayName += _T(" (user)");

		out.push_back(t);
	}
	finder.Close();
}

} // namespace

std::vector<STitleBlockTemplate> CTitleBlockTemplateStore::Enumerate()
{
	std::vector<STitleBlockTemplate> result;

	// Bundled, next to the executable
	const CString mainDir = CConCadApp::GetMainDir();
	AddTemplatesFromFolder(mainDir + _T("templates\\title-blocks"), false, result);

	// Dev-build fallback: one level up from exe (Debug/.. -> repo root)
	AddTemplatesFromFolder(mainDir + _T("..\\templates\\title-blocks"), false, result);

	// User additions in %APPDATA%\ConCAD\templates\title-blocks
	AddTemplatesFromFolder(GetUserFolder(), true, result);

	// Sort by display name for stable, user-friendly ordering.
	std::sort(result.begin(), result.end(),
		[](const STitleBlockTemplate& a, const STitleBlockTemplate& b) {
			return a.displayName.CompareNoCase(b.displayName) < 0;
		});

	return result;
}

bool CTitleBlockTemplateStore::ReadFile(const CString& path, CString& outSvg)
{
	outSvg.Empty();

	CFile file;
	if (!file.Open(path, CFile::modeRead | CFile::shareDenyWrite)) return false;

	const ULONGLONG len = file.GetLength();
	if (len == 0 || len > 5 * 1024 * 1024) return false;

	std::vector<char> buf((size_t)len + 1, 0);
	file.Read(buf.data(), (UINT)len);
	buf[(size_t)len] = '\0';

	CA2T converted(buf.data(), CP_UTF8);
	outSvg = (LPCTSTR)converted;
	return !outSvg.IsEmpty();
}

bool CTitleBlockTemplateStore::FindByName(const CString& name, CString& outSvg)
{
	outSvg.Empty();
	if (name.IsEmpty()) return false;

	const std::vector<STitleBlockTemplate> all = Enumerate();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].name.CompareNoCase(name) == 0)
		{
			return ReadFile(all[i].fullPath, outSvg);
		}
	}
	return false;
}

CString CTitleBlockTemplateStore::EncodeSvgBase64(const CString& svgText)
{
	if (svgText.IsEmpty()) return CString();

	// Encode the SVG's UTF-8 bytes (the on-disk representation), not the
	// build's wide chars, so the blob is portable across builds.
	CT2A utf8(svgText, CP_UTF8);
	const DWORD srcLen = (DWORD)strlen((LPCSTR)utf8);
	if (srcLen == 0) return CString();

	DWORD outChars = 0;
	if (!CryptBinaryToStringA((const BYTE*)(LPCSTR)utf8, srcLen,
		CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outChars) || outChars == 0)
	{
		return CString();
	}

	std::vector<char> b64((size_t)outChars + 1, 0);
	if (!CryptBinaryToStringA((const BYTE*)(LPCSTR)utf8, srcLen,
		CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64.data(), &outChars))
	{
		return CString();
	}
	b64[(size_t)outChars] = '\0';

	return CString(CA2T(b64.data()));
}

bool CTitleBlockTemplateStore::DecodeSvgBase64(const CString& base64, CString& outSvg)
{
	outSvg.Empty();
	if (base64.IsEmpty()) return false;

	CT2A b64(base64);
	const DWORD srcLen = (DWORD)strlen((LPCSTR)b64);
	if (srcLen == 0) return false;

	DWORD outBytes = 0;
	if (!CryptStringToBinaryA((LPCSTR)b64, srcLen, CRYPT_STRING_BASE64,
		NULL, &outBytes, NULL, NULL) || outBytes == 0)
	{
		return false;
	}

	std::vector<BYTE> bytes((size_t)outBytes + 1, 0);
	if (!CryptStringToBinaryA((LPCSTR)b64, srcLen, CRYPT_STRING_BASE64,
		bytes.data(), &outBytes, NULL, NULL))
	{
		return false;
	}
	bytes[(size_t)outBytes] = 0;

	outSvg = CString(CA2T((LPCSTR)bytes.data(), CP_UTF8));
	return !outSvg.IsEmpty();
}

//=========================================================================
// Growing revision table (see CTitleBlockTemplateStore::ExpandRevisionRows)

namespace {

// Value of attribute name in the start tag text, or "" if absent.
CString GetTagAttr(const CString& tag, const CString& name, int* pValueStart = nullptr, int* pValueLen = nullptr)
{
	for (int from = 0; ; )
	{
		int p = tag.Find(name, from);
		if (p < 0) return CString();
		from = p + 1;
		// Must be a whole attribute name: preceded by whitespace, followed by '='.
		if (p == 0 || !_istspace(tag[p - 1])) continue;
		int q = p + name.GetLength();
		while (q < tag.GetLength() && _istspace(tag[q])) ++q;
		if (q >= tag.GetLength() || tag[q] != _T('=')) continue;
		++q;
		while (q < tag.GetLength() && _istspace(tag[q])) ++q;
		if (q >= tag.GetLength() || (tag[q] != _T('"') && tag[q] != _T('\''))) continue;
		TCHAR quote = tag[q];
		int end = tag.Find(quote, q + 1);
		if (end < 0) return CString();
		if (pValueStart) *pValueStart = q + 1;
		if (pValueLen) *pValueLen = end - q - 1;
		return tag.Mid(q + 1, end - q - 1);
	}
}

// Replace attribute name's value inside svg's start tag at [tagStart, tagEnd).
// Returns the length change.
int SetTagAttr(CString& svg, int tagStart, int tagEnd, const CString& name, const CString& value)
{
	CString tag = svg.Mid(tagStart, tagEnd - tagStart);
	int vs = 0, vl = 0;
	GetTagAttr(tag, name, &vs, &vl);
	if (vl == 0 && vs == 0) return 0;
	svg = svg.Left(tagStart + vs) + value + svg.Mid(tagStart + vs + vl);
	return value.GetLength() - vl;
}

// Next occurrence of needle (from 'from') that sits inside an element's
// start tag — not in a comment or text.  Sets tagStart to its '<'.
int FindInStartTag(const CString& svg, const CString& needle, int from, int& tagStart)
{
	for (int p = svg.Find(needle, from); p >= 0; p = svg.Find(needle, p + 1))
	{
		int lt = svg.Left(p).ReverseFind(_T('<'));
		if (lt < 0 || svg.Mid(lt, p - lt).Find(_T('>')) >= 0) continue;
		if (lt + 1 < svg.GetLength() && _istalpha(svg[lt + 1]))
		{
			tagStart = lt;
			return p;
		}
	}
	return -1;
}

CString FormatNumber(double v)
{
	CString s;
	s.Format(_T("%.10g"), v);
	return s;
}

// End (one past '>') of the element whose start tag begins at tagStart.
int FindElementEnd(const CString& svg, int tagStart)
{
	int nameEnd = tagStart + 1;
	while (nameEnd < svg.GetLength() && !_istspace(svg[nameEnd]) && svg[nameEnd] != _T('>') && svg[nameEnd] != _T('/')) ++nameEnd;
	const CString name = svg.Mid(tagStart + 1, nameEnd - tagStart - 1);

	int gt = svg.Find(_T('>'), tagStart);
	if (gt < 0) return -1;
	if (svg[gt - 1] == _T('/')) return gt + 1;   // self-closing

	int depth = 1;
	int pos = gt + 1;
	while (depth > 0)
	{
		int lt = svg.Find(_T('<'), pos);
		if (lt < 0) return -1;
		int e = svg.Find(_T('>'), lt);
		if (e < 0) return -1;
		if (svg.Mid(lt + 1, 1) == _T("/"))
		{
			if (svg.Mid(lt + 2, name.GetLength()) == name) --depth;
		}
		else if (svg.Mid(lt + 1, name.GetLength()) == name && svg[e - 1] != _T('/'))
		{
			TCHAR after = svg[lt + 1 + name.GetLength()];
			if (_istspace(after) || after == _T('>')) ++depth;
		}
		pos = e + 1;
	}
	return pos;
}

} // namespace

CString CTitleBlockTemplateStore::ExpandRevisionRows(const CString& svgIn, const std::vector<CString>& descriptions, int* pRows)
{
	const int entryCount = (int)descriptions.size();
	if (pRows) *pRows = 0;
	CString svg = svgIn;
	const CString marker = _T("data-repeat=\"revisions\"");
	int rowStart = -1;
	int m = FindInStartTag(svg, marker, 0, rowStart);
	if (m < 0) return svg;

	int rowTagEnd = svg.Find(_T('>'), m);
	int rowEnd = FindElementEnd(svg, rowStart);
	if (rowStart < 0 || rowTagEnd < 0 || rowEnd < 0) return svgIn;

	const CString rowTag = svg.Mid(rowStart, rowTagEnd - rowStart);
	const double pitch = _tstof(GetTagAttr(rowTag, _T("data-row-height")));
	CString sMax = GetTagAttr(rowTag, _T("data-max-rows"));
	const int maxRows = sMax.IsEmpty() ? 5 : max(1, _ttoi(sMax));
	if (pitch <= 0.0) return svgIn;

	const int rows = max(1, min(entryCount, maxRows));
	const int first = entryCount > rows ? entryCount - rows : 0;   // newest rows
	if (pRows) *pRows = rows;

	const CString row = svg.Mid(rowStart, rowEnd - rowStart);

	// A row grows by 1.2 em per extra line of its change description: the
	// <text> holding {Rev1Desc} (font-size and data-wrap-width in root units).
	double descFont = 0.0, descWrap = 0.0;
	CString descFamily;
	int descWeight = FW_NORMAL;
	bool descItalic = false;
	int dp = row.Find(_T("{Rev1Desc}"));
	int dt = dp < 0 ? -1 : row.Left(dp).ReverseFind(_T('<'));
	if (dt >= 0)
	{
		const CString tag = row.Mid(dt, row.Find(_T('>'), dt) - dt);
		descWrap = _tstof(GetTagAttr(tag, _T("data-wrap-width")));
		descFont = _tstof(GetTagAttr(tag, _T("font-size")));
		descFamily = GetTagAttr(tag, _T("font-family"));
		const CString style = GetTagAttr(tag, _T("style"));
		int fs = style.Find(_T("font-size:"));
		if (fs >= 0) descFont = _tstof(style.Mid(fs + 10));
		int ff = style.Find(_T("font-family:"));
		if (ff >= 0)
		{
			descFamily = style.Mid(ff + 12);
			int semi = descFamily.Find(_T(';'));
			if (semi >= 0) descFamily = descFamily.Left(semi);
		}
		CString sWeight = GetTagAttr(tag, _T("font-weight"));
		CString sStyle = GetTagAttr(tag, _T("font-style"));
		int fw = style.Find(_T("font-weight:"));
		if (fw >= 0) sWeight = style.Mid(fw + 12);
		int fst = style.Find(_T("font-style:"));
		if (fst >= 0) sStyle = style.Mid(fst + 11);
		if (!sWeight.IsEmpty()) descWeight = ParseFontWeight(CT2A(sWeight));
		if (!sStyle.IsEmpty())  descItalic = ParseFontItalic(CT2A(sStyle));
	}

	// Repeat the row, each copy shifted down below the previous (possibly
	// taller) row, renumbered, and its data-stretch="row" elements lengthened.
	CString rowsText;
	double offset = 0.0;
	for (int i = 0; i < rows; ++i)
	{
		int lineCount = 1;
		if (first + i < entryCount && descFont > 0.0)
		{
			lineCount = (int)WrapText(descriptions[first + i], descWrap, descFont, descFamily, descWeight, descItalic).size();
		}
		const double grow = (lineCount - 1) * descFont * 1.2;

		CString n;
		n.Format(_T("%d"), i + 1);
		CString copy = row;
		if (i > 0)
		{
			copy.Replace(_T("{Rev1}"),     _T("{Rev") + n + _T("}"));
			copy.Replace(_T("{Rev1Date}"), _T("{Rev") + n + _T("Date}"));
			copy.Replace(_T("{Rev1Desc}"), _T("{Rev") + n + _T("Desc}"));
			copy.Replace(_T("{Rev1By}"),   _T("{Rev") + n + _T("By}"));
			copy.Replace(_T(" ") + marker, _T(""));
		}
		if (grow > 0.0)
		{
			const CString rowStretch = _T("data-stretch=\"row\"");
			int ts = -1;
			for (int p = FindInStartTag(copy, rowStretch, 0, ts); p >= 0; p = FindInStartTag(copy, rowStretch, p + 1, ts))
			{
				int te = copy.Find(_T('>'), p);
				const CString tag = copy.Mid(ts, te - ts);
				CString y2 = GetTagAttr(tag, _T("y2"));
				if (!y2.IsEmpty())
				{
					SetTagAttr(copy, ts, te, _T("y2"), FormatNumber(_tstof(y2) + grow));
				}
				else
				{
					CString h = GetTagAttr(tag, _T("height"));
					if (!h.IsEmpty()) SetTagAttr(copy, ts, te, _T("height"), FormatNumber(_tstof(h) + grow));
				}
			}
		}
		if (i == 0 && offset == 0.0)
		{
			rowsText += copy;
		}
		else
		{
			rowsText += _T("<g transform=\"translate(0,") + FormatNumber(offset) + _T(")\">") + copy + _T("</g>");
		}
		offset += pitch + grow;
	}
	svg = svg.Left(rowStart) + rowsText + svg.Mid(rowEnd);
	const double extra = offset - pitch;
	if (extra <= 0.0) return svg;

	// Stretch marked elements (e.g. the outer frame).
	const CString stretch = _T("data-stretch=\"rows\"");
	int ts = -1;
	for (int p = FindInStartTag(svg, stretch, 0, ts); p >= 0; p = FindInStartTag(svg, stretch, p + 1, ts))
	{
		int te = svg.Find(_T('>'), p);
		CString h = GetTagAttr(svg.Mid(ts, te - ts), _T("height"));
		if (!h.IsEmpty())
		{
			SetTagAttr(svg, ts, te, _T("height"), FormatNumber(_tstof(h) + extra));
		}
	}

	// Grow the root: viewBox height, and height in proportion (keeps units).
	int rs = svg.Find(_T("<svg"));
	int re = rs < 0 ? -1 : svg.Find(_T('>'), rs);
	if (rs >= 0 && re >= 0)
	{
		CString vb = GetTagAttr(svg.Mid(rs, re - rs), _T("viewBox"));
		double v[4] = { 0, 0, 0, 0 };
		CString tmp = vb;
		tmp.Replace(_T(','), _T(' '));
		if (_stscanf_s(tmp, _T("%lf %lf %lf %lf"), &v[0], &v[1], &v[2], &v[3]) == 4 && v[3] > 0.0)
		{
			const double newVbH = v[3] + extra;
			CString h = GetTagAttr(svg.Mid(rs, re - rs), _T("height"));
			if (!h.IsEmpty())
			{
				LPTSTR unit = nullptr;
				const double num = _tcstod(h, &unit);
				re += SetTagAttr(svg, rs, re, _T("height"), FormatNumber(num * newVbH / v[3]) + unit);
			}
			SetTagAttr(svg, rs, re, _T("viewBox"),
				FormatNumber(v[0]) + _T(" ") + FormatNumber(v[1]) + _T(" ") + FormatNumber(v[2]) + _T(" ") + FormatNumber(newVbH));
		}
	}
	return svg;
}

//=========================================================================
// Import conversion (see CTitleBlockTemplateStore::ConvertImportedSvg)

namespace {

bool IsEditorName(const char* name)
{
	return strncmp(name, "inkscape:", 9) == 0 || strncmp(name, "sodipodi:", 9) == 0;
}

// Namespace declarations that only the removed editor data uses.
bool IsEditorNamespaceDecl(const char* name)
{
	static const char* const decls[] = {
		"xmlns:inkscape", "xmlns:sodipodi", "xmlns:rdf", "xmlns:cc", "xmlns:dc", "xmlns:svg" };
	for (const char* d : decls)
		if (strcmp(name, d) == 0) return true;
	return false;
}

std::string Trim(const std::string& s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return std::string();
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

// Drop editor-only style properties: -inkscape-*, and the font-variant /
// font-stretch / font-feature-settings defaults Inkscape writes on every text.
std::string CleanStyle(const char* style)
{
	std::string out;
	std::string all(style);
	size_t pos = 0;
	while (pos <= all.size())
	{
		size_t semi = all.find(';', pos);
		if (semi == std::string::npos) semi = all.size();
		const std::string decl = Trim(all.substr(pos, semi - pos));
		pos = semi + 1;
		if (decl.empty()) continue;
		const size_t colon = decl.find(':');
		const std::string prop = Trim(decl.substr(0, colon));
		const std::string val = colon == std::string::npos ? std::string() : Trim(decl.substr(colon + 1));
		if (prop.compare(0, 9, "-inkscape") == 0) continue;
		if (val == "normal" && (prop.compare(0, 12, "font-variant") == 0 ||
		                        prop == "font-stretch" || prop == "font-feature-settings")) continue;
		if (!out.empty()) out += ';';
		out += prop + ":" + val;
	}
	return out;
}

struct ImportStats
{
	int  removed = 0;          // editor elements/attributes dropped
	bool flowText = false;     // <flowRoot>: Inkscape 0.92 flowed text
	bool image = false;        // <image>: not rendered
	bool multiLine = false;    // <text> with several line tspans
};

void CleanImportedNode(rapidxml::xml_document<>& doc, rapidxml::xml_node<>* node,
                       const std::string& source, ImportStats& st)
{
	for (rapidxml::xml_node<>* c = node->first_node(); c; )
	{
		rapidxml::xml_node<>* next = c->next_sibling();
		if (c->type() == rapidxml::node_element)
		{
			const char* n = c->name();
			if (IsEditorName(n) || strcmp(n, "metadata") == 0)
			{
				node->remove_node(c);
				++st.removed;
				c = next;
				continue;
			}
			if (strcmp(n, "flowRoot") == 0) st.flowText = true;
			if (strcmp(n, "image") == 0) st.image = true;
			CleanImportedNode(doc, c, source, st);
			if (strcmp(n, "defs") == 0 && !c->first_node())
			{
				node->remove_node(c);
			}
		}
		c = next;
	}
	if (node->type() != rapidxml::node_element) return;

	for (rapidxml::xml_attribute<>* a = node->first_attribute(); a; )
	{
		rapidxml::xml_attribute<>* next = a->next_attribute();
		const char* n = a->name();
		if (IsEditorName(n) || IsEditorNamespaceDecl(n))
		{
			node->remove_attribute(a);
			++st.removed;
		}
		else if (strcmp(n, "id") == 0 && source.find(std::string("#") + a->value()) == std::string::npos)
		{
			node->remove_attribute(a);
		}
		else if (strcmp(n, "style") == 0)
		{
			const std::string clean = CleanStyle(a->value());
			if (clean.empty()) node->remove_attribute(a);
			else a->value(doc.allocate_string(clean.c_str()));
		}
		a = next;
	}

	// The renderer positions a <text> by its own x/y; Inkscape may put them
	// on the <tspan> only.  Several tspans at different y = multi-line text,
	// which is drawn on one line.
	if (strcmp(node->name(), "text") == 0)
	{
		rapidxml::xml_node<>* first = node->first_node("tspan");
		if (first)
		{
			static const char* const xy[] = { "x", "y" };
			for (const char* k : xy)
			{
				rapidxml::xml_attribute<>* src = first->first_attribute(k);
				if (src && !node->first_attribute(k))
					node->append_attribute(doc.allocate_attribute(k, doc.allocate_string(src->value())));
			}
			for (rapidxml::xml_node<>* t = first->next_sibling("tspan"); t; t = t->next_sibling("tspan"))
			{
				rapidxml::xml_attribute<>* y0 = first->first_attribute("y");
				rapidxml::xml_attribute<>* y1 = t->first_attribute("y");
				if (y0 && y1 && strcmp(y0->value(), y1->value()) != 0) st.multiLine = true;
			}
		}
	}
}

void AppendEscaped(std::string& out, const char* s, bool attr)
{
	for (; *s; ++s)
	{
		switch (*s)
		{
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += attr ? ">" : "&gt;"; break;
		case '"': out += attr ? "&quot;" : "\""; break;
		default:  out += *s; break;
		}
	}
}

void AppendStartTag(std::string& out, rapidxml::xml_node<>* node)
{
	out += '<';
	out += node->name();
	for (rapidxml::xml_attribute<>* a = node->first_attribute(); a; a = a->next_attribute())
	{
		out += ' ';
		out += a->name();
		out += "=\"";
		AppendEscaped(out, a->value(), true);
		out += '"';
	}
}

// Mixed content (text and its tspans) is written on one line, unchanged.
void AppendInline(std::string& out, rapidxml::xml_node<>* node)
{
	switch (node->type())
	{
	case rapidxml::node_data:  AppendEscaped(out, node->value(), false); return;
	case rapidxml::node_cdata: out += "<![CDATA["; out += node->value(); out += "]]>"; return;
	case rapidxml::node_element: break;
	default: return;
	}
	AppendStartTag(out, node);
	if (!node->first_node()) { out += " />"; return; }
	out += '>';
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling()) AppendInline(out, c);
	out += "</";
	out += node->name();
	out += '>';
}

void AppendNode(std::string& out, rapidxml::xml_node<>* node, int depth)
{
	const std::string indent(depth * 2, ' ');
	if (node->type() == rapidxml::node_comment)
	{
		out += indent + "<!--" + node->value() + "-->\n";
		return;
	}
	if (node->type() != rapidxml::node_element) return;

	bool mixed = strcmp(node->name(), "text") == 0;
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling())
		if (c->type() == rapidxml::node_data || c->type() == rapidxml::node_cdata) mixed = true;
	if (mixed || !node->first_node())
	{
		out += indent;
		AppendInline(out, node);
		out += '\n';
		return;
	}
	out += indent;
	AppendStartTag(out, node);
	out += ">\n";
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling()) AppendNode(out, c, depth + 1);
	out += indent + "</" + node->name() + ">\n";
}

std::string SerializeSvg(rapidxml::xml_document<>& doc)
{
	std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
	for (rapidxml::xml_node<>* c = doc.first_node(); c; c = c->next_sibling()) AppendNode(out, c, 0);
	return out;
}

std::string FormatRounded(double v)
{
	char buf[64];
	sprintf_s(buf, "%.4f", v);
	std::string s(buf);
	s.erase(s.find_last_not_of('0') + 1);
	if (!s.empty() && s.back() == '.') s.pop_back();
	if (s == "-0") s = "0";
	return s;
}

// Text anchors (and their ascent) in root user units, for the crop.
void AddTextBounds(rapidxml::xml_node<>* node, const Affine& parent, const CssRuleMap& rules,
                   double& x0, double& y0, double& x1, double& y1, bool& any)
{
	for (rapidxml::xml_node<>* c = node->first_node(); c; c = c->next_sibling())
	{
		if (c->type() != rapidxml::node_element) continue;
		Affine xf = parent;
		if (rapidxml::xml_attribute<>* tr = c->first_attribute("transform"))
			xf = ConcatAffine(parent, ParseTransformAttr(tr->value()));
		if (strcmp(c->name(), "text") != 0)
		{
			if (strcmp(c->name(), "defs") != 0) AddTextBounds(c, xf, rules, x0, y0, x1, y1, any);
			continue;
		}
		CssTextProps eff;
		double x = 0.0, y = 0.0;
		ResolveNodeTextProps(c, rules, eff, &x, &y);
		if (rapidxml::xml_node<>* t = c->first_node("tspan")) ResolveNodeTextProps(t, rules, eff, nullptr, nullptr);
		const double size = eff.hasFontSize ? eff.fontSize : 0.0;
		double px[2] = { x, x }, py[2] = { y, y - size * 0.8 };
		for (int i = 0; i < 2; ++i)
		{
			xf.Apply(px[i], py[i]);
			if (!any) { x0 = x1 = px[i]; y0 = y1 = py[i]; any = true; }
			x0 = (std::min)(x0, px[i]); x1 = (std::max)(x1, px[i]);
			y0 = (std::min)(y0, py[i]); y1 = (std::max)(y1, py[i]);
		}
	}
}

// Content bounds in root user units; false when nothing is drawn.
bool ContentBounds(const std::string& svg, double vb[4], bool& hasViewBox, double bounds[4])
{
	std::vector<char> buf(svg.begin(), svg.end());
	buf.push_back('\0');
	NSVGimage* img = nsvgParse(buf.data(), "px", 96.0f);
	if (!img) return false;
	const double imgW = img->width, imgH = img->height;

	std::vector<char> xml(svg.begin(), svg.end());
	xml.push_back('\0');
	rapidxml::xml_document<> doc;
	doc.parse<rapidxml::parse_default>(xml.data());
	rapidxml::xml_node<>* root = doc.first_node("svg");
	if (!root) { nsvgDelete(img); return false; }

	hasViewBox = false;
	vb[0] = vb[1] = 0.0; vb[2] = imgW; vb[3] = imgH;
	if (rapidxml::xml_attribute<>* a = root->first_attribute("viewBox"))
	{
		double v[4];
		std::string s(a->value());
		std::replace(s.begin(), s.end(), ',', ' ');
		if (sscanf_s(s.c_str(), "%lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3]) == 4 && v[2] > 0.0 && v[3] > 0.0)
		{
			memcpy(vb, v, sizeof(v));
			hasViewBox = true;
		}
	}

	bool any = false;
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	if (imgW > 0.0 && imgH > 0.0)
	{
		const double sx = vb[2] / imgW, sy = vb[3] / imgH;
		for (const NSVGshape* sh = img->shapes; sh; sh = sh->next)
		{
			if (!(sh->flags & NSVG_FLAGS_VISIBLE)) continue;
			const double half = sh->stroke.type != NSVG_PAINT_NONE ? sh->strokeWidth / 2.0 : 0.0;
			const double bx0 = vb[0] + (sh->bounds[0] - half) * sx, by0 = vb[1] + (sh->bounds[1] - half) * sy;
			const double bx1 = vb[0] + (sh->bounds[2] + half) * sx, by1 = vb[1] + (sh->bounds[3] + half) * sy;
			if (!any) { x0 = bx0; y0 = by0; x1 = bx1; y1 = by1; any = true; }
			x0 = (std::min)(x0, bx0); y0 = (std::min)(y0, by0);
			x1 = (std::max)(x1, bx1); y1 = (std::max)(y1, by1);
		}
	}
	nsvgDelete(img);

	CssRuleMap rules;
	CollectStyleSheet(root, rules);
	AddTextBounds(root, Affine(), rules, x0, y0, x1, y1, any);

	bounds[0] = x0; bounds[1] = y0; bounds[2] = x1; bounds[3] = y1;
	return any && x1 > x0 && y1 > y0;
}

// Scale a width/height attribute ("161.7mm", "600") by f, keeping its unit.
// Percentages are left alone (meaningless for a title block).
void ScaleLengthAttr(rapidxml::xml_document<>& doc, rapidxml::xml_node<>* root, const char* name, double f)
{
	rapidxml::xml_attribute<>* a = root->first_attribute(name);
	if (!a) return;
	char* unit = nullptr;
	const double v = strtod(a->value(), &unit);
	if (unit == a->value() || strchr(unit, '%')) return;
	const std::string s = FormatRounded(v * f) + Trim(unit);
	a->value(doc.allocate_string(s.c_str()));
}

} // namespace

CString CTitleBlockTemplateStore::GetUserFolder()
{
	TCHAR appData[MAX_PATH] = { 0 };
	if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appData))) return CString();
	return CString(appData) + _T("\\ConCAD\\templates\\title-blocks");
}

bool CTitleBlockTemplateStore::SaveUserTemplate(const CString& name, const CString& svg, CString& outPath)
{
	const CString folder = GetUserFolder();
	if (folder.IsEmpty()) return false;
	SHCreateDirectoryEx(NULL, folder, NULL);
	outPath = folder + _T("\\") + name + _T(".svg");

	CT2A utf8(svg, CP_UTF8);
	CFile file;
	if (!file.Open(outPath, CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive)) return false;
	try
	{
		file.Write((LPCSTR)utf8, (UINT)strlen((LPCSTR)utf8));
		file.Close();
	}
	catch (CFileException* e)
	{
		e->Delete();
		return false;
	}
	return true;
}

bool CTitleBlockTemplateStore::ConvertImportedSvg(const CString& svgIn, CString& svgOut, CString& report)
{
	svgOut.Empty();
	report.Empty();

	CT2A utf8(svgIn, CP_UTF8);
	const std::string source((LPCSTR)utf8);
	std::vector<char> buf(source.begin(), source.end());
	buf.push_back('\0');

	rapidxml::xml_document<> doc;
	try
	{
		doc.parse<rapidxml::parse_declaration_node | rapidxml::parse_comment_nodes>(buf.data());
	}
	catch (const rapidxml::parse_error& e)
	{
		report.Format(_T("The file is not valid XML (%s)."), (LPCTSTR)CString(e.what()));
		return false;
	}
	rapidxml::xml_node<>* root = doc.first_node("svg");
	if (!root)
	{
		report = _T("The file is not an SVG drawing (no <svg> root element).");
		return false;
	}

	ImportStats st;
	CleanImportedNode(doc, root, source, st);
	// Drop the original <?xml ...?> declaration; SerializeSvg writes its own.
	for (rapidxml::xml_node<>* c = doc.first_node(); c; )
	{
		rapidxml::xml_node<>* next = c->next_sibling();
		if (c->type() == rapidxml::node_declaration) doc.remove_node(c);
		c = next;
	}
	std::string out = SerializeSvg(doc);

	// Crop to the drawn content.
	double vb[4], b[4];
	bool hasViewBox = false;
	CString cropNote;
	if (!ContentBounds(out, vb, hasViewBox, b))
	{
		report = _T("The SVG has no visible content.");
		return false;
	}
	const double eps = 0.001 * (std::max)(vb[2], vb[3]);
	if (fabs(b[0] - vb[0]) > eps || fabs(b[1] - vb[1]) > eps ||
	    fabs(b[2] - (vb[0] + vb[2])) > eps || fabs(b[3] - (vb[1] + vb[3])) > eps)
	{
		const double w = b[2] - b[0], h = b[3] - b[1];
		if (!hasViewBox && !root->first_attribute("width"))
		{
			// No viewBox and no size: the user units were pixels; keep them.
			root->append_attribute(doc.allocate_attribute("width", doc.allocate_string(FormatRounded(w).c_str())));
			root->append_attribute(doc.allocate_attribute("height", doc.allocate_string(FormatRounded(h).c_str())));
		}
		else
		{
			ScaleLengthAttr(doc, root, "width", w / vb[2]);
			ScaleLengthAttr(doc, root, "height", h / vb[3]);
		}
		const std::string box = FormatRounded(b[0]) + " " + FormatRounded(b[1]) + " " + FormatRounded(w) + " " + FormatRounded(h);
		if (rapidxml::xml_attribute<>* a = root->first_attribute("viewBox"))
			a->value(doc.allocate_string(box.c_str()));
		else
			root->append_attribute(doc.allocate_attribute("viewBox", doc.allocate_string(box.c_str())));

		const char* wa = root->first_attribute("width") ? root->first_attribute("width")->value() : "";
		const char* ha = root->first_attribute("height") ? root->first_attribute("height")->value() : "";
		cropNote.Format(_T("Cropped to the drawing: %s x %s."),
			(LPCTSTR)CString(CA2T(wa, CP_UTF8)), (LPCTSTR)CString(CA2T(ha, CP_UTF8)));
		out = SerializeSvg(doc);
	}

	svgOut = CString(CA2T(out.c_str(), CP_UTF8));

	// Summary.
	if (st.removed > 0) report += _T("Removed the drawing program's editor data.\n");
	if (!cropNote.IsEmpty()) report += cropNote + _T("\n");

	std::vector<CString> tokens;
	for (int pos = 0; (pos = svgOut.Find(_T('{'), pos)) >= 0; ++pos)
	{
		const int end = svgOut.Find(_T('}'), pos + 1);
		if (end < 0) break;
		const CString name = svgOut.Mid(pos + 1, end - pos - 1);
		bool ident = !name.IsEmpty() && (_istalpha(name[0]) || name[0] == _T('_'));
		for (int i = 0; ident && i < name.GetLength(); ++i)
			ident = _istalnum(name[i]) || name[i] == _T('_');
		if (!ident) continue;
		bool seen = false;
		for (const CString& t : tokens) seen = seen || t.CompareNoCase(name) == 0;
		if (!seen) tokens.push_back(name);
	}
	if (tokens.empty())
	{
		report += _T("No {Name} fields found - the block has only fixed text.\n");
	}
	else
	{
		report += _T("Fields: ");
		for (size_t i = 0; i < tokens.size(); ++i)
			report += (i ? _T(", {") : _T("{")) + tokens[i] + _T("}");
		report += _T("\n");
	}
	if (st.flowText) report += _T("Warning: flowed text (Inkscape 0.92 text frames) is not drawn - use plain text.\n");
	if (st.image) report += _T("Warning: embedded images are not drawn.\n");
	if (st.multiLine) report += _T("Warning: text with several lines is drawn on one line - make each line its own text.\n");
	report.TrimRight();
	return true;
}
