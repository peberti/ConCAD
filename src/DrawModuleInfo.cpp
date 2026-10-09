/*
 * ConCAD: the parameters of a placed module
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#include "stdafx.h"
#include "ConCadDoc.h"
#include "DrawModuleInfo.h"

CDrawModuleInfo::CDrawModuleInfo(CConCadDoc *pDesign) :
	CDrawingObject(pDesign)
{
	Field name;
	name.name = _T("Name");
	Field ref;
	ref.name = _T("Reference");
	m_fields.push_back(name);
	m_fields.push_back(ref);
}

void CDrawModuleInfo::SaveXML(CXMLWriter &xml)
{
	xml.addTag(GetXMLTag());
	for (size_t i = 0; i < m_fields.size(); i++)
	{
		xml.addTag(_T("FIELD"));
		xml.addAttribute(_T("name"), m_fields[i].name);
		xml.addAttribute(_T("value"), m_fields[i].value);
		xml.closeTag();
	}
	xml.closeTag();
}

void CDrawModuleInfo::LoadXML(CXMLReader &xml)
{
	m_fields.clear();
	xml.intoTag();
	CString tag;
	while (xml.nextTag(tag))
	{
		if (tag == _T("FIELD"))
		{
			Field f;
			xml.getAttribute(_T("name"), f.name);
			xml.getAttribute(_T("value"), f.value);
			m_fields.push_back(f);
		}
	}
	xml.outofTag();

	// Name and Reference always come first
	while (m_fields.size() < FixedFields)
	{
		Field f;
		f.name = m_fields.empty() ? _T("Name") : _T("Reference");
		m_fields.push_back(f);
	}
}

CDrawingObject* CDrawModuleInfo::Store()
{
	CDrawModuleInfo *NewObject = new CDrawModuleInfo(m_pDesign);
	*NewObject = *this;
	m_pDesign->Add(NewObject);
	return NewObject;
}

CString CDrawModuleInfo::Resolve(const CString &s) const
{
	if (s.Find(_T('{')) < 0)
	{
		return s;
	}

	CString result;
	int i = 0;
	const int len = s.GetLength();
	while (i < len)
	{
		int j = s.Find(_T('}'), i + 1);
		if (s[i] == _T('{') && j > i)
		{
			CString name = s.Mid(i + 1, j - i - 1);
			name.Trim();
			bool found = false;
			for (size_t f = 0; f < m_fields.size(); f++)
			{
				if (m_fields[f].name.CompareNoCase(name) == 0)
				{
					result += m_fields[f].value;
					found = true;
					break;
				}
			}
			if (found)
			{
				i = j + 1;
				continue;
			}
		}
		result += s[i];
		++i;
	}
	return result;
}
