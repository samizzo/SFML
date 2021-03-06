#
# Try to find Ogg/Vorbis libraries and include paths.
# Once done this will define
#
# VORBIS_FOUND
# VORBIS_INCLUDE_DIRS
# VORBIS_LIBRARIES
#

find_path(OGG_INCLUDE_DIR ogg/ogg.h)
find_path(VORBIS_INCLUDE_DIR vorbis/vorbisfile.h)

find_library(OGG_LIBRARY NAMES ogg)
find_library(VORBIS_LIBRARY NAMES vorbis)
find_library(VORBISFILE_LIBRARY NAMES vorbisfile)

if(SFML_ENABLE_VORBIS_ENCODING)
    find_library(VORBISENC_LIBRARY NAMES vorbisenc)
else()
    unset(VORBISENC_LIBRARY CACHE)
endif()

include(FindPackageHandleStandardArgs)
if(SFML_ENABLE_VORBIS_ENCODING)
    find_package_handle_standard_args(VORBIS DEFAULT_MSG VORBIS_LIBRARY VORBISFILE_LIBRARY VORBISENC_LIBRARY OGG_LIBRARY VORBIS_INCLUDE_DIR OGG_INCLUDE_DIR)
else()
    find_package_handle_standard_args(VORBIS DEFAULT_MSG VORBIS_LIBRARY VORBISFILE_LIBRARY OGG_LIBRARY VORBIS_INCLUDE_DIR OGG_INCLUDE_DIR)
endif()

set(VORBIS_INCLUDE_DIRS ${OGG_INCLUDE_DIR} ${VORBIS_INCLUDE_DIR})
set(VORBIS_LIBRARIES ${VORBISENC_LIBRARY} ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})

mark_as_advanced(OGG_INCLUDE_DIR VORBIS_INCLUDE_DIR OGG_LIBRARY VORBIS_LIBRARY VORBISFILE_LIBRARY VORBISENC_LIBRARY)
