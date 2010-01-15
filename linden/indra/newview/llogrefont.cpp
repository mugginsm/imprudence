/** 
 * @file llogrefont.cpp
 * @brief LLOgreFont class implementation
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#include "llviewerprecompiledheaders.h"
#include "llogrefont.h"
#include "OGRE/OgreFont.h"
#include "OGRE/OgreFontManager.h"
#include "llogre.h"
#include "llappviewer.h"




Ogre::TexturePtr LLOgreFont::createTextureFromString(const std::string &name, const std::string &text, const std::string &fontName, U32 width, U32 height)
{
   Ogre::TexturePtr texture;
   Ogre::ResourcePtr oldtexture = Ogre::TextureManager::getSingleton().getByName(name);
   if (oldtexture.isNull() == false)
   {
      return texture;
   }
   LLOgreRenderer::getPointer()->setOgreContext();

   Ogre::Font *font = 0;
   Ogre::ResourcePtr fontResource = Ogre::FontManager::getSingleton().getByName(fontName);
   if (fontResource.isNull() == false)
      font = static_cast<Ogre::Font*>(fontResource.getPointer());
   else
      llwarns << "Font " << fontName << " not found. Using fallback font." << llendl;

   texture = Ogre::TextureManager::getSingleton().createManual(
         name,
         Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
         Ogre::TEX_TYPE_2D,
         width, height,
         Ogre::MIP_UNLIMITED,
         Ogre::PF_A8R8G8B8,
         Ogre::TU_STATIC | Ogre::TU_AUTOMIPMAP);

   // Clear the texture. It seems we have to do this, otherwise artifacts are present on the texture.
   Ogre::HardwarePixelBufferSharedPtr buffer = texture->getBuffer();
   U8 *data = static_cast<U8*>(buffer->lock(Ogre::HardwareBuffer::HBL_NORMAL));
   size_t n;
   for (n=0 ; n<buffer->getSizeInBytes() ; ++n)
      data[n] = 0;
   buffer->unlock();

   writeToTexture(texture, text, font, Ogre::Image::Box(0, 0, width, height));

   LLOgreRenderer::getPointer()->setSLContext();
   return texture;
}

bool LLOgreFont::writeToTexture(Ogre::TexturePtr &texture, const std::string &text, Ogre::Font *font,
                                const Ogre::Image::Box &destRect, const Ogre::ColourValue &colour,
                                TextJustify justify,  bool wordwrap)
{
   Ogre::Image::Box rect = destRect;
   if (rect.getHeight() > texture->getHeight() || rect.getHeight() == 0)
      rect.bottom = texture->getHeight();

   if (rect.getWidth() > texture->getWidth() || rect.getWidth() == 0)
      rect.right = texture->getWidth();

   Ogre::ResourcePtr fontResource = Ogre::FontManager::getSingleton().getByName("BlueHighway");
   if (fontResource.isNull())
   {
      llwarns << "Font not found: BlueHighway" << llendl;
      return false;
   }

   font = static_cast<Ogre::Font*>(fontResource.getPointer());

   U32 width = rect.getWidth();
   U32 height = rect.getHeight();

   Ogre::TexturePtr fontTexture = (Ogre::TexturePtr)Ogre::TextureManager::getSingleton().getByName(font->getMaterial()->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getTextureName());
   
   Ogre::HardwarePixelBufferSharedPtr fontBuffer = fontTexture->getBuffer();
   Ogre::HardwarePixelBufferSharedPtr destBuffer = texture->getBuffer();

   Ogre::PixelBox destPb = destBuffer->lock(rect, Ogre::HardwareBuffer::HBL_NORMAL);

   // The font texture buffer was created write only...so we cannot read it back :o). One solution is to copy the buffer  instead of locking it. (Maybe there is a way to create a font texture which is not write_only ?)
        
   // create a buffer
   size_t fontBufferSize = fontBuffer->getSizeInBytes();
   
   Ogre::uint8* buffer = (Ogre::uint8*)new Ogre::uint8[fontBufferSize];
   // create pixel box using the copy of the buffer
   Ogre::PixelBox fontPb(fontBuffer->getWidth(), fontBuffer->getHeight(), fontBuffer->getDepth(), fontBuffer->getFormat(), buffer);          
   fontBuffer->blitToMemory(fontPb);
   
   Ogre::uint8* fontData = static_cast<Ogre::uint8*>( fontPb.data );
   //fontBuffer->lock(Ogre::HardwareBuffer::HBL_NORMAL);
   //Ogre::PixelBox &fontBox = static_cast<Ogre::PixelBox>(fontBuffer->getCurrentLock());
   //Ogre::uint8* fontData = static_cast<Ogre::uint8*>( fontBox.data );

   Ogre::uint8* destData = static_cast<Ogre::uint8*>( destPb.data );

   const size_t fontPixelSize = Ogre::PixelUtil::getNumElemBytes(fontPb.format);
   const size_t destPixelSize = Ogre::PixelUtil::getNumElemBytes(destPb.format);

   const size_t fontRowPitchBytes = fontPb.rowPitch * fontPixelSize;
	const size_t destRowPitchBytes = destPb.rowPitch * destPixelSize;

   Ogre::Box *GlyphTexCoords;
   GlyphTexCoords = new Ogre::Box[text.size()];

   Ogre::Font::UVRect glypheTexRect;
	size_t charheight = 0;
	size_t charwidth = 0;
   
   U32 i;
   for(i = 0; i < text.size(); i++)
	{
		if ((text[i] != '\t') && (text[i] != '\n') && (text[i] != ' '))
		{
			glypheTexRect = font->getGlyphTexCoords(text[i]);
			GlyphTexCoords[i].left = glypheTexRect.left * fontTexture->getSrcWidth();
			GlyphTexCoords[i].top = glypheTexRect.top * fontTexture->getSrcHeight();
			GlyphTexCoords[i].right = glypheTexRect.right * fontTexture->getSrcWidth();
			GlyphTexCoords[i].bottom = glypheTexRect.bottom * fontTexture->getSrcHeight();

			if (GlyphTexCoords[i].getHeight() > charheight)
				charheight = GlyphTexCoords[i].getHeight();
			if (GlyphTexCoords[i].getWidth() > charwidth)
				charwidth = GlyphTexCoords[i].getWidth();
		}
	}

   size_t cursorX = 0;
	size_t cursorY = 0;
//	size_t lineend = rect.getWidth();
   size_t lineend = width;
	bool carriagreturn = true;
   bool textureend = false;
   U32 strindex;
	for (strindex = 0; strindex < text.size() && !textureend; ++strindex)
	{
		switch(text[strindex])
		{
		case ' ': cursorX += charwidth;  break;
		case '\t':cursorX += charwidth * 3; break;
		case '\n':cursorY += charheight; carriagreturn = true; break;
		default:
			{
				//wrapping
				if ((cursorX + GlyphTexCoords[strindex].getWidth()> lineend) && !carriagreturn )
				{
					cursorY += charheight;
					carriagreturn = true;
				}
				
				//justify
				if (carriagreturn)
				{
					size_t l = strindex;
					size_t textwidth = 0;	
					size_t wordwidth = 0;

					while( (l < text.size() ) && (text[l] != '\n)'))
					{		
						wordwidth = 0;

						switch (text[l])
						{
						case ' ': wordwidth = charwidth; ++l; break;
						case '\t': wordwidth = charwidth *3; ++l; break;
						case '\n': l = text.size();
						}
						
						if (wordwrap)
							while((l < text.size()) && (text[l] != ' ') && (text[l] != '\t') && (text[l] != '\n'))
							{
								wordwidth += GlyphTexCoords[l].getWidth();
								++l;
							}
						else
							{
								wordwidth += GlyphTexCoords[l].getWidth();
								l++;
							}
	
						if ((textwidth + wordwidth) <= width)
							textwidth += (wordwidth);
						else
							break;
					}

					if ((textwidth == 0) && (wordwidth > width))
						textwidth = width;

					switch (justify)
					{
					case Center:	
                     cursorX = (width - textwidth)/2;
							lineend = width - cursorX;
							break;

					case Right:
                     cursorX = (width - textwidth);
							lineend = width;
							break;

					default:	
                     cursorX = 0;
							lineend = textwidth;
							break;
					}

					carriagreturn = false;
				}

				//abort - net enough space to draw
				if ((cursorY + charheight) > height)
            {
               textureend = true;
               break;
            }

				//draw pixel by pixel
            size_t i, j;
				for (i = 0; i <= GlyphTexCoords[strindex].getHeight(); ++i)
            {
					for (j = 0; j <= GlyphTexCoords[strindex].getWidth(); ++j)
					{
 						float alpha =  colour.a * (fontData[(i + GlyphTexCoords[strindex].top) * fontRowPitchBytes + (j + GlyphTexCoords[strindex].left) * fontPixelSize +1 ] / 255.0);
 						float invalpha = 1.0 - alpha;
 						size_t offset = (i + cursorY) * destRowPitchBytes + (j + cursorX) * destPixelSize;
                  Ogre::ColourValue pix = Ogre::ColourValue(1, 1, 1, 1);
                  Ogre::PixelUtil::unpackColour(&pix, destPb.format, &destData[offset]);
 						pix = (pix * invalpha) + (colour * alpha);
                  Ogre::PixelUtil::packColour(pix, destPb.format, &destData[offset]);
  					}
            }
 
				cursorX += GlyphTexCoords[strindex].getWidth();
			} // default
		} // switch
	} // for

	delete[] GlyphTexCoords;

	destBuffer->unlock();
//   fontBuffer->unlock();
	
   delete[] buffer;

   return true;
}
