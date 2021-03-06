//-
// ==========================================================================
// Copyright 2015 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk
// license agreement provided at the time of installation or download,
// or which otherwise accompanies this software in either electronic
// or hard copy form.
// ==========================================================================
//+
#if defined(D3D9_SUPPORTED)

#pragma warning (disable:4239)
#include <stdio.h>

#include "D3DResourceManager.h"

#include <maya/MGlobal.h>
#include <maya/MString.h>
#include <maya/MFnCamera.h>
#include <maya/MAngle.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MItDag.h>
#include <maya/MMatrix.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MBoundingBox.h>
#include <maya/MNodeMessage.h> // For monitor geometry list
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MFnLight.h>
#include <maya/MFnSpotLight.h>
#include <maya/MColor.h>
#include <maya/MFloatMatrix.h>

//////////////////////////////////////////////////////////////////////////////////////
//
// Monitors on Maya scene graph
//

// Handle node dirty changes
void geometryDirtyCallback( void* clientData )
{
	GeometryItem *item = (GeometryItem *)clientData;
	if (item)
	{
		MMessage::removeCallback( item->m_objectDirtyMonitor );	
		item->m_objectDirtyMonitor = 0;
		item->m_objectPath = MDagPath(); // Assign in valid dag path to mark as "bad"
	}
}

// Handle node attr change 
void geomteryChangedCallback( MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug, void* clientData)
{
	GeometryItem *item = (GeometryItem *)clientData;
	if (item)
	{
		MMessage::removeCallback( item->m_objectChangeMonitor );	
		item->m_objectChangeMonitor = 0;
		item->m_objectPath = MDagPath(); // Assign in valid dag path to mark as "bad"
	}
}
void textureChangedCallback( MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug, void* clientData)
{
	TextureItem *item = (TextureItem *)clientData;
	if (item)
	{
		MMessage::removeCallback( item->m_objectChangeMonitor );	
		item->m_objectChangeMonitor = 0;
		item->m_mayaNode = MObject::kNullObj; // Assign in valid dag path to mark as "bad"
	}
}

// Handle node delete
void geometryDeleteCallback( MObject &node,
						MDGModifier& modifier,
						void* clientData )
{
	GeometryItem *item = (GeometryItem *)clientData;
	if (item)
	{
		MMessage::removeCallback( item->m_objectDeleteMonitor );	
		item->m_objectDeleteMonitor = 0;
		item->m_objectPath = MDagPath(); // Assign in valid dag path to mark as "bad"
	}
}

void textureDeleteCallback( MObject &node,
						MDGModifier& modifier,
						void* clientData )
{
	TextureItem *item = (TextureItem *)clientData;
	if (item)
	{
		MMessage::removeCallback( item->m_objectDeleteMonitor );	
		item->m_objectDeleteMonitor = 0;
		item->m_mayaNode = MObject::kNullObj; // Assign in valid dag path to mark as "bad"
	}
}

///////////////////////////////

D3DResourceManager::D3DResourceManager()
{
	// All lights are off
	m_numberLightsEnabled = 0;

	initializeDefaultCamera();
}

/* virtual */
D3DResourceManager::~D3DResourceManager()
{
	const bool onlyInvalidItems = false;
	clearResources(onlyInvalidItems, true);
}

void					
D3DResourceManager::initializeDefaultCamera()
{
	// Set the default camera being 10 up and 10 back, with Y-up (to match Maya).
    m_camera.m_vEyePt = D3DXVECTOR3( 0.0f, 10.0f, -10.0f );
    m_camera.m_vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_camera.m_vUpVec = D3DXVECTOR3 ( 0.0f, 1.0f, 0.0f );

	// Set up default clip planes, and FOV to match Maya's
	m_camera.m_FieldOfView = 45.0f;
	m_camera.m_nearClip = 0.1f;
	m_camera.m_farClip = 1000.0f;
}

bool					
D3DResourceManager::translateCamera( const MDagPath &cameraPath )
//
// Description:
//		Translate Maya's camera 
//
{
	bool translatedCamera = false;
	if (cameraPath.isValid())
	{
		MStatus status;
		MFnCamera camera (cameraPath, &status);
		if ( !status ) {
			status.perror("MFnCamera constructor");
		}
		else
		{
			translatedCamera = true;

			MPoint eyePoint = camera.eyePoint( MSpace::kWorld );
			MPoint lookAtPt	= camera.centerOfInterestPoint( MSpace::kWorld );
			MVector	upDirection = camera.upDirection ( MSpace::kWorld );
			MFloatMatrix projMatrix = camera.projectionMatrix();

			double horizontalFieldOfView = MAngle( /* camera.verticalFieldOfView() / */ camera.horizontalFieldOfView()
				).asDegrees();
			double nearClippingPlane = camera.nearClippingPlane();
			double farClippingPlane = camera.farClippingPlane();

			// Convert API values to internal native storage.
			//
			m_camera.m_vEyePt = D3DXVECTOR3((float)eyePoint.x, (float)eyePoint.y, (float)eyePoint.z);
			m_camera.m_vLookatPt = D3DXVECTOR3((float)lookAtPt.x, (float)lookAtPt.y, (float)lookAtPt.z);
			m_camera.m_vUpVec = D3DXVECTOR3((float)upDirection.x, (float)upDirection.y, (float)upDirection.z);
			m_camera.m_FieldOfView = (float)horizontalFieldOfView;
			m_camera.m_nearClip = (float)nearClippingPlane;
			m_camera.m_farClip = (float)farClippingPlane;
			m_camera.m_isOrtho = camera.isOrtho();
		}
	}
	else
	{
		initializeDefaultCamera();
	}
	return translatedCamera;
}


void
D3DResourceManager::enableLights( bool val, LPDIRECT3DDEVICE9 D3D )
{
	unsigned i;
	for (i=0; i<m_numberLightsEnabled; i++)
	{
		D3D->LightEnable( i, val);
	}
}

bool D3DResourceManager::cleanupLighting(LPDIRECT3DDEVICE9 D3D)
{
	unsigned int i=0;
	for (i=0; i<m_numberLightsEnabled; i++)
	{
		D3D->LightEnable( i, false);
	}

	LightItemList::const_iterator it, end_it;
	end_it = m_lightItemList.end();
	for (it = m_lightItemList.begin(); it != end_it;  it++)
	{
		LightItem *item = *it;
		if (item)
		{
			delete item;
		}
	}
	m_lightItemList.clear();

	return true;
}

bool D3DResourceManager::setupLighting(LPDIRECT3DDEVICE9 D3D)
//
// Description:
//		Set up lighting / materials.
//
{

	// Set up Maya lights:
	// Nasty, need to scan for lights
	MStatus status;

	MItDag dagIterator( MItDag::kDepthFirst, MFn::kLight, &status );
	MDagPath lightPath;

	for (unsigned int i=0; i<8; i++)
	{
		D3D->LightEnable( i, false);
	}
	m_numberLightsEnabled = 0;

	HRESULT hr;
	for (; !dagIterator.isDone(); dagIterator.next())
	{
		if ( !dagIterator.getPath(lightPath) )
			continue;

		LightItem *lightItem = new LightItem;
		if (!lightItem)
			break;

		lightItem->m_objectDeleteMonitor = NULL;
		lightItem->m_objectDirtyMonitor = NULL;
		ZeroMemory(&(lightItem->m_lightDesc), sizeof(lightItem->m_lightDesc));

		D3DLIGHT9 *d3dLight = &(lightItem->m_lightDesc);
		d3dLight->Range = 10000.0f;
		d3dLight->Falloff = 0.0f;
		d3dLight->Diffuse.a = 1.0f;
		d3dLight->Ambient.a = 1.0f;
		d3dLight->Specular.a = 1.0f;
		d3dLight->Attenuation0 = 0.0f;
		d3dLight->Attenuation1 = 0.0f;
		d3dLight->Attenuation2 = 0.0f;

		// This code doesn't do all of the light attributes, but the
		// general ones are pretty much supported.
		//
		MFnLight    fnLight( lightPath );
		if ( lightPath.hasFn(MFn::kAmbientLight))
		{
			MColor      colorVal = fnLight.color();
			float intensity = fnLight.intensity();

			d3dLight->Type = D3DLIGHT_POINT;	
			d3dLight->Ambient.r = colorVal.r * intensity;
			d3dLight->Ambient.g = colorVal.g * intensity;
			d3dLight->Ambient.b = colorVal.b * intensity;
			d3dLight->Specular.r = 0.0f;
			d3dLight->Specular.g = 0.0f;
			d3dLight->Specular.b = 0.0f;
			d3dLight->Diffuse.r = d3dLight->Ambient.r;
			d3dLight->Diffuse.g = d3dLight->Ambient.g;
			d3dLight->Diffuse.b = d3dLight->Ambient.b;
			d3dLight->Attenuation0 = 1.0;
		}
		else if (lightPath.hasFn(MFn::kDirectionalLight) )
		{
			MColor      colorVal = fnLight.color();
			float intensity = fnLight.intensity();

			d3dLight->Type = D3DLIGHT_DIRECTIONAL;
			d3dLight->Diffuse.r = colorVal.r * intensity;
			d3dLight->Diffuse.g = colorVal.g * intensity;
			d3dLight->Diffuse.b = colorVal.b * intensity;
			d3dLight->Specular.r = d3dLight->Diffuse.r;
			d3dLight->Specular.g = d3dLight->Diffuse.g;
			d3dLight->Specular.b = d3dLight->Diffuse.b;
			d3dLight->Ambient.r = 0.0f;
			d3dLight->Ambient.g = 0.0f;
			d3dLight->Ambient.b = 0.0f;
		}
		else if (lightPath.hasFn(MFn::kPointLight))
		{
			d3dLight->Type = D3DLIGHT_POINT;

			MColor      colorVal = fnLight.color();
			float intensity = fnLight.intensity();

			d3dLight->Type = D3DLIGHT_POINT;
			d3dLight->Diffuse.r = colorVal.r * intensity;
			d3dLight->Diffuse.g = colorVal.g * intensity;
			d3dLight->Diffuse.b = colorVal.b * intensity;
			d3dLight->Specular.r = d3dLight->Diffuse.r;
			d3dLight->Specular.g = d3dLight->Diffuse.g;
			d3dLight->Specular.b = d3dLight->Diffuse.b;
			d3dLight->Ambient.r = 0.0f;
			d3dLight->Ambient.g = 0.0f;
			d3dLight->Ambient.b = 0.0f;
			d3dLight->Falloff = 0;
			// Should set attenuation based on "Decay Rate" attrib on light. TO ADD
			d3dLight->Attenuation0 = 1.0;
			d3dLight->Attenuation1 = 0.0f;
			d3dLight->Attenuation2 = 0.0f;
		}
		else if (lightPath.hasFn(MFn::kSpotLight))
		{
			d3dLight->Type = D3DLIGHT_SPOT;

			MColor      colorVal = fnLight.color();
			float intensity = fnLight.intensity();

			d3dLight->Type = D3DLIGHT_SPOT;
			d3dLight->Diffuse.r = colorVal.r * intensity;
			d3dLight->Diffuse.g = colorVal.g * intensity;
			d3dLight->Diffuse.b = colorVal.b * intensity;
			d3dLight->Specular = d3dLight->Diffuse;
			d3dLight->Ambient.r = 0.0f;
			d3dLight->Ambient.g = 0.0f;
			d3dLight->Ambient.b = 0.0f;

			MFnSpotLight fnSpotLight( lightPath );
			// Differs from OpenGL in that we don't want half the angle (divide by 2) 
			// in our setup.
			d3dLight->Phi = (float) (fnSpotLight.coneAngle() ) + 
							(float) (fnSpotLight.penumbraAngle() );
			d3dLight->Theta = d3dLight->Phi - (float) (fnSpotLight.penumbraAngle() );

			// Should set attenuation based on "Decay Rate" attrib on light. TO ADD
			d3dLight->Attenuation0 = 1.0;
			float dropOffVal = (float) fnSpotLight.dropOff() / 1000.0f;
			d3dLight->Attenuation1 = dropOffVal;
			d3dLight->Falloff = 1.0f;
		}
		else 
		{
			delete lightItem;
			lightItem = NULL;
			continue;
		}

		// Setup common position and direction information.
		if (lightItem && d3dLight)
		{
			MTransformationMatrix worldMatrix = lightPath.inclusiveMatrix();

			MVector translation = worldMatrix.translation( MSpace::kWorld );
			MVector direction( 0.0, 0.0, -1.0 ); 
			direction *= worldMatrix.asMatrix();
			direction.normalize();
			d3dLight->Position.x = (float)translation.x;
			d3dLight->Position.y = (float)translation.y;
			d3dLight->Position.z = (float)translation.z;
			d3dLight->Direction.x = (float)direction.x;
			d3dLight->Direction.y = (float)direction.y;
			d3dLight->Direction.z = (float)direction.z;
		}

		hr = D3D->SetLight( m_numberLightsEnabled, d3dLight);
		if (!SUCCEEDED(hr))
		{
			delete lightItem;
			lightItem = NULL;
			break;
		}
		hr = D3D->LightEnable(m_numberLightsEnabled, TRUE);
		if (!SUCCEEDED(hr))
		{
			delete lightItem;
			lightItem = NULL;
			break;
		}
		m_numberLightsEnabled++;

		// Only allow up to 8 lights to be built for now
		if (m_numberLightsEnabled >= 8)
			break;

		m_lightItemList.push_back( lightItem );
	}

	if (m_numberLightsEnabled == 0)
	{
		// Setup a headlight.
		D3DLIGHT9 Light;
		Light.Type = D3DLIGHT_DIRECTIONAL;
		Light.Specular.r = 1.0f; Light.Specular.g = 1.0f; Light.Specular.b = 1.0f; Light.Specular.a = 1.0f; 
		Light.Diffuse.r = 1.0f; Light.Diffuse.g = 1.0f; Light.Diffuse.b = 1.0f; Light.Diffuse.a = 1.0f; 
		Light.Ambient.r = 0.0f; Light.Ambient.g = 0.0f; Light.Ambient.b = 0.0f; Light.Ambient.a = 0.0f; 
		Light.Range = 100000;
		Light.Falloff = 0;
		Light.Direction = m_camera.m_vLookatPt - m_camera.m_vEyePt;	
		D3D->SetLight( 0, &Light);
		D3D->LightEnable( 0, true);

		// And a backlight.
		D3DLIGHT9 Light2;
		Light2.Type = D3DLIGHT_DIRECTIONAL;
		Light2.Specular.r = 1.0f; Light2.Specular.g = 1.0f; Light2.Specular.b = 1.0f; Light2.Specular.a = 1.0f; 
		Light2.Diffuse.r = 1.0f; Light2.Diffuse.g = 1.0f; Light2.Diffuse.b = 1.0f; Light2.Diffuse.a = 1.0f; 
		Light2.Ambient.r = 0.0f; Light2.Ambient.g = 0.0f; Light2.Ambient.b = 0.0f; Light2.Ambient.a = 0.0f; 
		Light2.Range = 100000;
		Light2.Falloff = 0;
		Light2.Direction = m_camera.m_vEyePt - m_camera.m_vLookatPt;	
		D3D->SetLight( 1, &Light2);
		D3D->LightEnable( 1, true);

		m_numberLightsEnabled = 2;
	}

	D3D->SetRenderState( D3DRS_LIGHTING, TRUE );

	// Make sure specular is on.
	D3D->SetRenderState( D3DRS_SPECULARENABLE, TRUE );
	D3D->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
	D3D->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
	// Set lighting to local-viewer
	D3D->SetRenderState(D3DRS_LOCALVIEWER, TRUE);
	// Make sure to auto-normalize normals
	D3D->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE );	

	return true;
}

//
// Get the geometry buffers for this bad boy
//
D3DGeometry* D3DResourceManager::getGeometry( const MDagPath& dagPath, LPDIRECT3DDEVICE9 D3D)
{
	D3DGeometry* Geometry = NULL;

	// Look for a cached mesh ...
	//
	// Check to see if object is in the list, if not added a
	// new item and cache some geometry
	GeometryItem *itemFound = NULL;

	GeometryItemList::const_iterator it, end_it;
	end_it = m_geometryItemList.end();
	for (it = m_geometryItemList.begin(); it != end_it;  it++)
	{
		GeometryItem *item = *it;
		if (item && item->m_objectPath == dagPath)
		{
			itemFound = item;
		}
	}
	// Build a new item, and add it to the list
	if (!itemFound)
	{
		itemFound = new GeometryItem;
		itemFound->m_objectPath = dagPath;
		Geometry = itemFound->m_objectGeometry = new D3DGeometry();
		if (Geometry)
		{
			Geometry->Populate( dagPath, D3D);
		}

		MFnDagNode dagNode(dagPath);
		MObject &obj = (MObject &) dagNode.object();
		itemFound->m_objectDeleteMonitor = 
			MNodeMessage::addNodeAboutToDeleteCallback( obj, geometryDeleteCallback, (void *)itemFound ); // Add callback for node deleted.
		// Don't get attr change messages during playback, so use node dirty also..sigh
		//itemFound->m_objectChangeMonitor = 
		//	MNodeMessage::addAttributeChangedCallback( obj, geometryChangedCallback, (void *)itemFound); // Add callback for attr changed.
		itemFound->m_objectChangeMonitor = 0;
		itemFound->m_objectChangeMonitor = 
			MNodeMessage::addNodeDirtyCallback( obj, geometryDirtyCallback, (void *)itemFound); // Add callback for node changed.		
		m_geometryItemList.push_back( itemFound );
	}
	else
	{
		Geometry = itemFound->m_objectGeometry;
	}

	// Create a new set of buffers for this mesh
	//
	return Geometry;
}


//
// Get the DirectX texture for a Maya texture node
//
D3DTexture* D3DResourceManager::getTexture( MObject& textureNode)
{
	D3DTexture* Texture = NULL;

	// Look for a cached texture ...
	//
	// Check to see if object is in the list, if not added a
	// new item and cache some texture
	TextureItem *itemFound = NULL;

	TextureItemList::const_iterator it, end_it;
	end_it = m_textureItemList.end();
	for (it = m_textureItemList.begin(); it != end_it;  it++)
	{
		TextureItem *item = *it;
		if (item && item->m_mayaNode.object() == textureNode)
		{
			itemFound = item;
		}
	}
	// Build a new item, and add it to the list
	if (!itemFound)
	{
		itemFound = new TextureItem();
		itemFound->m_mayaNode = textureNode;
		Texture = itemFound->m_texture = new D3DTexture( textureNode);

		itemFound->m_objectDeleteMonitor = 
			MNodeMessage::addNodeAboutToDeleteCallback( textureNode, textureDeleteCallback, (void *)itemFound ); // Add callback for node deleted.
		itemFound->m_objectChangeMonitor = 
			MNodeMessage::addAttributeChangedCallback( textureNode, textureChangedCallback, (void *)itemFound); // Add callback for attr changed.

		m_textureItemList.push_back( itemFound );
	}
	else
	{
		Texture = itemFound->m_texture;
	}

	// Create a new set of buffers for this mesh
	//
	return Texture;
}

bool					
D3DResourceManager::initializeDefaultSurfaceEffect( const MString &effectsLocation, LPDIRECT3DDEVICE9 D3D,
												    const MString & effectName )
//
// Description:
//		Initialize default surface effects found in a given directory.
//
{
	HRESULT hres;
	LPD3DXBUFFER pBufferErrors = NULL;

	// Shader flags for debugging
    DWORD dwShaderFlags = 0;
    #ifdef DEBUG_VS
	dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
    #ifdef DEBUG_PS
    dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

	ID3DXEffect* effect = NULL;

	MString effectLocation = effectsLocation + "\\" + effectName + ".fx";
	hres = D3DXCreateEffectFromFile( D3D, 
		effectLocation.asChar(),
		NULL, 
		NULL, 
		dwShaderFlags, 
		NULL, 
		&effect, 
		&pBufferErrors );
	if (FAILED(hres))
	{
		MGlobal::displayInfo("Failed to compile FX file: " + effectLocation);
		if (pBufferErrors)
		{
			const char *pCompilErrors = (const char*)pBufferErrors->GetBufferPointer();
			MGlobal::displayInfo(MString("Compiler errors:\n") + MString(pCompilErrors) );
		}
	}	
	else
	{
		// Create a new effect item
		//
		MGlobal::displayInfo("Maya default pixel shader loaded: " + effectLocation);
		SurfaceEffectItem *pei = new SurfaceEffectItem;
		if (pei)
		{
			pei->fEffect = effect;
			pei->fName = effectName;

			m_SurfaceEffectItemList.push_back( pei );
		}
	}

	return (m_SurfaceEffectItemList.size() > 0);
}


bool					
D3DResourceManager::initializePostEffects( const MString &effectsLocation, LPDIRECT3DDEVICE9 D3D)
//
// Description:
//		Initialize all effects found in a given directory.
//
{
	HRESULT hres;
	LPD3DXBUFFER pBufferErrors = NULL;

	// Shader flags for debugging
    DWORD dwShaderFlags = 0;
    #ifdef DEBUG_VS
	dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
    #ifdef DEBUG_PS
    dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

	// Should do a proper directory search for now it's just hard coded.
	MStringArray effectNames;
	effectNames.append("PostProcess_SobelFilter");
	effectNames.append("PostProcess_ToneMapFilter");
	effectNames.append("PostProcess_InvertFilter");

	for (unsigned int i=0; i<effectNames.length(); i++)
	{
		ID3DXEffect* effect = NULL;

		MString effectLocation = effectsLocation + "\\" + effectNames[i] + ".fx";
		hres = D3DXCreateEffectFromFile( D3D, 
		                           effectLocation.asChar(),
		                           NULL, 
		                           NULL, 
		                           dwShaderFlags, 
		                           NULL, 
		                           &effect, 
		                           &pBufferErrors );
		if (FAILED(hres))
		{
			MGlobal::displayInfo("Failed to compiler FX file" + effectLocation);
			if (pBufferErrors)
			{
				const char *pCompilErrors = (const char*)pBufferErrors->GetBufferPointer();
				MGlobal::displayInfo(MString("Compiler errors:\n") + MString(pCompilErrors) );
			}
		}	
		else
		{
			// Create a new effect item
			//
			PostEffectItem *pei = new PostEffectItem;
			if (pei)
			{
				pei->fEffect = effect;
				pei->fName = effectNames[i];

				m_PostEffectItemList.push_back( pei );
			}
		}
	}

	return (m_PostEffectItemList.size() > 0);
}
		

const MStringArray &	
D3DResourceManager::getListOfEnabledPostEffects()
{
	// Find out which ones are enabled...
	// Should get via some UI interface. For now use Maya optionVars.
	//
	// e.g. optionVar -sv "D3D_EFFECTS_LIST" "PostProcess_ToneMapFilter" ;
	// e.g. optionVar -sv "D3D_EFFECTS_LIST" "PostProcess_ToneMapFilter,PostProcess_SobelFilter" ;
	// e.g. optionVar -sv "D3D_EFFECTS_LIST" "" ; // Clear the list
	m_EnabledPostEffectItemList.clear();

	const MString effectListVar("D3D_EFFECTS_LIST");
	//MString effectList("PostProcess_ToneMapFilter, PostProcess_SobelFilter");
	MString effectList;
	if (!MGlobal::getOptionVarValue(effectListVar, effectList))
	{
		// Create an option var if none previously existed.
		MGlobal::setOptionVarValue(effectListVar, effectList);
	}
	else
	{
		// Parse the effect list to get back a list of enabled effects
		// (in order). Note that there is not restriction on effect
		// list order, and duplicates are allowed.
		effectList.split(' ', m_EnabledPostEffectItemList);
	}

	return m_EnabledPostEffectItemList;
}

//////////////////////////////////////////////////////////
void					
D3DResourceManager::clearResources(bool onlyInvalidItems, bool clearShaders)
{
	GeometryItemList::const_iterator git, end_git;
	end_git = m_geometryItemList.end();
	for (git = m_geometryItemList.begin(); git != end_git;  git++)
	{
		GeometryItem *item = *git;
		if (item)
		{
			if (!onlyInvalidItems || (onlyInvalidItems && !(item->m_objectPath.isValid() )))
			{
				if (item->m_objectGeometry)
				{
					delete item->m_objectGeometry;
					item->m_objectGeometry = NULL;
				}
				item->m_objectPath = MDagPath(); // Assign invalid dag path

				// Kill the delete monitor
				if (item->m_objectDeleteMonitor)
				{
					MMessage::removeCallback( item->m_objectDeleteMonitor );
					item->m_objectDeleteMonitor = 0;
				}
				// Kill the attr changed monitor
				if (item->m_objectChangeMonitor)
				{
					MMessage::removeCallback( item->m_objectChangeMonitor );	
					item->m_objectChangeMonitor = 0;					
				}
				// Kill node dirty monitor
				if (item->m_objectDirtyMonitor)
				{
					MMessage::removeCallback( item->m_objectDirtyMonitor );	
					item->m_objectDirtyMonitor = 0;										
				}
			}
		}
	}
	if (!onlyInvalidItems)
		m_geometryItemList.clear();

	TextureItemList::const_iterator tit, end_tit;
	end_tit = m_textureItemList.end();
	for (tit = m_textureItemList.begin(); tit != end_tit;  tit++)
	{
		TextureItem *item = *tit;
		if (item)
		{
			if (!onlyInvalidItems || (onlyInvalidItems && !(item->m_mayaNode.isValid() )))
			{
				if (item->m_texture)
				{
					delete item->m_texture;
					item->m_texture = NULL;
				}

				item->m_mayaNode = MObject::kNullObj;

				// Kill the delete monitor
				if (item->m_objectDeleteMonitor)
				{
					MMessage::removeCallback( item->m_objectDeleteMonitor );
					item->m_objectDeleteMonitor = 0;
				}
				// Kill the attr changed monitor
				if (item->m_objectChangeMonitor)
				{
					MMessage::removeCallback( item->m_objectChangeMonitor );	
					item->m_objectChangeMonitor = 0;					
				}
			}
		}
	}
	if (!onlyInvalidItems)
		m_textureItemList.clear();

	if (clearShaders)
	{
		// Clean up post effects list
		{
		PostEffectItemList::const_iterator eit, end_eit;
		end_eit = m_PostEffectItemList.end();
		for (eit = m_PostEffectItemList.begin(); eit != end_eit;  eit++)
		{
			PostEffectItem *item = *eit;
			if (item)
			{
				if (item->fEffect)
				{
					item->fEffect->Release();
					item->fEffect = NULL;
				}			
				delete item;
			}
		}
		m_PostEffectItemList.clear();
		m_EnabledPostEffectItemList.clear();
	}

		// Clean up surface effects list
		{
			SurfaceEffectItemList::const_iterator eit, end_eit;
			end_eit = m_SurfaceEffectItemList.end();
			for (eit = m_SurfaceEffectItemList.begin(); eit != end_eit;  eit++)
			{
				SurfaceEffectItem *item = *eit;
				if (item)
				{
					if (item->fEffect)
					{
						item->fEffect->Release();
						item->fEffect = NULL;
					}			
					delete item;
				}
			}
			m_SurfaceEffectItemList.clear();
		}
	}
}


#endif /* D3D9_SUPPORTED */



