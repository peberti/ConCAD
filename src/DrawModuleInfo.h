/*
 * ConCAD: the parameters of a placed module
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#pragma once

#include <vector>
#include "DrawingObject.h"

// One per placed module, in the module's group (same m_group).  Holds the
// module's parameters (Name, Reference and the module's own fields), shown
// in the Module panel of Tool Options.  Texts and notes of the module show
// them through {Name} tokens.  Not drawn and not clickable; it is selected,
// copied, deleted and undone together with the rest of the group.
class CDrawModuleInfo: public CDrawingObject
{
public:
	struct Field
	{
		CString name;
		CString value;
	};

	std::vector<Field> m_fields; // [0] Name, [1] Reference, then the module's own fields
	enum
	{
		FixedFields = 2
	};

	CDrawModuleInfo(CConCadDoc *pDesign);

	static const TCHAR* GetXMLTag()
	{
		return _T("MODULEINFO");
	}
	virtual void SaveXML(CXMLWriter &xml);
	virtual void LoadXML(CXMLReader &xml);
	virtual ObjType GetType()
	{
		return xModuleInfo;
	}
	virtual CString GetName() const
	{
		return _T("Module");
	}
	virtual BOOL IsConstruction()
	{
		return TRUE;
	}
	virtual BOOL IsInside(double, double, double, double)
	{
		return FALSE;
	}
	virtual BOOL IsCompletelyInside(double, double, double, double)
	{
		return FALSE;
	}
	virtual double DistanceFromPoint(CDPoint)
	{
		return 1e30;
	}
	virtual void Paint(CContext &, paint_options)
	{
	}
	virtual void Display(BOOL = TRUE)
	{
	}
	virtual void Shift(CDPoint)
	{
	}
	virtual void Rotate(CDPoint, int)
	{
	}
	virtual CDrawingObject* Store();

	// Replace {field} references by the module's values (case-insensitive);
	// other {tokens} are left for the design variables
	CString Resolve(const CString &s) const;
};
