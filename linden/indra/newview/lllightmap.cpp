/**
 * @file lllightmap.cpp
 * @brief LLLightMap class implementation
 *
 * From Ogre wiki: http://www.ogre3d.org/wiki/index.php/Light_mapping
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#include "llviewerprecompiledheaders.h"
#include "lllightmap.h"
#include <CImg.h>      // Open source image library (http://cimg.sourceforge.net/)
#include <OGRE/Ogre.h>
#include "llogre.h"
#include "llviewerobject.h"
#include "llogreobject.h"

using namespace Ogre;

int LLLightMap::sm_iMaxLights = 2;
float LLLightMap::sm_fPixelsPerUnit = 20.f;
int LLLightMap::sm_iMaxTexSize = 256;
bool LLLightMap::sm_bUseGlobalLight = true;
bool LLLightMap::sm_bDebug = false;


std::vector<std::pair<int, int> > LLLightMap::m_SearchPattern;
int LLLightMap::m_iLightMapCounter = 0;


LLLightMapRaycast::LLLightMapRaycast()
{
   mRayQuery = LLOgreRenderer::getPointer()->getSceneMgr()->createRayQuery(Ogre::Ray());
   mRayQuery->setSortByDistance(true);
   // To sort or not to sort? sorting is not necessary if we don't use maxDistance in raycasting (f.ex. sun as the light)
   // Otherwise, if we sort we can break after going past maxDistance which might speed up the process slightly
}

LLLightMapRaycast::~LLLightMapRaycast()
{
}

bool LLLightMapRaycast::raycast(MeshInformationCache *meshCache, const Ogre::Ray &ray, Ogre::Real maxDistance, const Ogre::Entity *ignoreEntity)
{   
   // set ray for the query
   mRayQuery->setRay(ray);

   // execute the query, returns a vector of hits
   if (mRayQuery->execute().size() <= 0)
   {
      // raycast did not hit an objects bounding box
      return false;
   }
  
   const bool isMaxDistanceSet = maxDistance >= 0.f;

   Ogre::RaySceneQueryResult &query_result = mRayQuery->getLastResults();
   for (size_t qr_idx = 0; qr_idx < query_result.size(); qr_idx++)
   {
     if (isMaxDistanceSet && query_result[qr_idx].distance > maxDistance)
        break;
    
     // only check this result if its a hit against an entity
     if ((query_result[qr_idx].movable != NULL) &&
         (query_result[qr_idx].movable->getMovableType().compare("Entity") == 0))
     {
         // get the entity to check
         Ogre::Entity *pentity = static_cast<Ogre::Entity*>(query_result[qr_idx].movable);
         if (ignoreEntity == pentity)
            continue;

         //// See that the entity associates to a viewerobject (this makes sure that f.ex. we don't check the avatar when raycastin)
         const Ogre::Any& any = pentity->getUserAny();
         if (any.isEmpty()) continue;
         LLOgreObject* obj = Ogre::any_cast<LLOgreObject*>(any);
         if (!obj) continue;
         LLViewerObject* vo = obj->getViewerObject();
         if (!vo) continue;

         size_t n;
         for (n=0 ; n<pentity->getNumSubEntities() ; ++n)
         {
            Ogre::SubEntity *subentity = pentity->getSubEntity(n);
            const MeshInformationCache::CacheMeshData *meshData = meshCache->getMeshInformation(subentity);

            bool isTransparent = subentity->getMaterial()->isTransparent();

            size_t i;
            for (i = 0; i < meshData->mIndices.size() ; i += 3)
            {
               // check for a hit against this triangle
               std::pair<bool, Ogre::Real> hit = Ogre::Math::intersects(ray, meshData->mVertices[meshData->mIndices[i]],
                    meshData->mVertices[meshData->mIndices[i+1]], meshData->mVertices[meshData->mIndices[i+2]], true, true);
               
               // did we hit something?
               if (hit.first)
               {
                  if (isTransparent)
                  {
                     Ogre::Vector3 hitPoint = ray.getPoint(hit.second);

                     Ogre::Vector3 barycentricCoords = getBarycentricCoordinates(meshData->mVertices[meshData->mIndices[i]],
                          meshData->mVertices[meshData->mIndices[i+1]], meshData->mVertices[meshData->mIndices[i+2]], hitPoint);

                     Ogre::Vector2 uv1 = meshData->mTextureCoords[meshData->mIndices[i]];
                     Ogre::Vector2 uv2 = meshData->mTextureCoords[meshData->mIndices[i+1]];
                     Ogre::Vector2 uv3 = meshData->mTextureCoords[meshData->mIndices[i+2]];

                     Ogre::Vector2 uvp = barycentricCoords.x * uv1 + barycentricCoords.y * uv2 + barycentricCoords.z * uv3;

                     if (hasAlpha(uvp, subentity->getMaterial()))
                     {
                        break;
                     }
                  }
                  return true;
               }
            }
         }
      }       
   }

   return false;
}

bool LLLightMapRaycast::hasAlpha(const Ogre::Vector2 &uv, const Ogre::MaterialPtr &material)
{
   Ogre::TextureUnitState *tu = material->getTechnique(0)->getPass(0)->getTextureUnitState(0);
   if (tu)
   {
      Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().getByName(tu->getTextureName());
      Ogre::HardwarePixelBufferSharedPtr readbuffer = texture->getBuffer();
      readbuffer->lock(Ogre::HardwareBuffer::HBL_READ_ONLY );
      const PixelBox &pixelbox = readbuffer->getCurrentLock();	
      uint8* data = static_cast<uint8*>(pixelbox.data);
      
      size_t bytes = Ogre::PixelUtil::getNumElemBytes(texture->getFormat());

      Ogre::Vector2 pixelUV(uv.x * texture->getWidth(), uv.y * texture->getHeight());

      uint8* pixel = data + (size_t)pixelUV.x * bytes + (size_t)pixelUV.y * pixelbox.rowPitch * bytes;
      Ogre::ColourValue color;
      Ogre::PixelUtil::unpackColour(&color, texture->getFormat(), pixel);
      
      readbuffer->unlock();

      if (color.a < 0.5f)
         return true;
   }
   return false;
}

Ogre::Vector3 LLLightMapRaycast::getBarycentricCoordinates(const Ogre::Vector3 &P1, const Ogre::Vector3 &P2, const Ogre::Vector3 &P3, const Ogre::Vector3 &P)
{
   Vector3 Coordinates = Ogre::Vector3::ZERO;

   Ogre::Real A = P1.x - P3.x;
   Ogre::Real B = P2.x - P3.x;
   Ogre::Real C = P3.x - P.x;
   Ogre::Real D = P1.y - P3.y;
   Ogre::Real E = P2.y - P3.y;
   Ogre::Real F = P3.y - P.y;
   Ogre::Real G = P1.z - P3.z;
   Ogre::Real H = P2.z - P3.z;
   Ogre::Real I = P3.z - P.z;

   Ogre::Real denom = A*(E+H)-B*(D+G);
   if (fabs(denom) >= 1e-6)
   {
      Coordinates.x = (B*(F+I)-C*(E+H)) / denom;
   }
   denom = B*(D+G) - A*(E+H);
   if (fabs(denom) >= 1e-6)
   {
      Coordinates.y = (A*(F+I)-C*(D+G)) / denom;
   }

	Coordinates.z = 1 - Coordinates.x - Coordinates.y;

	return Coordinates;
}

// ***************************************

LLLightMap::LLLightMap(SubEntity* pSubEntity, MeshInformationCache *meshCache)
: m_pSubEntity(pSubEntity)
, m_iTexSize(sm_iMaxTexSize)
, m_iMaxTexSize(sm_iMaxTexSize)
, m_iCoordSet(0)
, m_PixelsPerUnit(sm_fPixelsPerUnit)
, m_bUseGlobalLight(sm_bUseGlobalLight)
, m_bDebugLightmaps(sm_bDebug)
, m_pMeshCache(meshCache)
, m_iNumLights(sm_iMaxLights)
{
	if (m_SearchPattern.empty())
		BuildSearchPattern();

	m_LightMapName = "LightMap" + StringConverter::toString(m_iLightMapCounter++);

	if (CalculateLightMap())
   {
   	AssignMaterial();
   }
}

LLLightMap::LLLightMap(SubEntity* pSubEntity, size_t numLights, Real PixelsPerUnit, int iTexSize, bool useGlobalLight, bool bDebugLightmaps, MeshInformationCache *meshCache)
: m_pSubEntity(pSubEntity)
, m_iTexSize(iTexSize)
, m_iMaxTexSize(iTexSize)
, m_iCoordSet(0)
, m_PixelsPerUnit(PixelsPerUnit)
, m_bUseGlobalLight(useGlobalLight)
, m_bDebugLightmaps(bDebugLightmaps)
, m_pMeshCache(meshCache)
, m_iNumLights(numLights)
{
	if (m_SearchPattern.empty())
		BuildSearchPattern();

	m_LightMapName = "LightMap" + StringConverter::toString(m_iLightMapCounter++);

	if (CalculateLightMap())
   {
   	AssignMaterial();
   }
}


LLLightMap::~LLLightMap(void)
{
	if (!m_Texture.isNull())
	{
		TextureManager::getSingleton().remove((ResourcePtr)m_Texture);
		m_Texture.setNull();
	}
}

void LLLightMap::ResetCounter()
{
	m_iLightMapCounter = 0;
}

//static
void LLLightMap::setDefaultMaxLights(int maxLights)
{
   sm_iMaxLights = maxLights;
}

//static
void LLLightMap::setDefaultPixelsPerUnit(float pixelsPerUnit)
{
   sm_fPixelsPerUnit = pixelsPerUnit;
}

//static
void LLLightMap::setDefaultTexSize(int maxTexSize)
{
   sm_iMaxTexSize = maxTexSize;
}

//static
void LLLightMap::setDefaultUseGlobalLight(bool useGlobalLight)
{
   sm_bUseGlobalLight = useGlobalLight;
}

//static
void LLLightMap::setDefaultDebug(bool debug)
{
   sm_bDebug = debug;
}

void LLLightMap::loadResource(Resource *resource)
{
	Texture* texture = (Texture*)resource;

	// Get the pixel buffer
	HardwarePixelBufferSharedPtr pixelBuffer = texture->getBuffer();

	// Lock the pixel buffer and get a pixel box
	pixelBuffer->lock(HardwareBuffer::HBL_DISCARD); // for best performance use HBL_DISCARD!
	const PixelBox &pixelBox = pixelBuffer->getCurrentLock();

	uint8* data = static_cast<uint8*>(pixelBox.data);

	assert(pixelBox.getWidth() == pixelBox.getHeight());
	const int iTexSize = (int)pixelBox.getWidth();
	const int iRowPitch = (int)pixelBox.rowPitch;


	int i, j;

	for (j = 0; j < iTexSize; j++)
	{
		for(i = 0; i < iTexSize; i++)
		{
			data[iRowPitch*j + i] = (*m_LightMap)(i, j);
		}
	}

	// Unlock the pixel buffer
	pixelBuffer->unlock();
}

String LLLightMap::GetName()
{
	return m_LightMapName;
}

Vector3 LLLightMap::GetBarycentricCoordinates(const Vector2 &P1, const Vector2 &P2, const Vector2 &P3, const Vector2 &P)
{
	Vector3 Coordinates(0.0);
	Real denom = (-P1.x * P3.y - P2.x * P1.y + P2.x * P3.y + P1.y * P3.x + P2.y * P1.x - P2.y * P3.x);

	if (fabs(denom) >= 1e-6)
	{
		Coordinates.x = (P2.x * P3.y - P2.y * P3.x - P.x * P3.y + P3.x * P.y - P2.x * P.y + P2.y * P.x) / denom;
		Coordinates.y = -(-P1.x * P.y + P1.x * P3.y + P1.y * P.x - P.x * P3.y + P3.x * P.y - P1.y * P3.x) / denom;
//		Coordinates.z = (-P1.x * P.y + P2.y * P1.x + P2.x * P.y - P2.x * P1.y - P2.y * P.x + P1.y * P.x) / denom;
	}
	Coordinates.z = 1 - Coordinates.x - Coordinates.y;

	return Coordinates;
}

Real LLLightMap::GetTriangleArea(const Vector3 &P1, const Vector3 &P2, const Vector3 &P3)
{
	return 0.5*(P2-P1).crossProduct(P3-P1).length();
}

bool LLLightMap::CalculateLightMap()
{
	// Reset the lightmap to all 0's
	if (m_LightMap)
		m_LightMap->fill(0);

	// Get the submesh
	SubMesh* submesh = m_pSubEntity->getSubMesh();

   const MeshInformationCache::CacheMeshData *meshData = m_pMeshCache->getMeshInformation(m_pSubEntity);

	{
		VertexData* vertex_data = submesh->useSharedVertices ? submesh->parent->sharedVertexData : submesh->vertexData;
		// Get last set of texture coordinates
		int i = 0;
		const VertexElement* texcoordElem;
		const VertexElement* pCurrentElement = NULL;
		do
		{
			texcoordElem = pCurrentElement;
			pCurrentElement = vertex_data->vertexDeclaration->findElementBySemantic(Ogre::VES_TEXTURE_COORDINATES, i++);
		} while (pCurrentElement);
		m_iCoordSet = i-2;

  		if (!texcoordElem)
			return false;
   }

   if (m_PixelsPerUnit && m_Texture.isNull())
	{
		Real SurfaceArea = 0;
      size_t i;
      for (i=0 ; i<meshData->mIndices.size() ; i += 3)
      {

         SurfaceArea += GetTriangleArea(meshData->mVertices[meshData->mIndices[i]],
                        meshData->mVertices[meshData->mIndices[i + 1]],
                        meshData->mVertices[meshData->mIndices[i + 2]]);
		}
		Real TexSize = Math::Sqrt(SurfaceArea) * m_PixelsPerUnit;

		int iTexSize = 1;
		while (iTexSize < TexSize)
			iTexSize *= 2;

      m_iTexSize = std::min(m_iMaxTexSize, iTexSize);
   }
   if (m_iTexSize < 32) // don't bother creating tiny lightmaps, they won't be very visible anyway
      return false;

   // Create the texture with the new size
   CreateTexture();

   size_t i;
   for (i=0 ; i<meshData->mIndices.size() ; i += 3)
   {
      LightTriangle(meshData->mVertices[meshData->mIndices[i]],
                    meshData->mVertices[meshData->mIndices[i+1]],
                    meshData->mVertices[meshData->mIndices[i+2]],

						  meshData->mNormals[meshData->mIndices[i]],
                    meshData->mNormals[meshData->mIndices[i+1]],
                    meshData->mNormals[meshData->mIndices[i+2]],

                    meshData->mTextureCoords2[meshData->mIndices[i]],
                    meshData->mTextureCoords2[meshData->mIndices[i+1]],
                    meshData->mTextureCoords2[meshData->mIndices[i+2]]);
   }

	FillInvalidPixels();
	m_LightMap->blur(1.0);
//   std::string filename = std::string("c:/temp/lm/") + m_LightMapName + std::string(".bmp");
//   m_LightMap->save_bmp(filename.c_str());

	return true;
}

void LLLightMap::CreateTexture()
{
	if (!m_Texture.isNull())
		return;
	m_LightMap.reset(new cimg_library::CImg<unsigned char>(m_iTexSize, m_iTexSize, 1, 2, 0));
	if (TextureManager::getSingleton().resourceExists(m_LightMapName))
		TextureManager::getSingleton().remove(m_LightMapName);
	// Create the texture
	m_Texture = TextureManager::getSingleton().createManual(
			m_LightMapName, // name
			ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
			TEX_TYPE_2D,      // type
			m_iTexSize, m_iTexSize,         // width & height
//			-1,                // number of mipmaps
         Ogre::MIP_DEFAULT,
			PF_L8,     // pixel format
			TU_DEFAULT,
			this);
}

void LLLightMap::AssignMaterial()
{
	if (!m_Material.isNull() || m_Texture.isNull())
		return;

	if (MaterialManager::getSingleton().resourceExists(m_LightMapName))
		MaterialManager::getSingleton().remove(m_LightMapName);
	if (m_bDebugLightmaps)
	{
		m_Material = MaterialManager::getSingleton().create(m_LightMapName, StringUtil::BLANK, false);
	}
	else
	{
      m_Material = m_pSubEntity->getMaterial();
	}
	Pass* pPass = m_Material->getTechnique(0)->getPass(0);

   TextureUnitState* pTextureUnitState = pPass->getTextureUnitState("lightmap");
   if (pTextureUnitState)
   {
      //m_Material = m_Material->clone(m_LightMapName);
      //Pass* pPass = m_Material->getTechnique(0)->getPass(0);
      //pTextureUnitState = pPass->getTextureUnitState("lightmap");
      pTextureUnitState->setTextureName(m_Texture->getName());
      pTextureUnitState->setTextureCoordSet(m_iCoordSet);
//      m_pSubEntity->setMaterialName(m_LightMapName);
      m_pSubEntity->setMaterialName(m_Material->getName());

   } else
   {
   	pTextureUnitState = pPass->createTextureUnitState(m_Texture->getName(), m_iCoordSet);
      pTextureUnitState->setName("lightmap");
	   pTextureUnitState->setColourOperation(LBO_MODULATE);
	   pTextureUnitState->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);
      m_pSubEntity->setMaterialName(m_Material->getName());
   }
}

void LLLightMap::FillInvalidPixels()
{
	int i, j;
	int x, y;
   std::vector<std::pair<int, int> >::iterator itSearchPattern;
	for (i=0; i<m_iTexSize; ++i)
	{
		for (j=0; j<m_iTexSize; ++j)
		{
			// Invalid pixel found
			if ((*m_LightMap)(i, j, 0, 1) == 0)
			{
				for (itSearchPattern = m_SearchPattern.begin(); itSearchPattern != m_SearchPattern.end(); ++itSearchPattern)
				{
					x = i+itSearchPattern->first;
					y = j+itSearchPattern->second;
					if (x < 0 || x >= m_iTexSize)
						continue;
					if (y < 0 || y >= m_iTexSize)
						continue;
					// If search pixel is valid assign it to the invalid pixel and stop searching
					if ((*m_LightMap)(x, y, 0, 1) == 1)
					{
						(*m_LightMap)(i, j) = (*m_LightMap)(x, y);
						break;
					}
				}
			}
		}
	}
}

void LLLightMap::BuildSearchPattern()
{
	m_SearchPattern.clear();
	const int iSize = 5;
	int i, j;
	for (i=-iSize; i<=iSize; ++i)
	{
		for (j=-iSize; j<=iSize; ++j)
		{
			if (i==0 && j==0)
				continue;
         m_SearchPattern.push_back(std::make_pair(i, j));
		}
	}
	sort(m_SearchPattern.begin(), m_SearchPattern.end(), SortCoordsByDistance());
}

void LLLightMap::LightTriangle(const Vector3 &P1, const Vector3 &P2, const Vector3 &P3,
							  const Vector3 &N1, const Vector3 &N2, const Vector3 &N3,
							  const Vector2 &T1, const Vector2 &T2, const Vector2 &T3)
{
	Vector2 TMin = T1, TMax = T1;
	TMin.makeFloor(T2);
	TMin.makeFloor(T3);
	TMax.makeCeil(T2);
	TMax.makeCeil(T3);
	int iMinX = GetPixelCoordinate(TMin.x);
	int iMinY = GetPixelCoordinate(TMin.y);
	int iMaxX = GetPixelCoordinate(TMax.x);
	int iMaxY = GetPixelCoordinate(TMax.y);
	int i, j;
	Vector2 TextureCoord;
	Vector3 BarycentricCoords;
	Vector3 Pos;
	Vector3 Normal;
	for (i=iMinX; i<=iMaxX; ++i)
	{
		for (j=iMinY; j<=iMaxY; ++j)
		{
			TextureCoord.x = GetTextureCoordinate(i);
			TextureCoord.y = GetTextureCoordinate(j);
			BarycentricCoords = GetBarycentricCoordinates(T1, T2, T3, TextureCoord);
			Pos = BarycentricCoords.x * P1 + BarycentricCoords.y * P2 + BarycentricCoords.z * P3;
			Normal = BarycentricCoords.x * N1 + BarycentricCoords.y * N2 + BarycentricCoords.z * N3;
			Normal.normalise();
			if ((*m_LightMap)(i, j, 0, 1) == 1 || BarycentricCoords.x < 0 || BarycentricCoords.y < 0 || BarycentricCoords.z < 0)
				continue;
			(*m_LightMap)(i, j) = GetLightIntensity(Pos, Normal);
			(*m_LightMap)(i, j, 0, 1) = 1;
		}
	}
	
}

/*
This is the only function which you should need to modify. Basically given the position coordinate
and the surface normal at that point, you should return the light intensity value as a number between
0 and 255. In this example I use the PhysX library to cast a ray in a fixed direction to see if it
intersects with any other objects in the scene, if it does then this point is in the shade.
*/
uint8 LLLightMap::GetLightIntensity(const Vector3 &Position, const Vector3 &Normal)
{
   const uint8 AmbientValue = 100;
	const uint8 MaxValue = 255;

   LLOgreRenderer *renderer = LLOgreRenderer::getPointer();
   const Ogre::Light *globalLight = renderer->getGlobalLight();
   Ogre::Entity *parentEntity = m_pSubEntity->getParent();
   const Ogre::LightList &lightlist = parentEntity->queryLights();

   float shadowStrength = 0.f;
   uint8 LightValue = MaxValue;
   int hit = 0;
   int actualLights = 0;
   if (m_bUseGlobalLight)
   {
      actualLights++;
   if (Raycast_directional(globalLight, Position, Normal))
   {
      hit++;
      shadowStrength = 0.6f;
   }
   }

   size_t numLights = std::min(lightlist.size(), m_iNumLights + 1); // + 1 for global light (it should always be first on the light list and we skip it)
   size_t n;
   
   
   for (n=0 ; n<numLights ; ++n)
   {
      if (lightlist[n] == globalLight)
         continue;

      actualLights++;

      float newStrength = 0.0f;
      bool facingAway = false;
      bool bHit = Raycast(lightlist[n], Position, Normal, &newStrength, &facingAway);


	   if (bHit)
	   {
         shadowStrength = std::max(shadowStrength, newStrength);
         hit++;
	   } else if (facingAway == false && m_bUseGlobalLight)
         shadowStrength = 0.f;
   }
   if (actualLights == 0)
      return LightValue;

//   if (hit >= actualLights - 1)
   if (hit >= 1)
   {
      LightValue = AmbientValue  + (MaxValue - AmbientValue) * (1.f - shadowStrength); // pixel in shade
   }

   return LightValue;
}

bool LLLightMap::Raycast(const Ogre::Light *light, const Vector3 &Position, const Vector3 &Normal, float *outShadowStrength, bool *facingAway)
{
   const Real Tolerance = 1e-3;

   Ogre::Vector3 LightDirection = Position - light->getDerivedPosition();

	Ogre::Real maxDistance = LightDirection.length();
   LightDirection.normalise();

   // Calculate light strength at point to determinate shadow strength
   float strength = 1.0f / (light->getAttenuationConstant() + maxDistance * light->getAttenuationLinear() + maxDistance * maxDistance * light->getAttenuationQuadric());
   *outShadowStrength = std::max(0.f, std::min(1.0f, strength));
   

	Real Intensity = -LightDirection.dotProduct(Normal);
   if (Intensity < Tolerance)
   {
      *facingAway = true;
      return false;
   }
   *facingAway = false;


   Vector3 Origin = Position - LightDirection * Tolerance;
   
   Ogre::Ray ray(Origin, -LightDirection);

   Ogre::Entity *lightEntity = 0;
   const Ogre::Any& any = light->getUserAny();
   if (!any.isEmpty())
   {
      LLOgreObject* obj = Ogre::any_cast<LLOgreObject*>(any);
      if (obj)
      {
         lightEntity = obj->getEntity();
      }
   }
   
  
   return m_Raycaster.raycast(m_pMeshCache, ray, maxDistance, lightEntity);
}

bool LLLightMap::Raycast_directional(const Ogre::Light *light, const Vector3 &Position, const Vector3 &Normal)
{
   const Real Tolerance = 1e-3;

   const Ogre::Vector3 &LightDirection = light->getDirection();
	Ogre::Real maxDistance = -1.f;

	Real Intensity = -LightDirection.dotProduct(Normal);
   if (Intensity < Tolerance)
      return false;

   Vector3 Origin = Position - LightDirection * Tolerance;
   Ogre::Ray ray(Origin, -LightDirection);

  
   return m_Raycaster.raycast(m_pMeshCache, ray, maxDistance);
}

