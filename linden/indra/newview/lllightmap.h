/** 
 * @file lllightmap.h
 * @brief LLLightmap class header file
 *
 * Based on article from Ogre wiki: http://www.ogre3d.org/wiki/index.php/Light_mapping
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#ifndef LL_LLLIGHTMAP_H
#define LL_LLLIGHTMAP_H

#include <OGRE/Ogre.h>
#include <boost/shared_ptr.hpp>

class MeshInformationCache;

namespace cimg_library
{
	template<typename T> struct CImg;
};

//! Special raycasting implemention optimized for light map generation.
/*! Ogre renderer should be initialized and Ogre context should be set before using this class.
*/
class LLLightMapRaycast
{
public:
   //! default constructor
   LLLightMapRaycast();

   //! destructor
   ~LLLightMapRaycast();

   //! do a raycast into the scene
   /*! 
       \note maxDistance is not checked per poly. F.ex. true may be returned
             if ray hits a polygon on the far side of an object, even if the
             distance to the polygon is > maxDistance.

       \param ray ray to use for the raycasting
       \param maxDistance max distance for the ray
       \return True if ray hit an object, false otherwise
   */
   bool raycast(MeshInformationCache *meshCache, const Ogre::Ray &ray, Ogre::Real maxDistance, const Ogre::Entity *ignoreEntity = 0);

private:
   Ogre::Vector3 getBarycentricCoordinates(const Ogre::Vector3 &P1, const Ogre::Vector3 &P2, const Ogre::Vector3 &P3, const Ogre::Vector3 &P);
   bool hasAlpha(const Ogre::Vector2 &uv, const Ogre::MaterialPtr &material);


   Ogre::RaySceneQuery *mRayQuery;
};

class LLLightMap : public Ogre::ManualResourceLoader
{
public:
   //! Generate lightmap for the specified subentity using the default options
   LLLightMap(Ogre::SubEntity* pSubEntity, MeshInformationCache *meshCache = 0);

   //! Generate lightmap for the subentity using the specified options
	LLLightMap(Ogre::SubEntity* pSubEntity, size_t numLights = 1, Ogre::Real PixelsPerUnit = 0, int iTexSize = 512, bool useGlobalLight = true,
         bool bDebugLightmaps = false, MeshInformationCache *meshCache = 0);
	~LLLightMap(void);

   void loadResource(Ogre::Resource *resource);
	Ogre::String GetName();
	static void ResetCounter();

	struct SortCoordsByDistance
	{
      bool operator()(std::pair<int, int> &left, std::pair<int, int> &right)
		{
			return (left.first*left.first + left.second*left.second) < 
				   (right.first*right.first + right.second*right.second);
		}
	};

   static void setDefaultMaxLights(int maxLights);
   static void setDefaultPixelsPerUnit(float pixelsPerUnit);
   static void setDefaultTexSize(int maxTexSize);
   static void setDefaultUseGlobalLight(bool useGlobalLight);
   static void setDefaultDebug(bool debug);

protected:
	void LightTriangle(const Ogre::Vector3 &P1, const Ogre::Vector3 &P2, const Ogre::Vector3 &P3,
					   const Ogre::Vector3 &N1, const Ogre::Vector3 &N2, const Ogre::Vector3 &N3,
					   const Ogre::Vector2 &T1, const Ogre::Vector2 &T2, const Ogre::Vector2 &T3);
   Ogre::uint8 GetLightIntensity(const Ogre::Vector3 &Position, const Ogre::Vector3 &Normal);
   bool Raycast(const Ogre::Light *light, const Ogre::Vector3 &Position, const Ogre::Vector3 &Normal, float *outShadowStrength, bool *facingAway);
   bool Raycast_directional(const Ogre::Light *light, const Ogre::Vector3 &Position, const Ogre::Vector3 &Normal);
	bool CalculateLightMap();
	void AssignMaterial();
	void CreateTexture();
	void FillInvalidPixels();
	static void BuildSearchPattern();

	/// Convert between texture coordinates given as reals and pixel coordinates given as integers
   inline int GetPixelCoordinate(Ogre::Real TextureCoord);
   inline Ogre::Real GetTextureCoordinate(int iPixelCoord);

	/// Calculate coordinates of P in terms of P1, P2 and P3
	/// P = x*P1 + y*P2 + z*P3
	/// If any of P.x, P.y or P.z are negative then P is outside of the triangle
   Ogre::Vector3 GetBarycentricCoordinates(const Ogre::Vector2 &P1, const Ogre::Vector2 &P2, const Ogre::Vector2 &P3, const Ogre::Vector2 &P);
	/// Get the surface area of a triangle
   Ogre::Real GetTriangleArea(const Ogre::Vector3 &P1, const Ogre::Vector3 &P2, const Ogre::Vector3 &P3);

   static int sm_iMaxLights;
   static float sm_fPixelsPerUnit;
   static int sm_iMaxTexSize;
   static bool sm_bUseGlobalLight;
   static bool sm_bDebug;

   Ogre::TexturePtr m_Texture;
   Ogre::MaterialPtr m_Material;
   Ogre::SubEntity* m_pSubEntity;
   boost::shared_ptr<cimg_library::CImg<unsigned char> > m_LightMap;
   Ogre::String m_LightMapName;
	int m_iTexSize;
   int m_iMaxTexSize;
	int m_iCoordSet;
   size_t m_iNumLights;
	static int m_iLightMapCounter;
   Ogre::Real m_PixelsPerUnit;
   static std::vector<std::pair<int, int> > m_SearchPattern;
   bool m_bUseGlobalLight;
	bool m_bDebugLightmaps;
   MeshInformationCache *m_pMeshCache;
   LLLightMapRaycast m_Raycaster;
};

int LLLightMap::GetPixelCoordinate(Ogre::Real TextureCoord)
{
	int iPixel = TextureCoord*m_iTexSize;
	if (iPixel < 0)
		iPixel = 0;
	if (iPixel >= m_iTexSize)
		iPixel = m_iTexSize-1;
	return iPixel;
}

Ogre::Real LLLightMap::GetTextureCoordinate(int iPixelCoord)
{
   return (Ogre::Real(iPixelCoord)+0.5)/Ogre::Real(m_iTexSize);
}


class LLEntityLightMap
{
public:
   //! Generate lightmaps for the entity using specified settings
	LLEntityLightMap(Ogre::Entity* pEntity, size_t numLights = 1, Ogre::Real PixelsPerUnit = 0, int iTexSize = 512, bool useGlobalLight = true, bool bDebugLightmaps = false, 
         MeshInformationCache *meshCache = 0)
	{
		int i, iNumSubEntities = pEntity->getNumSubEntities();
		for (i=0; i<iNumSubEntities; ++i)
		{
         boost::shared_ptr<LLLightMap> LightMap(new LLLightMap(pEntity->getSubEntity(i), numLights, PixelsPerUnit, iTexSize, useGlobalLight, bDebugLightmaps, meshCache));
			m_LightMaps.push_back(LightMap);
		}
	}

   //! Generate lightmaps for the entity using default settings
   LLEntityLightMap(Ogre::Entity* pEntity, MeshInformationCache *meshCache = 0)
	{
		int i, iNumSubEntities = pEntity->getNumSubEntities();
		for (i=0; i<iNumSubEntities; ++i)
		{
         boost::shared_ptr<LLLightMap> LightMap(new LLLightMap(pEntity->getSubEntity(i), meshCache));
			m_LightMaps.push_back(LightMap);
		}
	}

protected:
   std::vector<boost::shared_ptr<LLLightMap> > m_LightMaps;
};

#endif // LL_LLLIGHTMAP_H
