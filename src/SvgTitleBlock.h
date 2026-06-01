#pragma once

#include "DPoint.h"

class CContext;
class CDetails;
struct NSVGimage;

// SVG-based title-block renderer.  Parses SVG via NanoSVG (paths only); text
// elements are picked up by a second walk over the raw XML so CDetails::Resolve
// can substitute {tokens} into <text> bodies.
class CSvgTitleBlock
{
public:
	CSvgTitleBlock();
	~CSvgTitleBlock();

	// Parse svgText.  Returns false on parse failure or empty/zero-sized svg.
	bool Load(const CString& svgText);

	bool IsLoaded() const { return m_image != nullptr; }

	void Clear();

	// Render the parsed SVG into the rectangle (tl..br), preserving the
	// SVG viewBox's aspect ratio (letterbox).
	void Paint(CContext& dc, CDPoint tl, CDPoint br, const CDetails& details) const;

private:
	NSVGimage* m_image;
	CStringA   m_svgUtf8;        // kept for the <text> walk

	CSvgTitleBlock(const CSvgTitleBlock&);
	CSvgTitleBlock& operator=(const CSvgTitleBlock&);
};
