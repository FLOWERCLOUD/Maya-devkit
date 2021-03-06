#ifndef _MFnContainerNode
#define _MFnContainerNode
//-
// ==========================================================================
// Copyright (C) 1995 - 2006 Autodesk, Inc., and/or its licensors.  All
// rights reserved.
//
// The coded instructions, statements, computer programs, and/or related
// material (collectively the "Data") in these files contain unpublished
// information proprietary to Autodesk, Inc. ("Autodesk") and/or its
// licensors,  which is protected by U.S. and Canadian federal copyright law
// and by international treaties.
//
// The Data may not be disclosed or distributed to third parties or be
// copied or duplicated, in whole or in part, without the prior written
// consent of Autodesk.
//
// The copyright notices in the Software and this entire statement,
// including the above license grant, this restriction and the following
// disclaimer, must be included in all copies of the Software, in whole
// or in part, and all derivative works of the Software, unless such copies
// or derivative works are solely in the form of machine-executable object
// code generated by a source language processor.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
// AUTODESK DOES NOT MAKE AND HEREBY DISCLAIMS ANY EXPRESS OR IMPLIED
// WARRANTIES INCLUDING, BUT NOT LIMITED TO, THE WARRANTIES OF
// NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE,
// OR ARISING FROM A COURSE OF DEALING, USAGE, OR TRADE PRACTICE. IN NO
// EVENT WILL AUTODESK AND/OR ITS LICENSORS BE LIABLE FOR ANY LOST
// REVENUES, DATA, OR PROFITS, OR SPECIAL, DIRECT, INDIRECT, OR
// CONSEQUENTIAL DAMAGES, EVEN IF AUTODESK AND/OR ITS LICENSORS HAS
// BEEN ADVISED OF THE POSSIBILITY OR PROBABILITY OF SUCH DAMAGES.
// ==========================================================================
//+
//
// CLASS:    MFnContainerNode
//
// ****************************************************************************

#if defined __cplusplus

// ****************************************************************************
// INCLUDED HEADER FILES


#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>

// ****************************************************************************
// DECLARATIONS

class MObject;
class MPlugArray;
class MStringArray;

// ****************************************************************************
// CLASS DECLARATION (MFnContainerNode)

//! \ingroup OpenMaya MFn
//! \brief container function set 
/*!
  MFnContainerNode is the function set for creating, querying and
  editing containers.

  Maya uses container nodes to bundle sets of related nodes
  together with a published attribute list that defines the primary
  interface to those nodes. This class allows you to query information
  about container nodes in the Maya scene.
*/
class OPENMAYA_EXPORT MFnContainerNode : public MFnDependencyNode
{
	declareMFn(MFnContainerNode, MFnDependencyNode);

public:
    MStatus		getPublishedPlugs(MPlugArray& publishedPlugs,
								  MStringArray& publishedNames) const;
    MStatus		getPublishedNames(MStringArray& publishedNames,
								  bool unboundOnly) const;
    MStatus		getMembers(MObjectArray& members) const;
    MStatus		getSubcontainers(MObjectArray& members) const;
    MStatus		getParentContainer(MObject& parent) const;
    MStatus		getRootTransform(MObject& root) const;

	//! Specify which type of published node
    enum MPublishNodeType {
		kParentAnchor,	//!< published as parent anchor
		kChildAnchor,	//!< published as child anchor
		kGeneric		//!< published as node (non-anchor)
	};
	
    MStatus		getPublishedNodes(MPublishNodeType type,
								  MStringArray& publishedNames,
								  MObjectArray& nodes) const;

	MStatus 	clear();

BEGIN_NO_SCRIPT_SUPPORT:

 	declareMFnConstConstructor( MFnContainerNode, MFnDependencyNode );

END_NO_SCRIPT_SUPPORT:

protected:
// No protected members

private:

};

#endif /* __cplusplus */
#endif /* _MFnContainerNode */
