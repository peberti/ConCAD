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
	m_excludeFromBom = false;
	Field name;
	name.name = _T("Name");
	m_fields.push_back(name);
}

void CDrawModuleInfo::SaveXML(CXMLWriter &xml)
{
	xml.addTag(GetXMLTag());
	if (m_excludeFromBom)
	{
		xml.addAttribute(_T("bom"), _T("exclude")); // only when set: older builds ignore it
	}
	for (size_t i = 0; i < m_fields.size(); i++)
	{
		xml.addTag(_T("FIELD"));
		xml.addAttribute(_T("name"), m_fields[i].name);
		xml.addAttribute(_T("value"), m_fields[i].value);
		if (!m_fields[i].choices.IsEmpty())
		{
			xml.addAttribute(_T("choices"), m_fields[i].choices); // only when used: older builds ignore it
		}
		xml.closeTag();
	}
	xml.closeTag();
}

void CDrawModuleInfo::LoadXML(CXMLReader &xml)
{
	m_fields.clear();
	CString bom;
	m_excludeFromBom = xml.getAttribute(_T("bom"), bom) && bom.CompareNoCase(_T("exclude")) == 0;
	xml.intoTag();
	CString tag;
	while (xml.nextTag(tag))
	{
		if (tag == _T("FIELD"))
		{
			Field f;
			xml.getAttribute(_T("name"), f.name);
			xml.getAttribute(_T("value"), f.value);
			xml.getAttribute(_T("choices"), f.choices);
			m_fields.push_back(f);
		}
	}
	xml.outofTag();

	// Name always comes first
	if (m_fields.empty())
	{
		Field f;
		f.name = _T("Name");
		m_fields.push_back(f);
	}
	// Modules no longer have a Reference: files saved before always have
	// one after the Name.  An empty one is dropped; one with a value stays
	// as an ordinary field, so texts showing {Reference} keep their value.
	if (m_fields.size() > 1 && m_fields[1].name.CompareNoCase(_T("Reference")) == 0 && m_fields[1].value.IsEmpty())
	{
		m_fields.erase(m_fields.begin() + 1);
	}
	EnsureStandardFields();
}

bool CDrawModuleInfo::IsStandardField(const CString& name)
{
	return name.CompareNoCase(_T("Name")) == 0 || name.CompareNoCase(_T("Partnumber")) == 0 || name.CompareNoCase(_T("Type")) == 0;
}

void CDrawModuleInfo::EnsureStandardFields()
{
	const TCHAR* const standard[] = { _T("Partnumber"), _T("Type") };
	size_t pos = FixedFields;
	for (int s = 0; s < 2; s++)
	{
		bool found = false;
		for (size_t i = 0; i < m_fields.size(); i++)
		{
			if (m_fields[i].name.CompareNoCase(standard[s]) == 0)
			{
				found = true;
				pos = i + 1;
				break;
			}
		}
		if (!found)
		{
			Field f;
			f.name = standard[s];
			pos = min(pos, m_fields.size());
			m_fields.insert(m_fields.begin() + pos, f);
			++pos;
		}
	}
}

static const TCHAR ChoicesPrefix[] = _T("$$CHOICES:");

void CDrawModuleInfo::SplitChoices(const CString& choices, std::vector<CString>& list)
{
	list.clear();
	int pos = 0;
	while (pos >= 0 && pos <= choices.GetLength())
	{
		const int end = choices.Find(_T(';'), pos);
		CString c = end < 0 ? choices.Mid(pos) : choices.Mid(pos, end - pos);
		c.Trim();
		bool dup = c.IsEmpty();
		for (size_t i = 0; i < list.size() && !dup; i++)
		{
			dup = list[i] == c;
		}
		if (!dup)
		{
			list.push_back(c);
		}
		pos = end < 0 ? -1 : end + 1;
	}
}

CString CDrawModuleInfo::NormalizeChoices(const CString& choices)
{
	std::vector<CString> list;
	SplitChoices(choices, list);
	CString result;
	for (size_t i = 0; i < list.size(); i++)
	{
		if (i > 0)
		{
			result += _T("; ");
		}
		result += list[i];
	}
	return result;
}

CString CDrawModuleInfo::ChoicesAttributeName(const CString& field)
{
	return CString(ChoicesPrefix) + field;
}

bool CDrawModuleInfo::IsChoicesAttribute(const CString& name, CString* field)
{
	const int len = (int)_tcslen(ChoicesPrefix);
	if (name.GetLength() <= len || name.Left(len).CompareNoCase(ChoicesPrefix) != 0)
	{
		return false;
	}
	if (field != NULL)
	{
		*field = name.Mid(len);
	}
	return true;
}

void CDrawModuleInfo::SetChoices(const CString& field, const CString& choices)
{
	for (size_t i = 0; i < m_fields.size(); i++)
	{
		if (m_fields[i].name.CompareNoCase(field) == 0)
		{
			m_fields[i].choices = NormalizeChoices(choices);
			return;
		}
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
