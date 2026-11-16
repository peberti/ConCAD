/*
 * ConCAD: the parameters of a placed module
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#pragma once

#include <vector>
#include "DrawingObject.h"

// One per placed module, in the module's group (same m_group).  Holds the
// module's parameters (Name, Partnumber, Type and the module's own fields), shown
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
		CString choices; // the values offered in a drop-down, "a; b; c" (may be empty)
	};

	std::vector<Field> m_fields; // [0] Name, then Partnumber, Type and the module's own fields
	bool m_excludeFromBom; // left out of File -> Export BoM (Module Tool Options)
	enum
	{
		FixedFields = 1
	};

	// Name, Partnumber and Type: cannot be renamed or deleted
	static bool IsStandardField(const CString& name);
	// Add Partnumber and Type (empty) after Name if missing
	void EnsureStandardFields();

	// A parameter's choice list ("a; b; c"): the values its drop-down
	// offers; any other value can still be typed.  In the module library
	// it is a hidden attribute "$$CHOICES:<parameter>" next to the
	// parameter (older builds hide "$$" attributes).
	static void SplitChoices(const CString& choices, std::vector<CString>& list);
	static CString NormalizeChoices(const CString& choices); // trimmed, no empty or duplicate entries
	static CString ChoicesAttributeName(const CString& field);
	// True for a "$$CHOICES:" attribute; field = the parameter it belongs to
	static bool IsChoicesAttribute(const CString& name, CString* field = NULL);
	// The field's choices, if it has a field of that name (case-insensitive)
	void SetChoices(const CString& field, const CString& choices);

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
