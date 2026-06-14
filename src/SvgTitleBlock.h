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
	CString name;          // filename stem only — the stable key stored in the .dsn
	CString fullPath;
};

// Enumerates SVG title-block templates from:
//   1. <exe-dir>/templates/title-blocks/         (installer-bundled)
//   2. <exe-dir>/../templates/title-blocks/      (dev build fallback)
//   3. %APPDATA%/ConCAD/templates/title-blocks/  (user additions)
class CTitleBlockTemplateStore
{
public:
	static std::vector<STitleBlockTemplate> Enumerate();
	// Read an .svg file from disk into a CString as UTF-8 text.
	// Returns false on failure (missing/empty/too-large).
	static bool ReadFile(const CString& path, CString& outSvg);
	// Resolve a template name (filename stem) against the enumerated stores and
	// read its SVG.  Returns false when no template of that name is installed.
	static bool FindByName(const CString& name, CString& outSvg);

	// Base64 helpers used by the hybrid .dsn storage: the SVG's UTF-8 bytes are
	// base64-encoded so the embedded copy carries no XML-special characters.
	static CString EncodeSvgBase64(const CString& svgText);
	static bool    DecodeSvgBase64(const CString& base64, CString& outSvg);
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
