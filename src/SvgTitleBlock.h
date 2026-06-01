#pragma once

#include "DPoint.h"
#include <vector>

class CContext;
class CDetails;
struct NSVGimage;

// One enumerated title-block template (an .svg file on disk).
struct STitleBlockTemplate
{
	CString displayName;   // filename stem, plus "(user)" for the per-user folder
	CString fullPath;
};

// Enumerates SVG title-block templates from:
//   1. <exe-dir>/templates/title-blocks/         (installer-bundled)
//   2. <exe-dir>/../templates/title-blocks/      (dev build fallback)
//   3. %APPDATA%/TinyCAD/templates/title-blocks/ (user additions)
class CTitleBlockTemplateStore
{
public:
	static std::vector<STitleBlockTemplate> Enumerate();
	// Read an .svg file from disk into a CString as UTF-8 text.
	// Returns false on failure (missing/empty/too-large).
	static bool ReadFile(const CString& path, CString& outSvg);
};

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

	// Natural SVG dimensions in pixels (96 dpi).  False if not loaded or
	// the SVG declared zero-area.  Callers convert to internal CAD units.
	bool GetNaturalSizePixels(double& width_px, double& height_px) const;

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
