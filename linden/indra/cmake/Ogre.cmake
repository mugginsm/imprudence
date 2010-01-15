# -*- cmake -*-
include(Prebuilt)


if (WINDOWS)
    set(OGRE_LIBRARIES
	OgreMain
	OgreGUIRenderer
	)
endif (WINDOWS)
set(OGRE_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/ogre)
