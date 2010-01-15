#include"llviewerprecompiledheaders.h"
#include"llogrepostprocess.h"
#include"llogre.h"


extern LLDispatcher gGenericDispatcher;

const std::string PostProcessDispatchHandler::mMessageID("RexPostP");
bool PostProcessDispatchHandler::operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
      const LLDispatchHandler::sparam_t& string)
{
   // We may get commas instead of dots in the string representation of the floats
   unsigned int id = Ogre::StringConverter::parseUnsignedInt(string[0]);
   bool enable = Ogre::StringConverter::parseBool(string[1]);

   if (enable)
   {
      llinfos << "Server enabling postprocess effect id: " << id << llendl;
      LLOgreRenderer::getPointer()->getPostprocess()->setPostprocess(LLOgreRenderer::getPointer()->getViewport(), id, -1, LLOgrePostprocessItem::ENABLER_SERVER);
   } else
   {
      llinfos << "Server disabling postprocess effect id: " << id << llendl;
      LLOgreRenderer::getPointer()->getPostprocess()->removePostprocess(LLOgreRenderer::getPointer()->getViewport(), id);
   }

   return true;
}

// ***********************

LLOgrePostprocess::LLOgrePostprocess(Ogre::RenderWindow* mRender):
hdrListener(0)
{	
   gGenericDispatcher.addHandler(mDispatchPostProcess.mMessageID, &mDispatchPostProcess);

	//loadAllViewportsInRenderWindow(mRender);
	//createMaterialParameterList();
	mActiveParameters.setNull();
	loadAllViewportsInRenderWindow(mRender);
}

LLOgrePostprocess::~LLOgrePostprocess()
{
	delete hdrListener;
}

void LLOgrePostprocess::loadAllViewportsInRenderWindow(Ogre::RenderWindow* mRender)
{
	updatePostprocessNamelist();
	for(unsigned int index = 0; index < mRender->getNumViewports(); index++)
		setViewport(mRender->getViewport(index));
}

void LLOgrePostprocess::updatePostprocessNamelist(void)
{
	// Empty old namelist before we start to create a new one
	if(!mPostprocessNameList.empty())
		mPostprocessNameList.clear();

	// Get all postprocess effects that we want to use in ogre compositor and add them to the namelist
	Ogre::ResourceManager::ResourceMapIterator i = Ogre::CompositorManager::getSingleton().getResourceIterator();
	while(i.hasMoreElements())
	{
		LLOgrePostprocessItem* item = new LLOgrePostprocessItem();
		try
		{
			Ogre::ResourcePtr mat = i.getNext();
			const Ogre::String& name = mat->getName();
			
			if(name == "Ogre/Scene")
				continue;

			item->CompositorItem.CompositorName = name;
			item->CompositorItem.ID.generate();
			item->EffectEnabled = false;
         item->EnabledBy = LLOgrePostprocessItem::ENABLER_UNKNOWN;

			mPostprocessNameList.push_back(name);
			mCompostiorList.push_back(*item);
		}
		catch(...)
		{
			delete item;
			throw;
		}
		delete item;
	}
}

/*bool LLOgrePostprocess::createMaterialParameterList()
{
	Ogre::ConfigFile file;
	
	Ogre::StringVectorPtr fileStringVector = Ogre::ResourceGroupManager::getSingleton().findResourceNames( Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, "*.controls" );
	Ogre::String secName, materialName;
	std::vector<Ogre::String>::iterator FileNameIte;
	for(FileNameIte = fileStringVector->begin(); FileNameIte < fileStringVector->end(); FileNameIte++)
	{
		try
		{
			file.load(*FileNameIte, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, "\t;=", true);

			Ogre::ConfigFile::SectionIterator secIte = file.getSectionIterator();
			while(secIte.hasMoreElements())
			{
				secName = secIte.peekNextKey();
				Ogre::LogManager::getSingleton().logMessage("SecName:" + secName);
				Ogre::ConfigFile::SettingsMultiMap* setMap = secIte.getNext();
				if(!secName.empty() && setMap)
				{
					materialName = file.getSetting("material", secName);
					Ogre::LogManager::getSingleton().logMessage("Material:" + materialName);
					Ogre::ConfigFile::SettingsMultiMap::iterator setMapIte = setMap->begin();
					
					RexOgreLegacyMaterialControl* material_control = new RexOgreLegacyMaterialControl(materialName);
					while(setMapIte != setMap->end())
					{
						Ogre::String typeName = setMapIte->first; 
						Ogre::String dataString = setMapIte->second;
						if(typeName == "control")
						{
							Ogre::StringVector controlItem = Ogre::StringUtil::split(dataString, ",");
							if(controlItem.size() != 6)
							{
								Ogre::LogManager::getSingleton().logMessage("Error " + typeName + " need to have 6 parameters!");						
								continue;
							}
							
							
							LLOgreShaderValue shaderValue;

							shaderValue.DisplayName = controlItem[0];
							shaderValue.ParamName = controlItem[1];

							if (controlItem[2] == "GPU_VERTEX")
								shaderValue.ShaderValueType = GPU_VERTEX;
							else if (controlItem[2] == "GPU_FRAGMENT")
								shaderValue.ShaderValueType = GPU_FRAGMENT;

							shaderValue.MinValue = Ogre::StringConverter::parseReal(controlItem[3]);
							shaderValue.MaxValue = Ogre::StringConverter::parseReal(controlItem[4]);
							shaderValue.valueIndex = Ogre::StringConverter::parseInt(controlItem[5]);
							Ogre::LogManager::getSingleton().logMessage(shaderValue.ParamName);
							material_control->setMaterialValue(shaderValue);
						}
						setMapIte++;
					}
					mMaterialControlList->push_back(*material_control);
					delete material_control; 
					material_control = 0;
				}

			}
		}
		catch(Ogre::Exception e)
		{
			Ogre::LogManager::getSingleton().logMessage(e.getFullDescription());
		}
	}
	return true;
}*/

/*LLOgrePostprocessItem LLOgrePostprocess::getCompositorItem(const std::string name)
{
	std::vector<LLOgrePostprocessItem>::iterator ite;
	LLOgrePostprocessItem mTemp;
	// Search right compositor by it's name
	for(ite = mCompostiorList.begin(); ite!= mCompostiorList.end(); ite++)
	{
		if(ite->CompositorItem.CompositorName == name)
			mTemp = *ite;
	}
	return mTemp;
}*/

LLUUID* LLOgrePostprocess::getEffectUUID(const std::string name)
{
	std::vector<LLOgrePostprocessItem>::iterator ite = mCompostiorList.begin();
	while(ite != mCompostiorList.end())
	{
		if(ite->CompositorItem.CompositorName == name)
			return &ite->CompositorItem.ID;
		ite++;
	}
	return NULL;
}

BOOL LLOgrePostprocess::isPostprocessEnabled(Ogre::Viewport* viewport, const LLUUID id)
{
	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.find(viewport);
	if(ite != mVpPostprocessList.end())
	{
		std::vector<LLOgrePostprocessItem>::iterator ite2;
		for(ite2 = ite->second.begin(); ite2 < ite->second.end(); ite2++)
		{
			if(ite2->CompositorItem.ID == id)			
				return ite2->EffectEnabled;
		}
	}
	else
		Ogre::LogManager::getSingleton().logMessage("Error cant find the viewport");

	return false;
}

std::string* LLOgrePostprocess::getEffectName(const LLUUID id) 
{
	std::vector<LLOgrePostprocessItem>::iterator ite;
	for(ite = mCompostiorList.begin(); ite < mCompostiorList.end(); ite++)
	{
		if(ite->CompositorItem.ID == id)
			return &ite->CompositorItem.CompositorName;
	}
	return NULL;
}

void LLOgrePostprocess::setViewport(Ogre::Viewport* viewport)
{
	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.find(viewport);
	// try to find if viewport already exists on map list
	if(ite == mVpPostprocessList.end())
	{
		std::vector<LLOgrePostprocessItem>::iterator ite2;
		// add all compositor components in new key value and set enabled to false
		for(ite2 = mCompostiorList.begin(); ite2 < mCompostiorList.end(); ite2++)
		{
			mVpPostprocessList[viewport].push_back(*ite2);
			mVpPostprocessList[viewport].back().EffectEnabled = false;
         mVpPostprocessList[viewport].back().EnabledBy = LLOgrePostprocessItem::ENABLER_UNKNOWN;
		}
	}
	else
		Ogre::LogManager::getSingleton().logMessage("Error viewport already exits: LLOgrePostprocess.cpp/setViewport");
}

void LLOgrePostprocess::removeViewport(Ogre::Viewport* viewport)
{
	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.find(viewport);
	if(ite == mVpPostprocessList.end())
	{
		mVpPostprocessList[viewport].clear();
		mVpPostprocessList.erase(ite);
	}
	else
		Ogre::LogManager::getSingleton().logMessage("Error cant find the viewport");
}

void LLOgrePostprocess::setPostprocess(Ogre::Viewport* viewport ,const Ogre::String name, int position, LLOgrePostprocessItem::Enabler enabler)
{
    LLOgreRenderer::getPointer()->setOgreContext();

	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.find(viewport);
	if(ite != mVpPostprocessList.end())
	{
		std::vector<LLOgrePostprocessItem>::iterator ite2;
		for(ite2 = ite->second.begin(); ite2 < ite->second.end(); ite2++)
		{
			if(ite2->CompositorItem.CompositorName == name)
         {
            if (ite2->EffectEnabled == true) // ignore if effect already enabled
            {
               LLOgreRenderer::getPointer()->setSLContext();
               return;
            }

				ite2->EffectEnabled = true;
            if (ite2->EnabledBy != LLOgrePostprocessItem::ENABLER_CLIENT) // Update enabler unless it was the client
               ite2->EnabledBy = enabler;
         }
		}
	}

	// HDR need to be the 1st on the compositorchain
	// cant use two HDR's in the same time 
	if(name == "HDR" || name == "HC_HDR")
		position = 0;
		
	Ogre::CompositorInstance* instance = Ogre::CompositorManager::getSingleton().addCompositor(viewport, name, position);
	Ogre::CompositorManager::getSingleton().setCompositorEnabled(viewport, name, true);

	// We need to add listener for hdr or it wont work right
	if(instance && name == "HDR" || instance && name == "HC_HDR")
	{
		hdrListener = new HDRListener();
		instance->addListener(hdrListener);
		hdrListener->notifyViewportSize(viewport->getActualWidth(), viewport->getActualHeight());
		hdrListener->notifyCompositor(instance);
	}

    LLOgreRenderer::getPointer()->setSLContext();
}

void LLOgrePostprocess::setPostprocess(Ogre::Viewport* viewport, unsigned int index, int position, LLOgrePostprocessItem::Enabler enabler)
{
   if (index < mPostprocessNameList.size())
   {
      std::vector<std::string>::const_iterator iter = mPostprocessNameList.begin();
      std::advance(iter, index);
      setPostprocess(viewport, *iter, position, enabler);
   } // else index out of bounds
}

void LLOgrePostprocess::enablePostprocess(Ogre::Viewport* viewport ,const Ogre::String name, bool enable)
{
    LLOgreRenderer::getPointer()->setOgreContext();

   std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.find(viewport);
	if(ite != mVpPostprocessList.end())
	{
		std::vector<LLOgrePostprocessItem>::iterator ite2;
		for(ite2 = ite->second.begin(); ite2 < ite->second.end(); ite2++)
		{
			if(ite2->CompositorItem.CompositorName == name)
				ite2->EffectEnabled = true;
		}
	}

	Ogre::CompositorManager::getSingleton().setCompositorEnabled(viewport, name, enable);

    LLOgreRenderer::getPointer()->setSLContext();
}

void LLOgrePostprocess::removePostprocess(Ogre::Viewport* viewport ,const Ogre::String name)
{
    LLOgreRenderer::getPointer()->setOgreContext();

	unsigned int index = 0;
	
	std::vector<LLOgrePostprocessItem>::iterator ite;
	for(ite = mCompostiorList.begin(); ite < mCompostiorList.end(); ite++)
	{
		if(ite->CompositorItem.CompositorName == name)
      {
			mVpPostprocessList[viewport].at(index).EffectEnabled = false;
         mVpPostprocessList[viewport].at(index).EnabledBy = LLOgrePostprocessItem::ENABLER_UNKNOWN;
      }
		index++;
	}

	Ogre::CompositorManager::getSingleton().removeCompositor(viewport, name);

    LLOgreRenderer::getPointer()->setSLContext();
}

void LLOgrePostprocess::removePostprocess(Ogre::Viewport* viewport, unsigned int index)
{
   if (index < mPostprocessNameList.size())
   {
      std::vector<std::string>::const_iterator iter = mPostprocessNameList.begin();
      std::advance(iter, index);
      removePostprocess(viewport, *iter);
   } // else index out of bounds
}

void LLOgrePostprocess::removeAllPostprocess(void)
{
    LLOgreRenderer::getPointer()->setOgreContext();

	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.begin();;
	while(ite != mVpPostprocessList.end())
	{
		std::vector<LLOgrePostprocessItem>::iterator ite2 = ite->second.begin();
		while(ite2 != ite->second.end())
		{
			Ogre::CompositorManager::getSingleton().removeCompositor(ite->first, ite2->CompositorItem.CompositorName);
			ite2->EffectEnabled = false;
         ite2->EnabledBy = LLOgrePostprocessItem::ENABLER_UNKNOWN;
			ite2++;
		}
		ite++;
	}

    LLOgreRenderer::getPointer()->setSLContext();
}

void LLOgrePostprocess::removeAllServerSetPostprocess(void)
{
   LLOgreRenderer::getPointer()->setOgreContext();

	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.begin();;
	while(ite != mVpPostprocessList.end())
	{
		std::vector<LLOgrePostprocessItem>::iterator ite2 = ite->second.begin();
		while(ite2 != ite->second.end())
		{
         if (ite2->EnabledBy == LLOgrePostprocessItem::ENABLER_SERVER)
         {
            Ogre::CompositorManager::getSingleton().setCompositorEnabled(ite->first, ite2->CompositorItem.CompositorName, false);
	   		ite2->EffectEnabled = false;
            ite2->EnabledBy = LLOgrePostprocessItem::ENABLER_UNKNOWN;
         }
			ite2++;
		}
		ite++;
	}

   LLOgreRenderer::getPointer()->setSLContext();
}

void LLOgrePostprocess::setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const float value)
{	
	if(!_findMaterialParameters(materialName, paramName) || mActiveParameters.isNull())
		return;

	mActiveParameters->setNamedConstant(paramName, value);
	mActiveParameters.setNull();
}

void LLOgrePostprocess::setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const Ogre::Vector3 value)
{	
	if(!_findMaterialParameters(materialName, paramName) || mActiveParameters.isNull())
		return;

	mActiveParameters->setNamedConstant(paramName, value);
	mActiveParameters.setNull();
}

void LLOgrePostprocess::setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const Ogre::Vector4 value)
{	
	if(!_findMaterialParameters(materialName, paramName) || mActiveParameters.isNull())
		return;

	mActiveParameters->setNamedConstant(paramName, value);
	mActiveParameters.setNull();
}

void LLOgrePostprocess::setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const int value)
{	
	if(!_findMaterialParameters(materialName, paramName) || mActiveParameters.isNull())
		return;
	
	if(!mActiveParameters->_findNamedConstantDefinition(paramName)->isFloat())
		mActiveParameters->setNamedConstant(paramName, value);
	mActiveParameters.setNull();
}

void LLOgrePostprocess::setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const Ogre::ColourValue value)
{	
	if(!_findMaterialParameters(materialName, paramName) || mActiveParameters.isNull())
		return;

	mActiveParameters->setNamedConstant(paramName, value);
	mActiveParameters.setNull();
}

const BOOL LLOgrePostprocess::_findMaterialParameters(const Ogre::String materialName, const Ogre::String paramName)
{
	Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(materialName);
	if(mat.isNull())
		return false;
	
	// we need to load material or ogre wont know what technique our hardware will support
	mat->load();
	Ogre::Technique::PassIterator passIte = mat->getSupportedTechnique(0)->getPassIterator();
	while(passIte.hasMoreElements())
	{
		Ogre::Pass* pass = passIte.getNext();
		// check that does pass has vertex or fragment programs
		if(pass->isProgrammable())
		{
			Ogre::GpuProgramParametersSharedPtr vpParam = pass->getVertexProgramParameters();
			if(vpParam->hasNamedParameters())
			{
				if(vpParam->_findNamedConstantDefinition(paramName))
				{
					mActiveParameters = vpParam;
					return true;
				}
			}
			Ogre::GpuProgramParametersSharedPtr fpParam = pass->getFragmentProgramParameters();
			if(fpParam->hasNamedParameters())
			{
				if(fpParam->_findNamedConstantDefinition(paramName))
				{
					mActiveParameters = fpParam;
					return true;
				}
			}
		}
	}
	return false;
}

void LLOgrePostprocess::notifyMaterialChangeToCompositorChain(Ogre::Viewport* viewport)
{
	// if postprocess effect have been added in your compositorChain before material have been changed, you need to
	// tell compositorChain that changes has been made, because compositorChain only holds copy of that material and so changes wont
	// take place until those materials has been updated.
	Ogre::CompositorManager::getSingleton().getCompositorChain(viewport)->_markDirty();
}

void LLOgrePostprocess::notifyAllViewportsForMaterialChange()
{
	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.begin();
	while(ite != mVpPostprocessList.end())
	{
		Ogre::CompositorManager::getSingleton().getCompositorChain(ite->first)->_markDirty();
		ite++;
	}
}

void LLOgrePostprocess::restoreOldSettings(Ogre::Viewport* viewport)
{
	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>>::iterator ite = mVpPostprocessList.find(viewport);
	if(ite != mVpPostprocessList.end())
	{
		std::vector<LLOgrePostprocessItem>::iterator ite2;
		for(ite2 = ite->second.begin(); ite2 < ite->second.end(); ite2++)
		{
			if(ite2->EffectEnabled)
         {
            ite2->EffectEnabled = false;
				this->setPostprocess(viewport, ite2->CompositorItem.CompositorName,-1, ite2->EnabledBy);
         }
		}
	}
}

/*void LLOgrePostprocess::notifyValueChange(const LLUUID id, const Ogre::String valueName, Ogre::Real value)
{
	Ogre::String effectName = getEffectName(id);

	LLOgrePostprocessItem item = getCompositorItem(effectName);
	Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(item.CompositorItem.MaterialList.back());

	if(effectName == "")
			return;
	else if(effectName == "NewB&W")
	{
		Ogre::ResourceManager::ResourceMapIterator ite = Ogre::CompositorManager::getSingleton().getResourceIterator();

		Ogre::GpuProgramParametersSharedPtr fpParams = mat->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
		
		fpParams->setNamedConstant(valueName, Ogre::Vector3(value, value, value));

		// if we can find any param_named variables in ogre .material we will make a namelist of them for future use
		// Please note! if your value in material wont be used in shader it wont be shown in this list.*/
		/*if(fpParams->hasNamedParameters())
		{
			Ogre::GpuNamedConstants constants = fpParams->getConstantDefinitions();
			// In ogre source: typedef std::map<String, GpuConstantDefinition> GpuConstantDefinitionMap
			Ogre::GpuConstantDefinitionMap::iterator ite = constants.map.begin();
			Ogre::String lastString;
			while(ite != constants.map.end())
			{
				size_t erasedValue = ite->first.length() - 3;
				if(ite->first.substr(0, erasedValue) != lastString)
				{
					Ogre::LogManager::getSingleton().logMessage(ite->first);
				}
					
				lastString = ite->first;
				ite++;
			}
		}*/
		// effect variable change wont take a place if we wont do this in chain
		/*Ogre::CompositorManager::getSingleton().getCompositorChain(LLOgreRenderer::getPointer()->getCamera()->getViewport())->_markDirty(); 
	}
	else if(effectName == "Laplace")
	{
		Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName("Ogre/Compositor/Laplace");
		Ogre::GpuProgramParametersSharedPtr fpParams = mat->getTechnique(0)->getPass(0)->getFragmentProgramParameters();

		// if we can find any param_named variables in ogre .material we will make a namelist of them for future use
		if(fpParams->hasNamedParameters())
		{
			Ogre::GpuNamedConstants constants = fpParams->getConstantDefinitions();
			// In ogre source: typedef std::map<String, GpuConstantDefinition> GpuConstantDefinitionMap
			Ogre::GpuConstantDefinitionMap::iterator ite = constants.map.begin();
			Ogre::String lastString;
			while(ite != constants.map.end())
			{
				size_t erasedValue = ite->first.length() - 3;
				if(ite->first.substr(0, erasedValue) != lastString)
				{
					Ogre::LogManager::getSingleton().logMessage(ite->first);
				}
					
				lastString = ite->first;
				ite++;
			}
		}
	}
}*/

//---------------------------
//HDRListener Methods
//---------------------------
	HDRListener::HDRListener()
	{
	}

	HDRListener::~HDRListener()
	{
	}

	void HDRListener::notifyViewportSize(int width, int height)
	{
		mVpWidth = width;
		mVpHeight = height;
	}

	void HDRListener::notifyCompositor(Ogre::CompositorInstance* instance)
	{
		// Get some RTT dimensions for later calculations
		Ogre::CompositionTechnique::TextureDefinitionIterator defIter =
			instance->getTechnique()->getTextureDefinitionIterator();
		while (defIter.hasMoreElements())
		{
			Ogre::CompositionTechnique::TextureDefinition* def =
				defIter.getNext();
			if(def->name == "rt_bloom0")
			{
				mBloomSize = (int)def->width; // should be square
				// Calculate gaussian texture offsets & weights
				float deviation = 3.0f;
				float texelSize = 1.0f / (float)mBloomSize;

				// central sample, no offset
				mBloomTexOffsetsHorz[0][0] = 0.0f;
				mBloomTexOffsetsHorz[0][1] = 0.0f;
				mBloomTexOffsetsVert[0][0] = 0.0f;
				mBloomTexOffsetsVert[0][1] = 0.0f;
				mBloomTexWeights[0][0] = mBloomTexWeights[0][1] =
					mBloomTexWeights[0][2] = Ogre::Math::gaussianDistribution(0, 0, deviation);
				mBloomTexWeights[0][3] = 1.0f;

				// 'pre' samples
				for(int i = 1; i < 8; ++i)
				{
					mBloomTexWeights[i][0] = mBloomTexWeights[i][1] =
						mBloomTexWeights[i][2] = 1.25f * Ogre::Math::gaussianDistribution(i, 0, deviation);
					mBloomTexWeights[i][3] = 1.0f;
					mBloomTexOffsetsHorz[i][0] = i * texelSize;
					mBloomTexOffsetsHorz[i][1] = 0.0f;
					mBloomTexOffsetsVert[i][0] = 0.0f;
					mBloomTexOffsetsVert[i][1] = i * texelSize;
				}
				// 'post' samples
				for(int i = 8; i < 15; ++i)
				{
					mBloomTexWeights[i][0] = mBloomTexWeights[i][1] =
						mBloomTexWeights[i][2] = mBloomTexWeights[i - 7][0];
					mBloomTexWeights[i][3] = 1.0f;

					mBloomTexOffsetsHorz[i][0] = -mBloomTexOffsetsHorz[i - 7][0];
					mBloomTexOffsetsHorz[i][1] = 0.0f;
					mBloomTexOffsetsVert[i][0] = 0.0f;
					mBloomTexOffsetsVert[i][1] = -mBloomTexOffsetsVert[i - 7][1];
				}

			}
		}
	}
	
	void HDRListener::notifyMaterialSetup(Ogre::uint32 pass_id, Ogre::MaterialPtr &mat)
	{
		// Prepare the fragment params offsets
		switch(pass_id)
		{
		//case 994: // rt_lum4
		case 993: // rt_lum3
		case 992: // rt_lum2
		case 991: // rt_lum1
		case 990: // rt_lum0
			break;
		case 800: // rt_brightpass
			break;
		case 701: // rt_bloom1
			{
				// horizontal bloom
				mat->load();
				Ogre::GpuProgramParametersSharedPtr fparams =
					mat->getBestTechnique()->getPass(0)->getFragmentProgramParameters();
				//const Ogre::String& progName = mat->getBestTechnique()->getPass(0)->getFragmentProgramName();
				fparams->setNamedConstant("sampleOffsets", mBloomTexOffsetsHorz[0], 15);
				fparams->setNamedConstant("sampleWeights", mBloomTexWeights[0], 15);

				break;
			}
		case 700: // rt_bloom0
			{
				// vertical bloom
				mat->load();
				Ogre::GpuProgramParametersSharedPtr fparams =
					mat->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
				//const Ogre::String& progName = mat->getBestTechnique()->getPass(0)->getFragmentProgramName();
				fparams->setNamedConstant("sampleOffsets", mBloomTexOffsetsVert[0], 15);
				fparams->setNamedConstant("sampleWeights", mBloomTexWeights[0], 15);

				break;
			}
		}
	}
	
	void HDRListener::notifyMaterialRender(Ogre::uint32 pass_id, Ogre::MaterialPtr &mat)
	{
	}
//---------------------------
//HDRListener Methods end
//---------------------------