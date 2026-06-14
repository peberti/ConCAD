/*
 * Project:		TinyCAD program for schematic capture
 *				https://www.tinycad.net
 * Copyright:	© 1994-2019 Matt Pyne
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *				http://www.opensource.org/licenses/lgpl-license.html
 */

#pragma once

#include "NewTypes.h"
#include "XMLWriter.h"
#include "XMLReader.h"

class CConCadDoc;

typedef Int16 hRESOURCE;

//*************************************************************************
//*                                                                       *
//*         The resource settings                                         *
//*                                                                       *
//*************************************************************************
class CDocResource
{
	typedef std::list<CDocResource*> TResourceList;
	typedef TResourceList::iterator TResourceListIterator;

	//-- The identifier for this resource
private:
	hRESOURCE ResourceNumber;

	//-- Used during merging of two resource tables
private:
	hRESOURCE MergeNumber;

	//-- Used during Garbage collection
private:
	bool InUse;

	//-- The Next font in this list
public:
	CDocResource* next;

public:
	//=====================================================================
	//== ctor/dtor/initializing                                          ==
	//=====================================================================
public:
	CDocResource();
public:
	virtual ~CDocResource();

	//=====================================================================
	//== accessor/mutator                                                ==
	//=====================================================================
public:
	hRESOURCE GetResourceNumber() const;

	//=====================================================================
	//==                                                                 ==
	//=====================================================================
	//-- Convert an old resource number into a new resource number
public:
	hRESOURCE ConvertOld(hRESOURCE n);

	//-- Add a new resource to this list
public:
	hRESOURCE Add(CDocResource *, hRESOURCE = -1);

	//-- Get a resource from the list
public:
	CDocResource* Get(hRESOURCE);

	//-- The Garbage collection facilities
public:
	void Tag(hRESOURCE);
public:
	void UnTag();

	//-- Clear the merge numbers
public:
	void ResetMerge();

public:
	void SaveXML(CConCadDoc *pDesign, const TCHAR *name, CXMLWriter &xml);
public:
	void Load(CConCadDoc *pDesign, CStream &);

	//-- Compare this resource with another resource
public:
	virtual bool Comparex(const CDocResource*) const;
public:
	virtual BOOL Compare(CDocResource*) = 0;

	//=====================================================================
	//==  The virtual parts of the resource                              ==
	//=====================================================================

	//--
public:
	virtual void SaveItemXML(CConCadDoc *pDesign, CXMLWriter &xml) = 0;

	//--
public:
	virtual void LoadItem(CConCadDoc *pDesign, CStream &, hRESOURCE) = 0;

	//--
public:
	virtual void LoadItemXML(CConCadDoc *pDesign, CXMLReader &xml) = 0;
};
//=========================================================================
