/** 
 * @file llogrefont.cpp
 * @brief LLOgreFont class header file
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#ifndef LL_LLOGREFONT_H					
#define LL_LLOGREFONT_H

#include "OGRE/Ogre.h"

class LLOgreFont
{
public:
   //! How to justify text
   enum TextJustify
   {
      Left = 0,
      Center,
      Right
   };

private:
   //! constructor
   LLOgreFont() {};

public:
   //! Creates a texture of specified size and writes text to it
   /*! If texture with specified name exists, returns 0

       \param name Texture name (should be unique)
       \param text Text to write
       \param fontName Font to use
       \param width Texture width
       \param height Texture height
       \return A texture with specified text written in it, of empty texture ptr if texture already exists
   */  
   static Ogre::TexturePtr createTextureFromString(const std::string &name, const std::string &text, const std::string &fontName, U32 width, U32 height);

   //! Writes a string to texture
   /*!
       \param texture texture to write text to
       \param text string to write to texture
       \param font font to use
       \param rect Rectangle in texture where to draw the text to
       \param colour text colour
       \param justify text justify
       \param wordwrap use wordwrap
   */
   static bool writeToTexture(Ogre::TexturePtr &texture, const std::string &text, Ogre::Font *font,
      const Ogre::Image::Box &destRect, const Ogre::ColourValue &colour = Ogre::ColourValue(0, 0, 0), TextJustify justify = Left,  bool wordwrap = true);
};

#endif // LL_LLOGREFONT_H
