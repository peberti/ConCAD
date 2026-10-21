/*
 TinyCAD program for schematic capture
 Copyright 1994/1995/2002,2003 Matt Pyne.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "stdafx.h"
#include <set>
#include <map>

#include "ConCadView.h"
#include "ConCad.h"
#include "special.h"
#include "library.h"
#include "net.h"
#include "registry.h"
#include "diag.h"
#include "LineUtils.h"
#include "ConCadMultiDoc.h"
#include "MainFrm.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include "rapidxml-1.13/rapidxml.hpp"
#include "rapidxml-1.13/rapidxml_print.hpp"

void CImportFile::assignContext(const CollectionMemberReference<CImportFile *> parent, CString myReference)
{
	m_parent = parent;
	CImportFile *pparent = parent.getObjectWithDefault(NULL);
	if (pparent == NULL)
	{
		m_RefContext = myReference;
	}
	else
	{
		m_RefContext = pparent->m_RefContext;
		m_RefContext += "/";
		m_RefContext += myReference;
	}
}

/// Construction/destruction

CNetList::CNetList()
{
	m_CurrentNet = 1;
	m_prefix_references = FALSE;
	m_follow_imports = TRUE;
	m_prefix_import = FALSE;
	m_err_file = NULL;
	m_errors = 0;
}

CNetList::~CNetList()
{
	/// Delete the imports...
	clear();
}

/**
 * Clear out the imports etc..
 */
void CNetList::clear()
{
	fileCollection::iterator fi = m_imports.begin();
	while (fi != m_imports.end())
	{
		delete *fi;
		++fi;
	}
	m_imports.clear();
	m_nodes.clear();
	if (m_err_file)
	{
		fclose(m_err_file);
	}
	m_err_file = NULL;
}

/**
 * Write the to error file.
 * 
 * @param str
 */
void CNetList::writeError(const _TCHAR *str, ...)
{
	va_list argptr;

	TCHAR buffer[1024];
	++m_errors;

	va_start( argptr, str );
	_vsntprintf_s(buffer, sizeof (buffer) / sizeof(TCHAR), str, argptr);
	_ftprintf(m_err_file, _T("Netlist error #%d:  %s"), m_errors, buffer);
}

/**
 * Open the error file.
 * 
 * @param filename
 */
void CNetList::createErrorFile(const TCHAR *filename)
{
	/// Open the filename for the results
	m_err_filename = filename;
//	int brk = m_err_filename.ReverseFind('\\');
	int brk = m_err_filename.ReverseFind('.');
	if (brk)
	{
		m_err_filename = m_err_filename.Left(brk);
	}
	m_err_filename += _T(".txt");

	errno_t err;

	err = _tfopen_s(&m_err_file, m_err_filename, _T("w"));
	m_errors = 0;
	if (!m_err_file)
	{
		if (static_cast<CMainFrame*>((static_cast<CConCadApp*>(AfxGetApp())->m_pMainWnd))->runAsConsoleApp)
		{	//in console mode, also output the error message to stderr, wherever that might be pointed
			_ftprintf(stderr, _T("ConCAD command error:  Cannot open file %s for writing.  Make sure volume is not write protected or full and that sufficient permission is present for writing to this location.\n"), (LPCTSTR)m_err_filename);
		}
		else
		{
			Message(IDS_CANNOTOPEN);
			return;
		}
	}
}

/**
 * Open the error file in a text view.
 * 
 * @param force
 */
void CNetList::reopenErrorFile(bool force)
{
	_ftprintf(m_err_file, _T("\n%d %s found\n"), m_errors, m_errors == 1 ? _T("error") : _T("errors"));
	fclose(m_err_file);

	if (static_cast<CMainFrame*>((static_cast<CConCadApp*>(AfxGetApp())->m_pMainWnd))->runAsConsoleApp)
	{	//in console mode, also output the error message to stderr, wherever that might be pointed
		if (m_errors != 0) 
		{
			_ftprintf(stderr, _T("\n%d %s found\n"), m_errors, m_errors == 1 ? _T("error") : _T("errors"));
		}
	}
	else
	{
		if (force || (m_errors > 0))
		{
			CConCadApp::EditTextFile(m_err_filename);
		}
	}
}

/** This method assigns netlist numbers which may or may not get concatenated later with
 * an alpha prefix to form a net name.  
 * 
 * Many users have requested that net number be preserved and never get reused or reassigned once
 * assigned initially.  This method would be involved in fixing this problem.
 * 
 * @return the next available net number
 */
int CNetList::GetNewNet()
{
	return m_CurrentNet++;
}

/**
 * Add a node to the tree (if necessary), return value of the netlist.
 * 
 * @param ins
 * @return
 */
int CNetList::Add(CNetListNode &ins)
{
	/// Is this a node without connection point?
	if (!ins.m_a.hasValue())
	{
		/// Always assign a new net-list index
		ins.m_NetList = GetNewNet();
		m_nets[ins.m_NetList].push_back(ins);
		return ins.m_NetList;
	}
	/// Has this node already been assigned a net-list index?
	else if (ins.m_NetList == -1)
	{
		/// Is this node already in the tree?
		int found = m_nodes[ins.m_a];
		/// No, so we can add without checking for prior connections...
		if (found != 0)
		{
			ins.m_NetList = found;
		}
		else
		{
			ins.m_NetList = GetNewNet();
			m_nodes[ins.m_a] = ins.m_NetList;
		}

		m_nets[ins.m_NetList].push_back(ins);

		return ins.m_NetList;
	}
	else
	{
		/// Is this node already in the tree?
		int found = m_nodes[ins.m_a];
		/// If this node was already found, but with a different netlist
		/// number, then first we must join the two netlists together
		if (found != 0 && found != ins.m_NetList)
		{
			/// The two nets must be joined
			int NewList = ins.m_NetList;
			int OldList = found;

			/// Get the old list
			nodeVector::iterator OldNetList = m_nets[OldList].begin();

			/// Make the old list's net-list index the same as the new list
			while (OldNetList != m_nets[OldList].end())
			{
				(*OldNetList).m_NetList = NewList;
				//	This line has a bug:
				//	m_nodes[ (*OldNetList).m_a] = NewList;
				//
				// Bugfix contributed by Chunk.Lee (see TinyCAD Developer Discussions on TinyCAD web site dated 2013-12-18
				// BUG fixed: "(*OldNetList).m_a" has no value if (OldNetList) generated by a power pin!!
				if ((*OldNetList).m_a.hasValue()) m_nodes[(*OldNetList).m_a] = NewList;
				++OldNetList;
			}

			/// Now concatenate the old lists onto the new list
			std::copy(m_nets[OldList].begin(), m_nets[OldList].end(), std::back_inserter(m_nets[NewList]));

			/// Delete the old list
			netCollection::iterator it = m_nets.find(OldList);
			m_nets.erase(it);

			ins.m_NetList = NewList;
		}

		/// Add this node to the netlist
		m_nets[ins.m_NetList].push_back(ins);
		m_nodes[ins.m_a] = ins.m_NetList;

		return ins.m_NetList;
	}
}

/** 
 * Used only during debugging the netlister.  This method formats
 * the netlist objects and outputs them to the debugger output.
 */
void CNetList::dumpNetListObjects()
{
	/// Scan each netlist
	TRACE("\nEntering CNetList::DumpNetListObjects():\n");
	netCollection::iterator ni = m_nets.begin();
	while (ni != m_nets.end())
	{
		int net = (*ni).first;
		nodeVector &v = (*ni).second;
		/// Dump each node in the netlist
		nodeVector::iterator vi = v.begin();

		TRACE("\n  ==>Dumping objects for net=%d, preferred net name=\"%S\"\n", net, (*vi).getPreferredLabel());

		while (vi != v.end())
		{
			enum ObjType objectType;
			CString objectNameString;

			CNetListNode &node = *vi;

			if (node.m_parent)
			{
				objectType = node.m_parent->GetType();
				switch (objectType)
				{
					case xAnnotation:
						objectNameString = _T("xAnnotation");
						break;
					case xSymbol:
						objectNameString = _T("xSymbol");
						break;
					case xSymbolEx:
						objectNameString = _T("xSymbolEx");
						break;
					case xSymbolEx2:
						objectNameString = _T("xSymbolEx2");
						break;
					case xBusName:
						objectNameString = _T("xBusName");
						break;
					case xNoConnect:
						objectNameString = _T("xNoConnect");
						break;
					case xJunction:
						objectNameString = _T("xJunction");
						break;
					case xLabel:
						objectNameString = _T("xLabel");
						break;
					case xWire:
						objectNameString = _T("xWire");
						break;
					case xBus:
						objectNameString = _T("xBus");
						break;
					case xPower:
						objectNameString = _T("xPower");
						break;
					case xPin:
						objectNameString = _T("xPin");
						break;
					case xLabelEx:
						objectNameString = _T("xLabelEx");
						break;
					case xLabelEx2:
						objectNameString = _T("xLabelEx2");
						break;
					case xPinEx:
						objectNameString = _T("xPinEx");
						break;
					case xLine:
						objectNameString = _T("xLine");
						break;
					case xHierarchicalSymbol:
						objectNameString = _T("xHierarchicalSymbol");
						break;
					default:
						objectNameString = _T("Unknown");
						break;
				}
			}
			if (node.m_parent && node.m_parent->GetType() == xWire)
			{
//				Skip nodes of type=xWire in dump as there are typically a lot and they don't particularly add value when debugging the netlist
//				TRACE("    ==>Object:  xWire node\n");
			}
			else if (node.m_parent && node.m_parent->GetType() == xLabelEx2)
			{
				TRACE("    ==>Object:  xLabelEx2      =\"%S\" on m_net=%d, context hierarchy level=%d\n", static_cast<CDrawLabel*> (node.m_parent)->GetValue(), node.m_NetList, node.getFileNameIndex());
			}
			else if (node.m_parent && node.m_parent->GetType() == xPower)
			{
				CString powerLabel = get_power_label((CDrawPower *) node.m_parent);
				TRACE("    ==>Object:  xPower         =\"%S\" on m_net=%d, context hierarchy level=%d\n", powerLabel, node.m_NetList, node.getFileNameIndex());
			}
			else if (!node.m_parent)
			{
				TRACE("    ==>Object:  xJunction(Hard) on m_net=%d, context hierarchy level=%d, m_parent->GetType=NULL, net label=\"%S\", refdes=\"%S\", pin number=\"%S\"\n", node.m_NetList, node.getFileNameIndex(), node.getLabel(), node.m_reference, node.m_pin);
			}
			else
			{
				TRACE("    ==>Object:  %-15S on m_net=%d, context hierarchy level=%d, m_parent->GetType=%d, net label=\"%S\", refdes=\"%S\", pin number=\"%S\"\n", objectNameString, node.m_NetList, node.getFileNameIndex(), node.m_parent->GetType(), node.getLabel(), node.m_reference, node.m_pin);
			}

			++vi;
		}

		++ni;
	}
	TRACE("Exiting CNetList::DumpNetListObjects()\n\n");
}

// Use the netlist hints (in the symbols) to re-number to netlist
void CNetList::ApplyHints()
{
	/// Build a re-mapping from the generated netlist numbers to the hinted netlist numbers
	std::map<int, int>	remapping;
	std::set<int> used_netlists;
	int next_unused_netlist = 1;

	/// Scan each netlist
	netCollection::iterator ni = m_nets.begin();
	while (ni != m_nets.end())
	{
		nodeVector &v = (*ni).second;

		/// Update the nodes in the netlist
		nodeVector::iterator vi = v.begin();
		while (vi != v.end())
		{
			CNetListNode &node = *vi;
			if (node.m_pMethod)
			{
				// Does this node have a hint for this netlist?
				int hint = node.m_pMethod->getNetlistHint(node.m_pin);
				if (hint != 0)
				{
					// Is there already a remapping for this netlist?
					bool remap = true;
					if (remapping.find(node.m_NetList) != remapping.end())
					{
						// Yes, so which one is better?
						if (remapping[node.m_NetList] <= hint)
						{
							// Smaller number wins, so no remapping today
							remap = false;
						}
					}

					if (remap && used_netlists.find(hint) != used_netlists.end())
					{
						// This netlist number is already in use
						remap = false;
					}

					if (remap)
					{
						// Ok, remap
						remapping[node.m_NetList] = hint;

						TRACE("%d => %d\n", node.m_NetList, hint);

						// Generate the correct unused netlist number
						used_netlists.insert(hint);
						next_unused_netlist = max(next_unused_netlist, node.m_NetList + 1);
						next_unused_netlist = max(next_unused_netlist, hint + 1);
					}
				}
			}
			++vi;
		}

		++ni;
	}

	/// Remap each netlist
	ni = m_nets.begin();
	while (ni != m_nets.end())
	{
		nodeVector &v = (*ni).second;

		/// Update the nodes in the netlist
		nodeVector::iterator vi = v.begin();
		while (vi != v.end())
		{
			CNetListNode &node = *vi;

			auto remap = remapping.find(node.m_NetList);
			if (remap == remapping.end())
			{
				remapping[node.m_NetList] = next_unused_netlist;
				TRACE("%d => %d\n", node.m_NetList, next_unused_netlist);

				++next_unused_netlist;
				remap = remapping.find(node.m_NetList);
			}

			node.m_NetList = remap->second;

			if (node.m_pMethod)
			{
				node.m_pMethod->setNetlistHint(node.m_pin, node.m_NetList);
			}
			++vi;
		}

		++ni;
	}
}

/** 
 * Tell all of the wires what network they are associated with.
 */
void CNetList::WriteWires()
{
	/// Scan each netlist
//	TRACE("\nEntering CNetList::WriteWires():  Updating the wires in this design\n");
	netCollection::iterator ni = m_nets.begin();
	while (ni != m_nets.end())
	{
		int net = (*ni).first;
//		TRACE("  ==>Updating wires for net=%d\n",net);

		nodeVector &v = (*ni).second;

		/// Update the nodes in the netlist
		nodeVector::iterator vi = v.begin();
		while (vi != v.end())
		{
//			enum ObjType objectType;	//Used for debugging only
//			CString objectNameString;	//Used for debugging only


			CNetListNode &node = *vi;
#if 0	//enable if you want to debug the netlist
			if (node.m_parent)
			{
				objectType = node.m_parent->GetType();
				switch(objectType)
				{
					case xAnnotation: 
						objectNameString = _T("xAnnotation");
						break;
					case xSymbol: 
						objectNameString = _T("xSymbol");
						break;
					case xSymbolEx: 
						objectNameString = _T("xSymbolEx");
						break;
					case xSymbolEx2: 
						objectNameString = _T("xSymbolEx2");
						break;
					case xBusName: 
						objectNameString = _T("xBusName");
						break;
					case xNoConnect: 
						objectNameString = _T("xNoConnect");
						break;
					case xJunction: 
						objectNameString = _T("xJunction");
						break;
					case xLabel: 
						objectNameString = _T("xLabel");
						break;
					case xWire: 
						objectNameString = _T("xWire");
						break;
					case xBus: 
						objectNameString = _T("xBus");
						break;
					case xPower: 
						objectNameString = _T("xPower");
						break;
					case xPin: 
						objectNameString = _T("xPin");
						break;
					case xLabelEx: 
						objectNameString = _T("xLabelEx");
						break;
					case xLabelEx2: 
						objectNameString = _T("xLabelEx2");
						break;
					case xPinEx: 
						objectNameString = _T("xPinEx");
						break;
					case xLine: 
						objectNameString = _T("xLine");
						break;
					case xHierarchicalSymbol: 
						objectNameString = _T("xHierarchicalSymbol");
						break;
					default: 
						objectNameString = _T("Unknown");
						break;
				}
			}
#endif
			if (node.m_parent && node.m_parent->GetType() == xWire)
			{

				//This is the only statement in this function that actually does work
				static_cast<CDrawLine*> (node.m_parent)->setNetwork(net);

//				TRACE("    ==>Updating xWire node\n");
			}
#if 0	//enable if you want to debug the netlist
			else if (node.m_parent && node.m_parent->GetType() == xLabelEx2)
			{
				TRACE("    ==>Skipping xLabelEx2=\"%S\" on net=%d\n", static_cast<CDrawLabel*>(node.m_parent)->GetValue(), node.m_NetList);
			}
			else if (node.m_parent && node.m_parent->GetType() == xPower)
			{
				CString powerLabel = get_power_label((CDrawPower *) node.m_parent);
				TRACE("    ==>Skipping xPower=\"%S\" on m_net=%d\n",powerLabel,node.m_NetList);
			}
			else if (!node.m_parent)
			{
				TRACE("    ==>Skipping %S node (type=xJunction(Hard)) on m_net=%d, refdes=\"%S\", pin number=\"%S\"\n",
				objectNameString, node.m_NetList, node.m_reference, node.m_pin);
			}
			else
			{
				TRACE("    ==>Skipping %S node (type=%d) on m_net=%d, refdes=\"%S\", pin number=\"%S\"\n",
						objectNameString, node.m_parent->GetType(),node.m_NetList, node.m_reference, node.m_pin);
			}
#endif
			++vi;
		}

		++ni;
	}
//	TRACE("Exiting CNetList::WriteWires()\n\n");
}

/**
 * Link together several netlists.
 * This is done by linking together:
 * nets that have nodes that have the same filename and same label name
 * 
 * @param nets
 */
void CNetList::Link(linkCollection& nets)
{
//	TRACE("CNetList::Link():  Entering the net node linker.  nets.size()=%d\n", nets.size());
	/// Get rid of any old data
	m_CurrentNet = 1;
	m_nodes.erase(m_nodes.begin(), m_nodes.end());
	m_nets.erase(m_nets.begin(), m_nets.end());

	/// Here is a list of known nodes and their netlist number
	typedef std::map<CString, int> stringCollection;
	stringCollection labels;

	/// Here is a list of known linking information
	linkMap map;

	/// The linkmap will contain for each netlist a map
	/// of old net number to new net number...
	map.resize(nets.size()); //size the map to be 1 map entry for each net in the specified netlist

	/// Now scan the nodes to generate the superset of
	/// labels and their netlists

//	TRACE("\n\n\n  ==>Pass 1:  Scanning all netlist nodes to generate the superset of labels and their linking information.\n");
	linkCollection::iterator i = nets.begin();
	while (i != nets.end()) //this loop traverses a collection of collections of nets organized by which sheet they originated on
	{
		CNetList &n = *i;
		// Dump the current net
//		TRACE("\n  ==>Linker pass 1:  Traversing linkCollection:  m_CurrentNet = %d, n.m_CurrentNet = %d, follow imports = %S\n",
//				m_CurrentNet,
//				n.m_CurrentNet,
//				(n.m_follow_imports) ? _T("True"):_T("False"));
		/// Scan each netlist
		netCollection::iterator ni = n.m_nets.begin();
		while (ni != n.m_nets.end()) //this loop traverses a collection of nets originally found on a single sheet
		{
			nodeVector &v = (*ni).second;
			//int old_netlist = (*ni).first; //Uncomment for debugging the netlist!
			int new_netlist = 0;

//			TRACE("\n    ==>Linker pass 1:  Traversing node in netlist:  node old_netlist = %d, node new_netlist = %d\n", old_netlist, new_netlist);
			/// Scan each node
			nodeVector::iterator vi = v.begin();
			while (vi != v.end()) //this loop traverses a collection of schematic objects associated with a single net originally from a single sheet
			{
				CNetListNode &node = *vi;
//				TRACE("      ==>Linker pass 1:  Traversing node vector:  Netlist %d Node name=\"%S\" from sheet %d, refdes=\"%S\", pin=\"%S\"\n",
//					old_netlist, node.getLabel(), node.m_sheet, node.m_reference, node.m_pin);

				/// Look for nodes with labels
				if (!node.getLabel().IsEmpty())
				{
					CString label_name = node.getLabel();

					/// Do we already have an entry for this label?
					stringCollection::iterator s = labels.find(label_name);
//					TRACE("        ==>Associated label=\"%S\"\n", label_name);
					if (s != labels.end())
					{
						/// Get netlist of label
						int q = (*s).second;
//						TRACE("        ==>Found an entry for label \"%S\" in labels table associated with netlist %d\n",
//							label_name,
//							(*s).second);

						/// Have we already assigned a netlist to this node?
						if (new_netlist == 0)
						{
							/// Yes... so this node must become that netlist
							new_netlist = q;
						}

						// Update netlist of label
						if (new_netlist != q)
						{
//							TRACE("Updating all labels of old netlist (=%d) with new netlist (=%d)\n",old_netlist, new_netlist);
							// Update all labels of old netlist with new netlist
							stringCollection::iterator li = labels.begin();
							while (li != labels.end())
							{
								if ( (*li).second == q)
								{
									labels[ (*li).first] = new_netlist;
								}
								li++;
							}
						}
					}
					else
					{
						/// No, so we must insert a new label node..
						if (new_netlist == 0)
						{
							new_netlist = m_CurrentNet++;
						}

						labels[label_name] = new_netlist;
					}

				}

				++vi;
			}

			++ni;
		}

		++i;
	}

	/// Now scan the nets and generate a
	/// mapping to the super net
	int index = 0;
	i = nets.begin();
	while (i != nets.end())
	{
		CNetList &n = *i;

		/// Scan each netlist
		netCollection::iterator ni = n.m_nets.begin();
		while (ni != n.m_nets.end())
		{
			nodeVector &v = (*ni).second;
			int old_netlist = (*ni).first;
			int new_netlist = 0;

			/// Scan each node
			nodeVector::iterator vi = v.begin();
			while (vi != v.end())
			{
				CNetListNode &node = *vi;

				if (!node.getLabel().IsEmpty())
				{
					/// We only link at label level...
					CString label_name = node.getLabel();

					/// Get netlist of label (should always be found)
					stringCollection::iterator s = labels.find(label_name);
					if (s != labels.end())
					{
						int q = (*s).second;

						/// Have we already assigned a netlist to this node?
						if (new_netlist == 0)
						{
							/// Yes... so this node must become that netlist
							new_netlist = q;
						}
					}
				}

				++vi;
			}

			/// Did we find a new netlist number for this node?
			if (new_netlist == 0)
			{
				new_netlist = m_CurrentNet++;
//				TRACE("    ==>Added new_netlist=%d for this node.\n", new_netlist);
			}

			/// Now write it into the map
			map[index][old_netlist] = new_netlist;
//			TRACE("    ==>Mapping new_netlist=%d onto map[index=%d][old_netlist=%d]\n", new_netlist, index, old_netlist);

			++ni;
		}

		++index; //prepare to process the collection of nets from the next schematic sheet
		++i; //prepare to process the collection of nets from the next schematic sheet
	}

	//	Uncomment for debugging the netlist
//	TRACE("\n\n\nDump of netlist objects after Linker pass 1 (nets.size() = %d):\n", nets.size());
//	CNetList::dumpNetListObjects();
//	TRACE("Dump complete.\n\n\n");


	/// Now build a super net of all of the
	/// netlists linked together...
	index = 0;
	i = nets.begin();
	int sheetIndex = 0;
//	TRACE("\n\n\n  ==>Linker pass 2:  Now building a super net of all of the netlists linked together from each schematic page (nets.size()=%d\n",nets.size());
	while (i != nets.end()) //this loop traverses a collection of collections of nets organized by which sheet they originated on
	{
		++sheetIndex; //used for debugging only
		CNetList &n = *i;
//		TRACE("\n  ==>Linker pass 2:  Traversing linkCollection, sheetIndex = %d, nets.size() = %d, node count = %d:  Current net = %d, follow imports = %S\n",
//				sheetIndex,
//				nets.size(),
//				n.m_nets.size(),
//				n.m_CurrentNet,
//				(n.m_follow_imports) ? _T("True"):_T("False"));

		//Uncomment for debugging the netlist
//		TRACE("\n\n\nDump of netlist objects in the middle of linker pass 2 (sheetIndex = %d, nets.size() = %d, node count = %d) just prior to processing current net=%d:\n",
//				sheetIndex,
//				nets.size(),
//				n.m_nets.size(),
//				n.m_CurrentNet);
//		CNetList::dumpNetListObjects();
//		TRACE("Dump complete.\n\n\n");


		/// Scan each netlist in the collection of nets from a specific schematic sheet
		netCollection::iterator ni = n.m_nets.begin();
		while (ni != n.m_nets.end()) //this loop traverses a collection of nets originally found on a single sheet.  Note that these nets may actually be net fragments at this point.
		{
			nodeVector &v = (*ni).second; //get the list of schematic objects for this net
			int old_netlist = (*ni).first; //get the net number for this net
			int new_netlist = map[index][old_netlist];
//			TRACE("\n    ==>Linker pass 2 (sheetIndex = %d):  Traversing node in netlist:  node old_netlist = %d, node new_netlist = %d, m_CurrentNet = %d\n", sheetIndex, old_netlist, new_netlist, n.m_CurrentNet);

			/// Update the nodes in the netlist
			nodeVector::iterator vi = v.begin();
			//			TRACE("      ==>Linker pass 2 (sheetIndex = %d):  Updating the nodes in new_netlist=%d\n", sheetIndex, new_netlist);
			while (vi != v.end())
			{
//				CNetListNode &node = *vi;	//This statement is for TRACE debug only
//				TRACE("      ==>Linker pass 2:  Traversing node vector:  old_netlist=%d, new_netlist=%d, Node name=\"%S\" from sheet %d, refdes=\"%S\", pin=\"%S\"\n",
//					old_netlist, new_netlist, node.getLabel(), node.m_sheet, node.m_reference, node.m_pin);
				vi->m_NetList = new_netlist;
				++vi;
			}

			/// Now concatenate the old lists onto the new list
//			TRACE("    ==>Pass 2 (sheetIndex=%d, nets.size()=%d, node count=%d):  Now concatenating all of the nodes in this netlist together.  n.m_CurrentNet = %d\n",
//					sheetIndex,
//					nets.size(),
//					n.m_nets.size(),
//					n.m_CurrentNet);

			std::copy(v.begin(), v.end(), std::back_inserter(m_nets[new_netlist]));

			//Uncomment to debug the netlist
//			TRACE("\n\n\nDump of netlist objects in the middle of Linker pass 2 (sheetIndex=%d, nets.size()=%d, node count=%d) just after the concatenation of all of the nodes. n.m_CurrentNet=%d:\n",
//					sheetIndex,
//					nets.size(),
//					n.m_nets.size(),
//					n.m_CurrentNet);
//			CNetList::dumpNetListObjects();
//			TRACE("Dump complete.\n\n\n");

			++ni;
		}

		++index;
		++i;
	}
//	TRACE("  ==>Linker pass 2:  Finished building the super netlist.  nets.size() = %d\n\n\n", nets.size());

	//Uncomment to debug the netlist
//	TRACE("\n\n\nDump of netlist objects contained in m_nets after pass 2:\n");
//	CNetList::dumpNetListObjects();
//	TRACE("Dump complete.\n\n\n");
	TRACE("  -->Linker pass 3:  Determine which of several possible net names to assign to each net in this final netlist\n");

//	struct t_NetNodeDetails {
//		CString constructedNetName;	//This represents one of potentially many possible net names (i.e., 1 possible name for almost every node object in each net)
//		int hierarchical_context_level;	//The hierarchical level that a possible net name occurs at is one of the final net name decision factors
//		enum ObjType obj_type;	//Some types of objects will be given priority over other types of objects, so the object type must be stored for later use.
//		enum NetType net_type;	//Some types of objects will be given priority over other types of objects based on the overall electrical type of the net
//		CString baseNetName;	//The undecorated base name that the preferred net name might be based on - this is either a pin name or a local label name.
//		CString partialReferencePath;	//The partial reference designator path used to navigate to this net node object - does not include the final reference designator
//		CString referenceDesignator;	//Will be empty for some object types such as labels that aren't associated with a symbol.
//	} netNodeDetails;
//	std::map<int priority, t_NetNodeDetails* netNodeDetails> prioritizedNetNames;

	netCollection::iterator ni = m_nets.begin();
	while (ni != m_nets.end())
	{
		int net = (*ni).first;
		TRACE("\n    ==>Extracting potential net names for objects in net=%d\n", net);

		nodeVector &v = (*ni).second;

		/// Collect the net names from the nodes in the netlist.
		/// Potential net names are considered from hierarchical pins with generated net names, net name labels, and net numbers when no other possibilities are present.
		/// The first choice from the lowest numbered file name index is the goal.  While the file name index is not exactly the hierarchical level
		/// that the label or pin occurred on, it is proportional to that hierarchical level.  0 is the topmost level.
		CString preferredNetName = "";
		int preferredNetNameLevel = 99999999; //this must only be larger than the largest number of files contained in the file index which is at least as large as the highest number of hierarchical levels in the schematic design
		enum ObjType preferredNetNameType = xNULL;

		nodeVector::iterator vi = v.begin();
		nodeVector::iterator vi_saved;
		while (vi != v.end())
		{
			CNetListNode &node = *vi;
			vi_saved = vi; //save a copy of the last value of vi used for use outside of the loop in order to set the final net name.

			if (node.m_parent && node.m_parent->GetType() == xLabelEx2)
			{
				TRACE("      ==>Object:  xLabelEx2=\"%S\" on net=%d from file name index=%d\n", static_cast<CDrawLabel*> (node.m_parent)->GetValue(), node.m_NetList, node.getFileNameIndex());
				//Labels of type=xLabelEx2 are preferred over labels of type=xPinEx or no type, but are not preferred over labels of type=xPower
				if (preferredNetNameLevel >= node.getFileNameIndex() && (preferredNetNameType != xPower))
				{
					//This xLabelEx2 is of equal or higher priority than the previous choice of xPinEx or a different xLabelEx2 (presumably a duplicate) because a user entered it.
					TRACE("        ==>Choosing xLabelEx2=\"%S\" from level=%d and type=%d over previous choice=\"%S\" from level=%d and type=%d\n", static_cast<CDrawLabel*> (node.m_parent)->GetValue(), node.getFileNameIndex(), xLabelEx2, preferredNetName, preferredNetNameLevel, preferredNetNameType);
					if (node.getFileNameIndex() != 0)
					{
						//Net labels from lower hierarchical levels are not guaranteed to be unique, so must be modified with the net number or hierarchical reference designator path (user's choice?).
						//						preferredNetName.Format(_T("_N_%d_%s"), node.m_NetList, static_cast<CDrawLabel*>(node.m_parent)->GetValue());

						//xLabelEx2 is not associated with a symbol, so it doesn't have a reference designator.  get_partial_reference_path() must be used rather
						//than get_reference_path() to avoid exceptions and a duplicate last reference designator.
						//If get_partial_reference_path() contains at least one hierarchical designator, then it will also include a trailing underscore to be used as a separator
						preferredNetName.Format(_T("%s%s"), (LPCTSTR)get_partial_reference_path((CDrawMethod*) node.m_parent, m_imports[node.getFileNameIndex()], true), (LPCTSTR) (static_cast<CDrawLabel*> (node.m_parent)->GetValue()));
					}
					else
					{
						preferredNetName = static_cast<CDrawLabel*> (node.m_parent)->GetValue(); //Hierarchical level 0 labels are guaranteed to be unique and are safe to use without modification.
					}
					preferredNetNameLevel = node.getFileNameIndex();
					preferredNetNameType = xLabelEx2;
				}
			}
			else if (node.m_parent && node.m_parent->GetType() == xPower)
			{ //xPower symbols are effectively global across hierarchical levels
				CString powerLabel = get_power_label((CDrawPower *) node.m_parent);
				TRACE("      ==>Object:  xPower=\"%S\" on m_net=%d, file name index=%d\n", powerLabel, node.m_NetList, node.getFileNameIndex());
				if (preferredNetNameLevel >= node.getFileNameIndex())
				{
					//This label is of equal or higher priority than all other choices
					TRACE("        ==>Choosing xPower=\"%S\" from level=%d and type=%d over previous choice=\"%S\" from level=%d and type=%d\n", powerLabel, node.getFileNameIndex(), xPower, preferredNetName, preferredNetNameLevel);
					preferredNetName = powerLabel;
					preferredNetNameLevel = node.getFileNameIndex();
					preferredNetNameType = xPower;
				}
			}
#if 0	//This net name possibility is under construction and is not yet safe to use!  It is still producing duplicate net names under some circumstances.
			else if (node.m_parent && (node.getFileNameIndex() > 0) && (node.m_parent->GetType() == xPinEx))
			{
				//Unnamed nets can most easily be given reasonable net names by constructing one from the controlling pin name.
				//The problem is in figuring out which one is controlling as different net types have different controlling pin types.
				//It is also a significant possiblity that the same constructed name might be chosen for more than one discrete net number.  
				//This will break any external net list tools and must be avoided at all costs.
				//
				//Must exclude pins from level=0 from this algorithm because they don't have a designator path other than the default "_" path.  
				//Using them will potentially result in multiple nets being given the same preferred name.
				TRACE("      ==>Object:  xPinEx=\"%S\" on net=%d from file name index=%d\n", node.getLabel(), node.m_NetList, node.getFileNameIndex());
				//This pin may already have a net label constructed on the fly from the node.getLabel() functionality, but it is not guaranteed to be unique in the flattened netlist,
				//so a new guaranteed unique name is constructed here for use in exporting netlists only.  The original constructed name is used to actually form the sheet local netlists
				//and is guaranteed to be unique in that context, but once merged into the global netlist, it may not be sufficient, especially if it exists only on lower hierarchical levels only.

				if ((preferredNetNameLevel >= node.getFileNameIndex()) && (preferredNetNameType != xPower) && (preferredNetNameType != xLabelEx2))
				{
					//This label is of equal or higher priority than the previous choice, but will not override even higher level hierarchical nets with names derived from labels or power symbols.
					if (!(static_cast<CDrawPin*>(node.m_parent)->GetPinName().IsEmpty()))
					{
						TRACE("        ==>Choosing xPinEx=\"%S\" from level=%d and type=%d (underlying label name = \"%s\") over previous choice=\"%S\" from level=%d and type=%d\n",
								static_cast<CDrawPin*>(node.m_parent)->GetPinName(),
								node.getFileNameIndex(),
								xPinEx,
								node.getLabel(),
								preferredNetName,
								preferredNetNameLevel,
								preferredNetNameType);
						//Construct a net name from this hierarchical pin's pin name consisting of the path of reference designators required to reach this pin (separated by underscores) and ending with the pin's name
						preferredNetName.Format(_T("GS_%s%s"), get_partial_reference_path((CDrawMethod*) node.m_parent, m_imports[node.getFileNameIndex()], true), static_cast<CDrawPin*>(node.m_parent)->GetPinName());
						//						preferredNetName.Format(_T("%s_%s"), node.m_reference, static_cast<CDrawPin*>(node.m_parent)->GetPinName());
						preferredNetNameLevel = node.getFileNameIndex();
						TRACE("           ==>Constructed name = \"%S\"\n",preferredNetName);
						preferredNetNameType = xPinEx;
					}
				}
			}
#endif
			++vi;
		}

		//If this net has no preferred name at all, then create one
		if (preferredNetName.IsEmpty())
		{
			preferredNetName.Format(_T("_N_%d"), (*vi_saved).m_NetList);
			preferredNetNameType = xNULL; //There isn't a type for constructed net names.
			preferredNetNameLevel = 0; //Since the complete net list is now known, let's assume that this was formed at level 0 even though it might have been embedded at a lower level.  This variable isn't actually used at this point except for debugging.
			TRACE("      ==>No preferred net name found for this node because it contained no net labels or power symbols or hierarchical pins with non-blank pin names.  Preferred net name will be constructed.\n");
		}

		//Now iterate through the nodes in this vector again, and this time set the preferred label for each node for later use
		vi = v.begin();
		while (vi != v.end())
		{
			(*vi++).setPreferredLabel(preferredNetName); //the saving of a separate preferred net name may not be necessary, but it seems necessary at the moment
			TRACE("      ==>Preferred net name = \"%S\" assigned to this node taken from level=%d and type=%d\n", preferredNetName, preferredNetNameLevel, preferredNetNameType);
		}

		++ni;
	}

	TRACE("  Linker Pass #3 complete\n\n\n\n");
	//Uncomment to debug the netlist
	TRACE("\n\n\nDump of netlist objects after pass 3 completes.  Total size of netlist=%d.\n", m_nets.size());
	CNetList::dumpNetListObjects();
	TRACE("Dump complete.\n\n\n");
}

/**
 * Perform the work of making a netlist from this design... 
 * Its important to understand that nets are a type of post-processing step -
 * They are not actually stored inside the TinyCAD design file.  The netlist is
 * created when it is needed and a pointer to the final linked top level netlist
 * is stored eventually in pDesign->m_nets.
 * 
 * @param pDesign
 */
void CNetList::MakeNet(CConCadMultiDoc *pDesign)
{
	linkCollection nets;
	Counter file_counter;

	//CConCadMultiDoc*	aDesign = pDesign;
	CollectionMemberReference<CImportFile *> impref;

	CImportFile *f = new CImportFile(pDesign);
	f->setFileNameIndex(file_counter.next());
	m_imports.push_back(f);

	// For each member of m_imports, we generate a netlist for all of its sheets.
	// This includes the root design. Recursion happens here because
	// new members may be added to m_imports as a result of the
	// MakeNetForSheet call. So we can't cache size() or any of its
	// members.
//	TRACE("\n\nEntering CNetList::MakeNet():  Making nets for every sheet in this design\n");
	for (unsigned int ip = 0; ip < m_imports.size(); ++ip)
	{
		/// Now create the nets for it
		int base = (int) nets.size();
		int newSheets = m_imports[ip]->getDesign()->GetNumberOfSheets();
		nets.resize(base + newSheets);
//		TRACE("Started with %d sheets, then added %d new sheets resulting in %d sheets total\n",
//			base,
//			newSheets,
//			nets.size());

		/// Generate a netlist for every sheet in this imported design
		for (int i = 0; i < newSheets; i++)
		{
			nets[i + base].m_prefix_references = m_prefix_references;
			nets[i + base].m_prefix_import = m_prefix_import;
			nets[i + base].MakeNetForSheet(m_imports, ip, i, file_counter);
		}

		// don't repeat this process if we're not following imports
		if (!m_follow_imports) break;
	}

	/// Now link the nets together
//	TRACE("\nLinking nets from all sheets together\n\n");
	Link(nets);

	ApplyHints();

	/// ... and write the results into the design for ease of use...
	WriteWires();
	//	TRACE("Leaving CNetList::MakeNet():  Finished making nets for every sheet in this design\n");
}

/**
 * Connections between nets (the nodes) are at:
 * .. the end of wires
 * .. the point of pins
 * .. the point of power
 * .. the point of labels
 * .. the point of no-connects
 *
 * To generate a netlist:
 * 1. Search design for nodes.
 * - Insert nodes into node tree and either assign net list to node or generate a new net list
 * - If wire joins two different netlists, then join the two netlists together
 * 2. Find Junctions
 * - Find which wires junctions lie on and connect their netlists
 * 3. Find Labels
 * - Find which wire each label lies on and connect their netlists
 * Perform the work of making a netlist from this design...
 */

/** 
 * Perform the work of making a netlist from a single sheet in this design...
 * 
 * @param imports
 * @param import_index -- the current item within the imports vector
 * @param sheetZeroIndex -- the zero-based index of the sheet within the design file
 * @param file_counter -- a counter used to set the file index of any new design files
 */
void CNetList::MakeNetForSheet(fileCollection &imports, int import_index, int sheetZeroIndexed, Counter& file_counter)
//( fileCollection &imports, int file_index_id, int &file_name_index, int sheet, CConCadDoc *pDesign )
//  m_imports[ip]->m_file_name_index, file_name_index, i+1, m_imports[ip]->m_pDesign->GetSheet( i ) );
{
	int sheetOneIndexed = sheetZeroIndexed + 1;
//  TRACE("\n\nEntering CNetList::MakeNetForSheet() for page %d\n",sheetOneIndexed);

	// lookup these values from the imports list
	int file_index_id = imports[import_index]->getFileNameIndex();
	CConCadDoc *pDesign = imports[import_index]->getDesign()->GetSheet(sheetZeroIndexed);
	// sheet is 1-indexed
	CollectionMemberReference<CImportFile *> referenceToMe(imports, import_index);

	/// Get rid of any old data
	m_CurrentNet = 1;
	m_nodes.erase(m_nodes.begin(), m_nodes.end());
	m_nets.erase(m_nets.begin(), m_nets.end());

	/// Here is some temporary data for this function
	typedef std::map<CString, int> stringCollection;
	stringCollection Powers;
	stringCollection Connected;

	//Prefetch iterator begin and end to speed things up
	const drawingIterator itBegin = pDesign->GetDrawingBegin();
	const drawingIterator itEnd = pDesign->GetDrawingEnd();

	/// Search for nodes, and build the node tree
	drawingIterator it = itBegin;
	for (; it != itEnd; ++it)
	{
		CDrawingObject *ObjPtr = *it;
		stringCollection::iterator found;
		int hold;
		CDPoint tr;

		switch (ObjPtr->GetType())
		{
			case xHierarchicalSymbol:
			{
				CDrawHierarchicalSymbol *pSymbol = static_cast<CDrawHierarchicalSymbol*> (ObjPtr);
//				TRACE("Found xHierarchicalSymbol with name=[%S], Ref=[%S] based on file \"%S\"\n\n",pSymbol->GetName(), pSymbol->GetRef(), pSymbol->GetFilename());
				/// Try and stop recursion by limiting the number of imports
				if (imports.size() > 100)
				{
					AfxMessageBox(IDS_RECURSION);
					continue;
				}

				/// Push back this filename into the list of extra imports

				CString partReferenceString = pSymbol->GetFieldByName("Ref");
				CImportFile *f = new CImportFile(referenceToMe, partReferenceString);
				f->setFileNameIndex(file_counter.next());
				if (f->Load(pSymbol->GetFilename()))
				{
					imports.push_back(f);

					/// Now search the symbol for pins to link the other symbols to
					drawingCollection method;
					pSymbol->ExtractSymbol(tr, method);

					drawingIterator it = method.begin();
					drawingIterator itEnd = method.end();
					while (it != itEnd)
					{
						CDrawingObject *pointer = *it;

						if (pointer->GetType() == xPinEx)
						{
							CDrawPin *thePin = static_cast<CDrawPin*> (pointer);

							/// This in effect labels the node with the new node name...
							CNetListNode n(f->getFileNameIndex(), sheetOneIndexed, thePin, thePin->GetActivePoint(pSymbol));
							n.setLabel(thePin->GetPinName());
							n.m_reference =
							// pSymbol->GetRefSheet(m_prefix_references,m_prefix_import,file_index_id,sheetOneIndexed);
							        get_reference_path(pSymbol, imports[file_index_id], CNetList::m_refDirectionForward);
							n.m_pin = thePin->GetNumber();
							n.m_pMethod = pSymbol;
							Add(n);
						}

						++it;
					}
				}
				else
				{
					delete f;
				}
			}
				break;
			case xMethodEx3:
#define thePin ((CDrawPin*)pointer)
#define theMethod ((CDrawMethod*)ObjPtr)
//			TRACE("Found xMethodEx3 (Symbol) at (%g,%g).  Symbol name=[%S], Symbol ref=[%S].\n",
//				theMethod->m_point_a.x, theMethod->m_point_a.y,
//				theMethod->GetName(), theMethod->GetRef());
			{
				drawingCollection method;
				((CDrawMethod *) ObjPtr)->ExtractSymbol(tr, method);

				CString myRefDes = get_reference_path(theMethod, imports[file_index_id], CNetList::m_refDirectionForward);

				drawingIterator it = method.begin();
				drawingIterator itEnd = method.end();
				while (it != itEnd)
				{
					CDrawingObject *pointer = *it;

					if (pointer->GetType() == xPinEx && ! (thePin->IsPower()))
					{
						//Only process pins that are NOT power pins here.  Power pins are handled later.
//						TRACE("  ==>Symbol name=[%S], ref=[%S]:  This pin is NOT a power pin.  PinName=[%S], Name=[%S], Pin number=[%S].\n",
//							theMethod->GetName(), theMethod->GetRef(),
//							thePin->GetPinName(), thePin->GetName(), thePin->GetNumber());

						CNetListNode n(file_index_id, sheetOneIndexed, thePin, thePin->GetActivePoint(theMethod));
						n.m_reference = myRefDes;

						n.m_pin = thePin->GetNumber();
						n.m_pMethod = theMethod;
						Add(n);
					}

					++it;
				}

				/// Has this symbol had it's power connected?
				if (Connected.find(myRefDes) == Connected.end())
				{
//					TRACE("  ==>Seeing if reference designator [%S] has any pins of type=power defined\n",myRefDes);
					Connected[myRefDes] = TRUE;

					drawingCollection method;
					((CDrawMethod *) ObjPtr)->ExtractSymbol(tr, method);
					drawingIterator it = method.begin();
					drawingIterator itEnd = method.end();
					while (it != itEnd)
					{
						CDrawingObject *pointer = *it;

						if (pointer->GetType() == xPinEx && thePin->IsPower())
						{
							// Hidden power pins will get an uninitialized CDPoint node point.
							// The Add method will never connect to any other uninitialized CDPoint node point
							// and thus hidden power pins will correctly never connect to anything by their node coordinate.
							CNetListNode n(file_index_id, sheetOneIndexed, thePin, CDPoint() );
							// Set netlist label name to invisible symbol power pin name
//							TRACE("  ==>Found a power pin in this symbol.  Setting netlist %d's label to power pin name=[\"%S\"]\n",
//								n.m_NetList,
//								thePin->GetPinName());
							n.setLabel(thePin->GetPinName());
							// n.m_reference = theMethod->GetRefSheet(m_prefix_references,m_prefix_import,file_index_id,sheetOneIndexed);
							n.m_reference = get_reference_path(theMethod, imports[file_index_id], CNetList::m_refDirectionForward);

							n.m_pin = thePin->GetNumber();
							n.m_pMethod = theMethod;

							/// Look up the netlist this power pin belongs to
							found = Powers.find(thePin->GetPinName());
							if (found != Powers.end()) n.m_NetList = (*found).second;
							hold = Add(n);
							if (found == Powers.end()) Powers[thePin->GetPinName()] = hold;
						}

						++it;
					}
//					TRACE("  ==>Finished checking this symbol for power pins\n");
				}
			}
				break;
			case xNoConnect:
				{
					CNetListNode newNode(file_index_id, sheetOneIndexed, ObjPtr, ObjPtr->m_point_a);
					Add(newNode);
//					TRACE("Found xNoConnect at (%g,%g).\n",ObjPtr->m_point_a.x, ObjPtr->m_point_a.y);
				}
				break;
			case xJunction:
				{
					CNetListNode newNode(file_index_id, sheetOneIndexed, ObjPtr, ObjPtr->m_point_a);
					Add(newNode);
//					TRACE("Found xJunction at (%g,%g).\n",ObjPtr->m_point_a.x, ObjPtr->m_point_a.y);
				}
				break;
			case xPower:
			{
				CNetListNode n(file_index_id, sheetOneIndexed, ObjPtr, ObjPtr->m_point_a);

				CString powerLabel = get_power_label((CDrawPower *) ObjPtr);
//			TRACE("Found xPower=[%S] at (%g,%g)\n",powerLabel,ObjPtr->m_point_a.x, ObjPtr->m_point_a.y);
				n.setLabel(powerLabel);

				/// Does this power item exist?
				found = Powers.find(powerLabel);
				if (found != Powers.end()) n.m_NetList = (*found).second;
				hold = Add(n);
				if (found == Powers.end()) Powers[powerLabel] = hold;
			}
				break;
			case xCable:
			case xWire:
//			TRACE("Found xWire at (%g,%g) (%g,%g)\n",
//				ObjPtr->m_point_a.x, ObjPtr->m_point_a.y,
//				ObjPtr->m_point_b.x, ObjPtr->m_point_b.y);

			{
				CNetListNode n(file_index_id, sheetOneIndexed, ObjPtr, ObjPtr->m_point_a);
				hold = Add(n);
			}
				{
					CNetListNode n(file_index_id, sheetOneIndexed, ObjPtr, ObjPtr->m_point_b);
					n.m_NetList = hold;
					Add(n);
				}
				break;
		}
	}

	/// Search for junctions and connect together
//  TRACE("Searching for junctions to connect together\n");
	it = itBegin;
	while (it != itEnd)
	{
		CDrawingObject *ObjPtr = *it;

		/// Search for junctions
		if (ObjPtr->GetType() == xJunction)
		{
			/// Find out which netlist was assigned to this point
			CDPoint a = ObjPtr->m_point_a;
			int NetNumber = m_nodes[a];
//		TRACE("  ==>Found a junction at (%g,%g).  Will now look for wires that cross this junction\n",a.x, a.y);

			/// Look for wires which cross this junction
			drawingIterator search_it = itBegin;
			while (search_it != itEnd)
			{
				CDrawingObject *search = *search_it;

				/// Find the wires
				/// If the wire has an end at this junction then it is already connected
				if ( (search->GetType() == xWire || search->GetType() == xCable) && (search->m_point_a != a) && (search->m_point_b != a))
				{
					/// Is this point on this wire?
					CLineUtils l(search->m_point_a, search->m_point_b);
					double distance_along_a;

					if (l.IsPointOnLine(a, distance_along_a))
					{
						CNetListNode n(file_index_id, sheetOneIndexed, NULL, search->m_point_a);
						n.m_NetList = NetNumber;
						NetNumber = Add(n);
//					TRACE("  ==>Found line segment with coordinates (%g,%g : %g,%g) that intersects junction at (%g,%g).  Adding this node to net number=[%d]\n",
//						search->m_point_a.x, search->m_point_a.y,
//						search->m_point_b.x, search->m_point_b.y,
//						a.x, a.y,
//						NetNumber);
					}
				}

				++search_it;
			}
		}

		++it;
	}

	/// Search for labels and connect to their respective lines
	stringCollection labels;
//  TRACE("Searching for labels to match with wires\n");
	it = itBegin;
	while (it != itEnd)
	{
		CDrawingObject *ObjPtr = *it;

		/// Search for labels
		if (ObjPtr->GetType() == xLabelEx2)
		{
			CDPoint a = static_cast<CDrawLabel*> (ObjPtr)->GetLabelPoint();
//		TRACE("Found label.  Value=[%S], Name=[%S], ObjType=[%d/0x%x] (enum value).  Looking for wires that connect to it\n",
//			static_cast<CDrawLabel*>(ObjPtr)->GetValue(),
//			static_cast<CDrawLabel*>(ObjPtr)->GetName(),
//			(int) (static_cast<CDrawLabel*>(ObjPtr)->GetType()),
//			(int) (static_cast<CDrawLabel*>(ObjPtr)->GetType()));

			/// Search for a wire this label is connect to
			/// Only attempt to connect to a single wire
			drawingIterator search_it = itBegin;
			while (search_it != itEnd)
			{
				CDrawingObject *search = *search_it;
				if ((search->GetType() == xWire || search->GetType() == xCable) && search->IsInside(a.x, a.x, a.y, a.y))
				{
					a = search->m_point_a; //Overwrite label point "a" with first point on newly found wire
//				TRACE("  ==>Found a wire with coordinates (%g,%g : %g,%g) that connects to label=[%S].  Stopping search after first find!\n",
//					search->m_point_a.x, search->m_point_a.y,
//					search->m_point_b.x, search->m_point_b.y,
//					static_cast<CDrawLabel*>(ObjPtr)->GetValue());
					break;
				}

				++search_it;
			}

			/// Look up this label
			CNetListNode n(file_index_id, sheetOneIndexed, ObjPtr, a); //Find the net list node that this wire is a member of
//			TRACE("Assigning label [%S] to netlist name=[%S], number=%d\n",((CDrawLabel *)ObjPtr)->GetValue(),n.getLabel(),n.m_NetList);

			n.setLabel( ((CDrawLabel *) ObjPtr)->GetValue());

			/// Has this label already been assigned a netlist?
			stringCollection::iterator found = labels.find( ((CDrawLabel *) ObjPtr)->GetValue());
			if (found != labels.end())
			{
				//			TRACE("Label [%S] has already been assigned to netlist %d via net name=[%S], net number=[%d].\n",
				//				((CDrawLabel *)ObjPtr)->GetValue(),
				//				(*found).second,
				//				n.getLabel(),
				//				n.m_NetList);
				n.m_NetList = (*found).second;
				n.setLabel(_T(""));
			}

			int hold = Add(n);

			/// If there was no netlist write it back...
			if (found == labels.end())
			{

				assert(labels[ ((CDrawLabel *)ObjPtr)->GetValue()] == 0);

				labels[ ((CDrawLabel *) ObjPtr)->GetValue()] = hold;
//				TRACE("Label=[%S] has not yet had a netlist assigned to it.  Holding netlist=%d in labels[%S] until later.  After assignment, netlist=%d\n",
//					((CDrawLabel *)ObjPtr)->GetValue(),
//					hold,
//					((CDrawLabel *)ObjPtr)->GetValue(),
//					labels[ ((CDrawLabel *)ObjPtr)->GetValue()]);
			}
		}

		++it;
	}

	/// Our work with the nodes map is complete, so we can discard it now...
	m_nodes.erase(m_nodes.begin(), m_nodes.end());

//  TRACE("Leaving CNetList::MakeNetForSheet() for page %d\n\n",sheetOneIndexed);
}

/**
 * Create netlist and output as the specified type of file.
 * 
 * @param type
 * @param pDesign
 * @param filename
 */
void CNetList::WriteNetListFile(int type, CConCadMultiDoc *pDesign, const TCHAR *filename)
{
	// The formats of Special > Create Netlist (the PCB formats were removed)
	switch (type)
	{
		case 1:
			WriteNetListFileXML(pDesign, filename);
			break;
		default:
			WriteNetListFileTinyCAD(pDesign, filename);
			break;
	}
}

/**
 * Create netlist and output as a PCB file
 * 
 * @param pDesign
 * @param filename
 */
void CNetList::WriteNetListFileTinyCAD(CConCadMultiDoc *pDesign, const TCHAR *filename)
{
	FILE *theFile;
	errno_t err;
	err = _tfopen_s(&theFile, filename, _T("w"));
	if ( (theFile == NULL) || (err != 0))
	{
		Message(IDS_CANNOTOPEN);
		return;
	}

	// Set the Busy icon
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));

	// Get the net list
	MakeNet(pDesign);

	CString temp;
	temp = pDesign->GetPathName();
	_ftprintf(theFile, NetComment _T(" ====+  Net List for %s  +====\n\n"), (LPCTSTR)temp);

	_ftprintf(theFile, NetComment _T(" ======+ The component list\n\n"));

	// Keep track of the references that we have output...
	std::set<CString> referenced;

	/// Do this for all of the files in the imports list...
	fileCollection::iterator fi = m_imports.begin();
	for (; fi != m_imports.end(); ++fi)
	{
		CConCadMultiDoc *dsn = pDesign;

		if ( (*fi)->getFileNameIndex() != 0)
		{
			dsn = static_cast<CConCadMultiDoc *> ( (*fi)->getDesign());
		}

		/// Generate a component for every sheet in this design
		for (int i = 0; i < dsn->GetNumberOfSheets(); i++)
		{
			drawingIterator it = dsn->GetSheet(i)->GetDrawingBegin();
			while (it != dsn->GetSheet(i)->GetDrawingEnd())
			{
				CDrawingObject *pointer = *it;

				if (pointer->GetType() == xMethodEx3)
				{
					CDrawMethod *pMethod = static_cast<CDrawMethod *> (pointer);
					CString Name = pMethod->GetField(CDrawMethod::Name);
					CString Ref = get_reference_path(pMethod, (*fi), CNetList::m_refDirectionForward);
					//pMethod->GetRefSheet(m_prefix_references,m_prefix_import,(*fi)->getFileNameIndex(),i+1);

					/// Do we need to output this part?
					if (referenced.find(Ref) == referenced.end())
					{
						referenced.insert(Ref);

						_ftprintf(theFile, _T("COMPONENT '%s' = %s\n"), (LPCTSTR)Ref, (LPCTSTR)Name);

						/// Now write it's "other" references
						for (int i = 2; i < pMethod->GetFieldCount(); i++)
						{
							_ftprintf(theFile, _T("\tOPTION '%s' = %s\n"), (LPCTSTR)(pMethod->GetFieldName(i)), (LPCTSTR)(pMethod->GetField(i)));
						}
					}
				}

				++it;
			}
		}
	}

	_ftprintf(theFile, _T("\n\n") NetComment _T(" ======+ The net list\n\n"));

	netCollection::iterator nit = m_nets.begin();

	while (nit != m_nets.end())
	{
		nodeVector::iterator nv_it = (*nit).second.begin();

		CString theLine, theLabel;
		int Label = nv_it->m_NetList;

		if (nv_it != (*nit).second.end())
		{
			theLine = "";
			BOOL first = TRUE, PrintLine = FALSE, Labeled = FALSE;

			while (nv_it != (*nit).second.end())
			{
				CNetListNode& theNode = *nv_it;
				++nv_it;

				if (!theNode.getLabel().IsEmpty() && !Labeled)
				{
					theLabel = theNode.getLabel();
					Labeled = TRUE;
				}

				if (!theNode.m_reference.IsEmpty())
				{
					CString add;
					add.Format(_T("(%s,%s)"), (LPCTSTR)theNode.m_reference, (LPCTSTR)theNode.m_pin);
					if (theNode.getLabel() != add)
					{
						if (first)
						{
							first = FALSE;
						}
						else
						{
							theLine += _T(",");
						}

						theLine += add;
						PrintLine = TRUE;
					}
				}
			}
			if (PrintLine)
			{
				_ftprintf(theFile, _T("NET  "));
				if (Labeled)
				{
					_ftprintf(theFile, _T("'%s'"), (LPCTSTR)theLabel);
				}
				else
				{
					_ftprintf(theFile, _T("'N%06d'"), Label);
				}
				_ftprintf(theFile, _T(" =  %s\n"), (LPCTSTR)theLine);
			}
		}

		++nit;
	}

	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
	fclose(theFile);
}

void to_utf8(const std::basic_string<TCHAR>& str, std::ofstream& outfile)
{
	// Setup the default encoding
#ifdef UNICODE
	iconv_t m_charset_conv = iconv_open( "UTF-8", "UCS-2-INTERNAL" );
#else
	iconv_t m_charset_conv = iconv_open("UTF-8", "char");
#endif

	// Allocate output buffer
	std::string outbuffer;
	outbuffer.resize(str.size());

	const TCHAR* pstr = str.c_str();
	size_t inbuf_size = str.size() * sizeof(TCHAR);

	// Do while there is input to convert
	while (inbuf_size)
	{
		size_t outbuf_size = outbuffer.size();
		char* m_conversion_buffer = const_cast<char*> (outbuffer.c_str());
		char* out = m_conversion_buffer;

		// Convert to UTF-8
		// increment: pstr, out
		// decrement: inbuf_size, outbuf_size
		iconv(m_charset_conv, (const char**) &pstr, &inbuf_size, &out, &outbuf_size);

		// Truncate buffer
		int converted_bytes_to_write = (int) (out - m_conversion_buffer);
		outbuffer.resize(converted_bytes_to_write);

		// output converted buffer
		outfile << outbuffer;
	}

	iconv_close(m_charset_conv);
}

/**
 * Create netlist and output as an XML file
 * 
 * @param pDesign
 * @param filename
 */
namespace RXML {
	typedef rapidxml::xml_node<TCHAR> node;
	typedef rapidxml::xml_attribute<TCHAR> attribute;
	typedef rapidxml::xml_document<TCHAR> document;
}
void CNetList::WriteNetListFileXML(CConCadMultiDoc *pDesign, const TCHAR *filename)
{
	std::ofstream outfile;
	outfile.open(filename);
	/// !!! jms -- TODO -- needs to handle file open error

	/// Set the Busy icon
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));

	rawWriteNetListFileXML(pDesign, outfile);

	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
	outfile.close();
}

void CNetList::rawWriteNetListFileXML(CConCadMultiDoc *pDesign, std::ofstream& outfile)
{
	RXML::document doc;

	// xml declaration
	RXML::node *decl = doc.allocate_node(rapidxml::node_declaration);
	decl->append_attribute(doc.allocate_attribute(_T("version"), _T("1.0")));
	decl->append_attribute(doc.allocate_attribute(_T("encoding"), _T("utf-8")));
	doc.append_node(decl);

	RXML::node *root = doc.allocate_node(rapidxml::node_element, _T("netlist"));
	doc.append_node(root);
	RXML::node *x_components = doc.allocate_node(rapidxml::node_element, _T("parts"));
	RXML::node *x_nets = doc.allocate_node(rapidxml::node_element, _T("nets"));
	root->append_node(x_components);
	root->append_node(x_nets);

	// Get the net list
	MakeNet(pDesign);

	RXML::attribute *attr = doc.allocate_attribute(_T("design"), doc.allocate_string(pDesign->GetPathName()));
	root->append_attribute(attr);

	// Keep track of the references that we have output...
	std::set<CString> referenced;

	/// Do this for all of the files in the imports list...
	fileCollection::iterator fi = m_imports.begin();
	for (; fi != m_imports.end(); ++fi)
	{
		CConCadMultiDoc *dsn = pDesign;

		if ( (*fi)->getFileNameIndex() != 0)
		{
			dsn = static_cast<CConCadMultiDoc *> ( (*fi)->getDesign());
		}

		/// Generate a component for every sheet in this design
		for (int i = 0; i < dsn->GetNumberOfSheets(); i++)
		{
			drawingIterator it = dsn->GetSheet(i)->GetDrawingBegin();
			while (it != dsn->GetSheet(i)->GetDrawingEnd())
			{
				CDrawingObject *pointer = *it;

				if (pointer->GetType() == xMethodEx3 || pointer->GetType() == xHierarchicalSymbol)
				{
					CDrawMethod *pMethod = static_cast<CDrawMethod *> (pointer);
					CString Name = pMethod->GetField(CDrawMethod::Name);
					CString Ref = get_reference_path(pMethod, (*fi), CNetList::m_refDirectionForward);
					//pMethod->GetRefSheet(m_prefix_references,m_prefix_import,(*fi)->getFileNameIndex(),i+1);

					/*
					 TinyCAD does not have a reference back to the library for which this symbol came from?!?!?
					 CDesignFileSymbol *pSymbol = pMethod->GetSymbolData();
					 */

					// Do we need to output this part?
					/*				if (referenced.find( Ref ) == referenced.end())
					 {
					 referenced.insert( Ref );
					 */
					RXML::node *x_component;

					if (pointer->GetType() == xHierarchicalSymbol)
					{
						CDrawHierarchicalSymbol *pSymbol = static_cast<CDrawHierarchicalSymbol*> (pointer);

						CString Name = pSymbol->GetFilename();
						x_component = doc.allocate_node(rapidxml::node_element, _T("hierarchical-part"));
						x_component->append_attribute(doc.allocate_attribute(_T("filename"), doc.allocate_string(Name)));
					}
					else
					{
						x_component = doc.allocate_node(rapidxml::node_element, _T("part"));
					}
					x_components->append_node(x_component);

					x_component->append_attribute(doc.allocate_attribute(_T("ref"), doc.allocate_string(Ref)));
					x_component->append_attribute(doc.allocate_attribute(_T("name"), doc.allocate_string(Name)));

					/// Now write attributes other than Ref and Name (which are always 1st and 2nd)
					int n = pMethod->GetFieldCount();
					for (int i = 2; i < n; i++)
					{
						RXML::node *x_component_attribute = doc.allocate_node(rapidxml::node_element, _T("attribute"), doc.allocate_string(pMethod->GetField(i)));
						x_component_attribute->append_attribute(doc.allocate_attribute(_T("name"), doc.allocate_string(pMethod->GetFieldName(i))));
						x_component->append_node(x_component_attribute);
					}
					// }
				}

				++it;
			}
		}
	}

	netCollection::iterator nit = m_nets.begin();

	while (nit != m_nets.end())
	{
		nodeVector::iterator nv_it = (*nit).second.begin();
		int Label = nv_it->m_NetList;

		RXML::node *x_net = doc.allocate_node(rapidxml::node_element, _T("net"));
		x_nets->append_node(x_net);

		TCHAR *netname = NULL;

		while (nv_it != (*nit).second.end())
		{
			CNetListNode& theNode = *nv_it;
			++nv_it;

			if (netname == NULL && !theNode.getPreferredLabel().IsEmpty())
			{
				netname = doc.allocate_string(theNode.getPreferredLabel());
			}

			if (theNode.m_parent != NULL && theNode.m_parent->GetType() == xPinEx)
			{
				CDrawPin *pPin = static_cast<CDrawPin *> (theNode.m_parent);

				bool isHierarchicalPin = (theNode.m_pMethod != NULL && theNode.m_pMethod->GetType() == xHierarchicalSymbol);
				RXML::node *x_pin = doc.allocate_node(rapidxml::node_element, isHierarchicalPin ? _T("hierarchical-pin") : _T("pin"));
				x_net->append_node(x_pin);
				if (!theNode.m_reference.IsEmpty())
				{
					x_pin->append_attribute(doc.allocate_attribute(_T("part"), doc.allocate_string(theNode.m_reference)));
				}
				x_pin->append_attribute(doc.allocate_attribute(_T("name"), doc.allocate_string(pPin->GetPinName())));
				x_pin->append_attribute(doc.allocate_attribute(_T("number"), doc.allocate_string(pPin->GetNumber())));
				const TCHAR *etype = CDrawPin::GetElectricalTypeName(pPin->GetElec());
				if (etype != NULL)
				{
					x_pin->append_attribute(doc.allocate_attribute(_T("type"), etype));
					// don't need to allocate_string since these types are fixed in memory
				}
			}
		}

		if (netname != NULL)
		{
			x_net->append_attribute(doc.allocate_attribute(_T("name"), netname));
		}

		TCHAR buf[12];
		_stprintf_s(buf, 12, _T("%d"), Label);
		x_net->append_attribute(doc.allocate_attribute(_T("number"), doc.allocate_string(buf)));

		++nit;
	}

	// output the XML document.
	std::basic_string<TCHAR> s;
	print(std::back_inserter(s), doc);

	// Convert to UTF-8.
	to_utf8(s, outfile);

}


// Get a hierarchical reference path from a symbol.
// This version appends or prepends the final reference desigator of the current symbol
// Care must be taken to ensure that psymbol is of a type of CDrawMethod* that contains a valid reference designator
//
// Note that file_name_index in all of these functions is misleading.
// It should really be called "context_instance_index" or something.
// It is a unique incremental identifier representing the context of a circuit or
// subcircuit. 0 represents the root (main) circuit. Each instance of a hierarchical
// design gets a new ID. (so if the root circuit has 2 instances of a hierarchical
// design, and that design has 2 instances of another hierarchical design,
// there will be 7 IDs total: 1 for the main circuit, 2 for the 2nd level design,
// and 4 for the 4 instances of the 3rd level design.
//
// As to the format of this function's output:
// Suppose the symbol in question is U4 inside a hierarchical circuit H1
// which in turn is inside another hierarchical circuit H22 in the root circuit.
// If forward=true and separator="_", this will return "H22_H1_U1".
// If forward=false and separator=":", this will return "U1:H1:H22".
// The default separator is an underscore.

CString CNetList::get_reference_path(const CDrawMethod* psymbol, const CImportFile* pcontext, bool forward, TCHAR separator)
{
	CString s;
	CString ref = pcontext->getReferenceContext();

	if (ref.GetLength() > 0)
	{

		s = get_partial_reference_path(psymbol, pcontext, forward, separator);

		//append or prepend, as appropriate, the current object's reference designtor
		if (forward)
		{
			s += psymbol->GetRef(); //For hierarchical symbols, this may result in duplication of the reference designator because the current ref is the same as the current context
		}
		else
		{
			s = psymbol->GetRef() + s; //For hierarchical symbols, this may result in duplication of the reference designator because the current ref is the same as the current context
		}
	}
	else
	{
		s = psymbol->GetRef();
	}
	TRACE("  ==>CNetList::get_reference_path() generated \"%S\"\n", s);
	return s;
}

// Get a hierarchical partial reference path from a symbol.
// This version does not append or prepend the designator of the current symbol
//
// Note that file_name_index in all of these functions is misleading.
// It should really be called "context_instance_index" or something.
// It is a unique incremental identifier representing the context of a circuit or
// subcircuit. 0 represents the root (main) circuit. Each instance of a hierarchical
// design gets a new ID. (so if the root circuit has 2 instances of a hierarchical
// design, and that design has 2 instances of another hierarchical design,
// there will be 7 IDs total: 1 for the main circuit, 2 for the 2nd level design,
// and 4 for the 4 instances of the 3rd level design.
//
// As to the format of this function's output:
// Suppose the symbol in question is U4 inside a hierarchical circuit H1
// which in turn is inside another hierarchical circuit H22 in the root circuit.
// If forward=true and separator="_", this will return "H22_H1_U1".
// If forward=false and separator=":", this will return "U1:H1:H22".
// The default separator is an underscore.
CString CNetList::get_partial_reference_path(const CDrawMethod* psymbol, const CImportFile* pcontext, bool forward, TCHAR separator)
{
	CString ref = pcontext->getReferenceContext();
	// The canonical reference for the constant index is forward,
	// with a separator of "/" and an additional "/" at the beginning.
	TRACE("CNetList::get_partial_reference_path() uses reference context = \"%S\".  Length=%d\n", ref, ref.GetLength());

	_TCHAR * canonical_separator = _T("/");
	CString s;
	if (ref.GetLength() > 0)
	{
		int curpos = 0;
		CString tok = ref.Tokenize(canonical_separator, curpos);
		while (tok != "")
		{
			if (forward)
			{
				s += tok;
				s += separator;
			}
			else
			{
				s = separator + tok + s;
			}
			tok = ref.Tokenize(canonical_separator, curpos);
		}
	}
	else
	{
		s = "";
	}
	TRACE("  ==>CNetList::get_partial_reference_path() generated \"%S\"\n", s);
	return s;
}

// Get a netlist label name for this power symbol
CString CNetList::get_power_label(CDrawPower *power)
{
	// Use the power value as netlist name where it is available
	if (!power->GetValue().IsEmpty())
	{
		return power->GetValue();
	}

	// Differentiate between the different power symbol shapes
	// and use fixed/default netlist names.
	switch (power->which)
	{
		case 0: // Bar
			return _T("TCPOWERBAR");
		case 1: // Circle
			return _T("TCPOWERCIRCLE");
		case 2: // Wave
			return _T("TCPOWERWAVE");
		case 3: // Arrow
			return _T("TCPOWERARROW");
		case 4: // Earth
			return _T("TCPOWEREARTH");
	}

	// Just in case... (or rather not a case ;-)
	return _T("TCPOWER");
}

CImportFile::~CImportFile()
{
	if (m_parent.isNotNull())
	{
		delete m_pDesign;
	}

	// we don't delete the design if there's no parent
	// (in which case this is a root design)
}

BOOL CImportFile::Load(const TCHAR *filename)
{
	ASSERT( m_pDesign == NULL );

	CFile f;
	CFileException e;
	if (!f.Open(filename, CFile::modeRead, &e))
	{
		e.ReportError();
		return FALSE;
	}
	CArchive ar(&f, CArchive::load);
	m_pDesign = new CConCadMultiDoc;
	m_pDesign->Serialize(ar);

	return TRUE;
}
