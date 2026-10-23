#pragma once

#include "Image.h"
#include "SvgTitleBlock.h"
#include <vector>

class CContext;
class CDetails;

// An inserted .svg picture.  The file's bytes are stored as-is (type "SVG"
// in the design's <IMAGE> resources) and drawn as vectors by the title-block
// renderer, so {tokens} in its <text> resolve against the sheet's details.
class CImageSvg: public CTCImage
{
private:
	std::vector<unsigned char> m_data;
	CSvgTitleBlock m_svg;

public:
	CImageSvg();
	virtual ~CImageSvg();

	// Write as an UUENCODE XML tag
	virtual void SaveXML(CXMLWriter &xml);

	// Set the memory image data (takes ownership of data)
	virtual bool SetCompressedData(unsigned char *data, size_t size);

	// Needs a CContext to draw; CDrawMetaFile calls PaintVector instead
	virtual void Paint(CDC &dc, CRect rect, int rotmir);

	// Draw the SVG into the rectangle a..b (drawing units), keeping its aspect ratio
	void PaintVector(CContext &dc, CDPoint a, CDPoint b, const CDetails &details) const;

	// Get the type of this object
	virtual CString GetType();

	// Natural size in pixels (96 dpi)
	virtual CSize GetSize(CDC &dc);
};
